// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/origin_bound_cert_service.h"

#include <algorithm>
#include <limits>

#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/threading/worker_pool.h"
#include "crypto/ec_private_key.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_errors.h"
#include "net/base/origin_bound_cert_store.h"
#include "net/base/x509_certificate.h"
#include "net/base/x509_util.h"

#if defined(USE_NSS)
#include <private/pprthred.h>  // PR_DetachThread
#endif

namespace net {

namespace {

const int kKeySizeInBits = 1024;
const int kValidityPeriodInDays = 365;

bool IsSupportedCertType(uint8 type) {
  switch(type) {
    case CLIENT_CERT_RSA_SIGN:
    case CLIENT_CERT_ECDSA_SIGN:
      return true;
    default:
      return false;
  }
}

}  // namespace

// Represents the output and result callback of a request.
class OriginBoundCertServiceRequest {
 public:
  OriginBoundCertServiceRequest(const CompletionCallback& callback,
                                SSLClientCertType* type,
                                std::string* private_key,
                                std::string* cert)
      : callback_(callback),
        type_(type),
        private_key_(private_key),
        cert_(cert) {
  }

  // Ensures that the result callback will never be made.
  void Cancel() {
    callback_.Reset();
    type_ = NULL;
    private_key_ = NULL;
    cert_ = NULL;
  }

  // Copies the contents of |private_key| and |cert| to the caller's output
  // arguments and calls the callback.
  void Post(int error,
            SSLClientCertType type,
            const std::string& private_key,
            const std::string& cert) {
    if (!callback_.is_null()) {
      *type_ = type;
      *private_key_ = private_key;
      *cert_ = cert;
      callback_.Run(error);
    }
    delete this;
  }

  bool canceled() const { return callback_.is_null(); }

 private:
  CompletionCallback callback_;
  SSLClientCertType* type_;
  std::string* private_key_;
  std::string* cert_;
};

// OriginBoundCertServiceWorker runs on a worker thread and takes care of the
// blocking process of performing key generation. Deletes itself eventually
// if Start() succeeds.
class OriginBoundCertServiceWorker {
 public:
  OriginBoundCertServiceWorker(
      const std::string& origin,
      SSLClientCertType type,
      OriginBoundCertService* origin_bound_cert_service)
      : origin_(origin),
        type_(type),
        serial_number_(base::RandInt(0, std::numeric_limits<int>::max())),
        origin_loop_(MessageLoop::current()),
        origin_bound_cert_service_(origin_bound_cert_service),
        canceled_(false),
        error_(ERR_FAILED) {
  }

  bool Start() {
    DCHECK_EQ(MessageLoop::current(), origin_loop_);

    return base::WorkerPool::PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &OriginBoundCertServiceWorker::Run),
        true /* task is slow */);
  }

  // Cancel is called from the origin loop when the OriginBoundCertService is
  // getting deleted.
  void Cancel() {
    DCHECK_EQ(MessageLoop::current(), origin_loop_);
    base::AutoLock locked(lock_);
    canceled_ = true;
  }

 private:
  void Run() {
    // Runs on a worker thread.
    error_ = OriginBoundCertService::GenerateCert(origin_,
                                                  type_,
                                                  serial_number_,
                                                  &private_key_,
                                                  &cert_);
#if defined(USE_NSS)
    // Detach the thread from NSPR.
    // Calling NSS functions attaches the thread to NSPR, which stores
    // the NSPR thread ID in thread-specific data.
    // The threads in our thread pool terminate after we have called
    // PR_Cleanup. Unless we detach them from NSPR, net_unittests gets
    // segfaults on shutdown when the threads' thread-specific data
    // destructors run.
    PR_DetachThread();
#endif
    Finish();
  }

  // DoReply runs on the origin thread.
  void DoReply() {
    DCHECK_EQ(MessageLoop::current(), origin_loop_);
    {
      // We lock here because the worker thread could still be in Finished,
      // after the PostTask, but before unlocking |lock_|. If we do not lock in
      // this case, we will end up deleting a locked Lock, which can lead to
      // memory leaks or worse errors.
      base::AutoLock locked(lock_);
      if (!canceled_) {
        origin_bound_cert_service_->HandleResult(origin_, error_, type_,
                                                 private_key_, cert_);
      }
    }
    delete this;
  }

  void Finish() {
    // Runs on the worker thread.
    // We assume that the origin loop outlives the OriginBoundCertService. If
    // the OriginBoundCertService is deleted, it will call Cancel on us. If it
    // does so before the Acquire, we'll delete ourselves and return. If it's
    // trying to do so concurrently, then it'll block on the lock and we'll
    // call PostTask while the OriginBoundCertService (and therefore the
    // MessageLoop) is still alive. If it does so after this function, we
    // assume that the MessageLoop will process pending tasks. In which case
    // we'll notice the |canceled_| flag in DoReply.

    bool canceled;
    {
      base::AutoLock locked(lock_);
      canceled = canceled_;
      if (!canceled) {
        origin_loop_->PostTask(
            FROM_HERE,
            NewRunnableMethod(this, &OriginBoundCertServiceWorker::DoReply));
      }
    }
    if (canceled)
      delete this;
  }

  const std::string origin_;
  const SSLClientCertType type_;
  // Note that serial_number_ must be initialized on a non-worker thread
  // (see documentation for OriginBoundCertService::GenerateCert).
  uint32 serial_number_;
  MessageLoop* const origin_loop_;
  OriginBoundCertService* const origin_bound_cert_service_;

  // lock_ protects canceled_.
  base::Lock lock_;

  // If canceled_ is true,
  // * origin_loop_ cannot be accessed by the worker thread,
  // * origin_bound_cert_service_ cannot be accessed by any thread.
  bool canceled_;

  int error_;
  std::string private_key_;
  std::string cert_;

  DISALLOW_COPY_AND_ASSIGN(OriginBoundCertServiceWorker);
};

// An OriginBoundCertServiceJob is a one-to-one counterpart of an
// OriginBoundCertServiceWorker. It lives only on the OriginBoundCertService's
// origin message loop.
class OriginBoundCertServiceJob {
 public:
  OriginBoundCertServiceJob(OriginBoundCertServiceWorker* worker,
                            SSLClientCertType type)
      : worker_(worker), type_(type) {
  }

  ~OriginBoundCertServiceJob() {
    if (worker_) {
      worker_->Cancel();
      DeleteAllCanceled();
    }
  }

  SSLClientCertType type() const { return type_; }

  void AddRequest(OriginBoundCertServiceRequest* request) {
    requests_.push_back(request);
  }

  void HandleResult(int error,
                    SSLClientCertType type,
                    const std::string& private_key,
                    const std::string& cert) {
    worker_ = NULL;
    PostAll(error, type, private_key, cert);
  }

 private:
  void PostAll(int error,
               SSLClientCertType type,
               const std::string& private_key,
               const std::string& cert) {
    std::vector<OriginBoundCertServiceRequest*> requests;
    requests_.swap(requests);

    for (std::vector<OriginBoundCertServiceRequest*>::iterator
         i = requests.begin(); i != requests.end(); i++) {
      (*i)->Post(error, type, private_key, cert);
      // Post() causes the OriginBoundCertServiceRequest to delete itself.
    }
  }

  void DeleteAllCanceled() {
    for (std::vector<OriginBoundCertServiceRequest*>::iterator
         i = requests_.begin(); i != requests_.end(); i++) {
      if ((*i)->canceled()) {
        delete *i;
      } else {
        LOG(DFATAL) << "OriginBoundCertServiceRequest leaked!";
      }
    }
  }

  std::vector<OriginBoundCertServiceRequest*> requests_;
  OriginBoundCertServiceWorker* worker_;
  SSLClientCertType type_;
};

// static
const char OriginBoundCertService::kEPKIPassword[] = "";

OriginBoundCertService::OriginBoundCertService(
    OriginBoundCertStore* origin_bound_cert_store)
    : origin_bound_cert_store_(origin_bound_cert_store),
      requests_(0),
      cert_store_hits_(0),
      inflight_joins_(0) {}

OriginBoundCertService::~OriginBoundCertService() {
  STLDeleteValues(&inflight_);
}

int OriginBoundCertService::GetOriginBoundCert(
    const std::string& origin,
    const std::vector<uint8>& requested_types,
    SSLClientCertType* type,
    std::string* private_key,
    std::string* cert,
    const CompletionCallback& callback,
    RequestHandle* out_req) {
  DCHECK(CalledOnValidThread());

  if (callback.is_null() || !private_key || !cert || origin.empty() ||
      requested_types.empty()) {
    *out_req = NULL;
    return ERR_INVALID_ARGUMENT;
  }

  SSLClientCertType preferred_type = CLIENT_CERT_INVALID_TYPE;
  for (size_t i = 0; i < requested_types.size(); ++i) {
    if (IsSupportedCertType(requested_types[i])) {
      preferred_type = static_cast<SSLClientCertType>(requested_types[i]);
      break;
    }
  }
  if (preferred_type == CLIENT_CERT_INVALID_TYPE) {
    // None of the requested types are supported.
    *out_req = NULL;
    return ERR_CLIENT_AUTH_CERT_TYPE_UNSUPPORTED;
  }

  requests_++;

  // Check if an origin bound cert of an acceptable type already exists for this
  // origin.
  if (origin_bound_cert_store_->GetOriginBoundCert(origin,
                                                   type,
                                                   private_key,
                                                   cert)) {
    if (IsSupportedCertType(*type) &&
        std::find(requested_types.begin(), requested_types.end(), *type) !=
        requested_types.end()) {
      cert_store_hits_++;
      *out_req = NULL;
      return OK;
    }
    DVLOG(1) << "Cert store had cert of wrong type " << *type << " for "
             << origin;
  }

  // |origin_bound_cert_store_| has no cert for this origin. See if an
  // identical request is currently in flight.
  OriginBoundCertServiceJob* job = NULL;
  std::map<std::string, OriginBoundCertServiceJob*>::const_iterator j;
  j = inflight_.find(origin);
  if (j != inflight_.end()) {
    // An identical request is in flight already. We'll just attach our
    // callback.
    job = j->second;
    // Check that the job is for an acceptable type of cert.
    if (std::find(requested_types.begin(), requested_types.end(), job->type())
        == requested_types.end()) {
      DVLOG(1) << "Found inflight job of wrong type " << job->type()
               << " for " << origin;
      *out_req = NULL;
      // If we get here, the server is asking for different types of certs in
      // short succession.  This probably means the server is broken or
      // misconfigured.  Since we only store one type of cert per origin, we
      // are unable to handle this well.  Just return an error and let the first
      // job finish.
      return ERR_ORIGIN_BOUND_CERT_GENERATION_TYPE_MISMATCH;
    }
    inflight_joins_++;
  } else {
    // Need to make a new request.
    OriginBoundCertServiceWorker* worker =
        new OriginBoundCertServiceWorker(origin, preferred_type, this);
    job = new OriginBoundCertServiceJob(worker, preferred_type);
    if (!worker->Start()) {
      delete job;
      delete worker;
      *out_req = NULL;
      // TODO(rkn): Log to the NetLog.
      LOG(ERROR) << "OriginBoundCertServiceWorker couldn't be started.";
      return ERR_INSUFFICIENT_RESOURCES;  // Just a guess.
    }
    inflight_[origin] = job;
  }

  OriginBoundCertServiceRequest* request =
      new OriginBoundCertServiceRequest(callback, type, private_key, cert);
  job->AddRequest(request);
  *out_req = request;
  return ERR_IO_PENDING;
}

// static
int OriginBoundCertService::GenerateCert(const std::string& origin,
                                         SSLClientCertType type,
                                         uint32 serial_number,
                                         std::string* private_key,
                                         std::string* cert) {
  std::string der_cert;
  std::vector<uint8> private_key_info;
  switch (type) {
    case CLIENT_CERT_RSA_SIGN: {
      scoped_ptr<crypto::RSAPrivateKey> key(
          crypto::RSAPrivateKey::Create(kKeySizeInBits));
      if (!key.get()) {
        DLOG(ERROR) << "Unable to create key pair for client";
        return ERR_KEY_GENERATION_FAILED;
      }
      if (!x509_util::CreateOriginBoundCertRSA(
          key.get(),
          origin,
          serial_number,
          base::TimeDelta::FromDays(kValidityPeriodInDays),
          &der_cert)) {
        DLOG(ERROR) << "Unable to create x509 cert for client";
        return ERR_ORIGIN_BOUND_CERT_GENERATION_FAILED;
      }

      if (!key->ExportPrivateKey(&private_key_info)) {
        DLOG(ERROR) << "Unable to export private key";
        return ERR_PRIVATE_KEY_EXPORT_FAILED;
      }
      break;
    }
    case CLIENT_CERT_ECDSA_SIGN: {
      scoped_ptr<crypto::ECPrivateKey> key(crypto::ECPrivateKey::Create());
      if (!key.get()) {
        DLOG(ERROR) << "Unable to create key pair for client";
        return ERR_KEY_GENERATION_FAILED;
      }
      if (!x509_util::CreateOriginBoundCertEC(
          key.get(),
          origin,
          serial_number,
          base::TimeDelta::FromDays(kValidityPeriodInDays),
          &der_cert)) {
        DLOG(ERROR) << "Unable to create x509 cert for client";
        return ERR_ORIGIN_BOUND_CERT_GENERATION_FAILED;
      }

      if (!key->ExportEncryptedPrivateKey(
          kEPKIPassword, 1, &private_key_info)) {
        DLOG(ERROR) << "Unable to export private key";
        return ERR_PRIVATE_KEY_EXPORT_FAILED;
      }
      break;
    }
    default:
      NOTREACHED();
      return ERR_INVALID_ARGUMENT;
  }

  // TODO(rkn): Perhaps ExportPrivateKey should be changed to output a
  // std::string* to prevent this copying.
  std::string key_out(private_key_info.begin(), private_key_info.end());

  private_key->swap(key_out);
  cert->swap(der_cert);
  return OK;
}

void OriginBoundCertService::CancelRequest(RequestHandle req) {
  DCHECK(CalledOnValidThread());
  OriginBoundCertServiceRequest* request =
      reinterpret_cast<OriginBoundCertServiceRequest*>(req);
  request->Cancel();
}

// HandleResult is called by OriginBoundCertServiceWorker on the origin message
// loop. It deletes OriginBoundCertServiceJob.
void OriginBoundCertService::HandleResult(const std::string& origin,
                                          int error,
                                          SSLClientCertType type,
                                          const std::string& private_key,
                                          const std::string& cert) {
  DCHECK(CalledOnValidThread());

  origin_bound_cert_store_->SetOriginBoundCert(origin, type, private_key, cert);

  std::map<std::string, OriginBoundCertServiceJob*>::iterator j;
  j = inflight_.find(origin);
  if (j == inflight_.end()) {
    NOTREACHED();
    return;
  }
  OriginBoundCertServiceJob* job = j->second;
  inflight_.erase(j);

  job->HandleResult(error, type, private_key, cert);
  delete job;
}

int OriginBoundCertService::cert_count() {
  return origin_bound_cert_store_->GetCertCount();
}

}  // namespace net

DISABLE_RUNNABLE_METHOD_REFCOUNT(net::OriginBoundCertServiceWorker);
