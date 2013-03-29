// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/server_bound_cert_service.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "crypto/ec_private_key.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

#if defined(USE_NSS)
#include <private/pprthred.h>  // PR_DetachThread
#endif

namespace net {

namespace {

const int kKeySizeInBits = 1024;
const int kValidityPeriodInDays = 365;
// When we check the system time, we add this many days to the end of the check
// so the result will still hold even after chrome has been running for a
// while.
const int kSystemTimeValidityBufferInDays = 90;

bool IsSupportedCertType(uint8 type) {
  switch(type) {
    case CLIENT_CERT_ECDSA_SIGN:
      return true;
    // If we add any more supported types, CertIsValid will need to be updated
    // to check that the returned type matches one of the requested types.
    default:
      return false;
  }
}

bool CertIsValid(const std::string& domain,
                 SSLClientCertType type,
                 base::Time expiration_time) {
  if (expiration_time < base::Time::Now()) {
    DVLOG(1) << "Cert store had expired cert for " << domain;
    return false;
  } else if (!IsSupportedCertType(type)) {
    DVLOG(1) << "Cert store had cert of wrong type " << type << " for "
             << domain;
    return false;
  }
  return true;
}

// Used by the GetDomainBoundCertResult histogram to record the final
// outcome of each GetDomainBoundCert call.  Do not re-use values.
enum GetCertResult {
  // Synchronously found and returned an existing domain bound cert.
  SYNC_SUCCESS = 0,
  // Retrieved or generated and returned a domain bound cert asynchronously.
  ASYNC_SUCCESS = 1,
  // Retrieval/generation request was cancelled before the cert generation
  // completed.
  ASYNC_CANCELLED = 2,
  // Cert generation failed.
  ASYNC_FAILURE_KEYGEN = 3,
  ASYNC_FAILURE_CREATE_CERT = 4,
  ASYNC_FAILURE_EXPORT_KEY = 5,
  ASYNC_FAILURE_UNKNOWN = 6,
  // GetDomainBoundCert was called with invalid arguments.
  INVALID_ARGUMENT = 7,
  // We don't support any of the cert types the server requested.
  UNSUPPORTED_TYPE = 8,
  // Server asked for a different type of certs while we were generating one.
  TYPE_MISMATCH = 9,
  // Couldn't start a worker to generate a cert.
  WORKER_FAILURE = 10,
  GET_CERT_RESULT_MAX
};

void RecordGetDomainBoundCertResult(GetCertResult result) {
  UMA_HISTOGRAM_ENUMERATION("DomainBoundCerts.GetDomainBoundCertResult", result,
                            GET_CERT_RESULT_MAX);
}

void RecordGetCertTime(base::TimeDelta request_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES("DomainBoundCerts.GetCertTime",
                             request_time,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(5),
                             50);
}

// On success, returns a ServerBoundCert object and sets |*error| to OK.
// Otherwise, returns NULL, and |*error| will be set to a net error code.
// |serial_number| is passed in because base::RandInt cannot be called from an
// unjoined thread, due to relying on a non-leaked LazyInstance
scoped_ptr<ServerBoundCertStore::ServerBoundCert> GenerateCert(
    const std::string& server_identifier,
    SSLClientCertType type,
    uint32 serial_number,
    int* error) {
  scoped_ptr<ServerBoundCertStore::ServerBoundCert> result;

  base::TimeTicks start = base::TimeTicks::Now();
  base::Time not_valid_before = base::Time::Now();
  base::Time not_valid_after =
      not_valid_before + base::TimeDelta::FromDays(kValidityPeriodInDays);
  std::string der_cert;
  std::vector<uint8> private_key_info;
  switch (type) {
    case CLIENT_CERT_ECDSA_SIGN: {
      scoped_ptr<crypto::ECPrivateKey> key(crypto::ECPrivateKey::Create());
      if (!key.get()) {
        DLOG(ERROR) << "Unable to create key pair for client";
        *error = ERR_KEY_GENERATION_FAILED;
        return result.Pass();
      }
      if (!x509_util::CreateDomainBoundCertEC(key.get(), server_identifier,
                                              serial_number, not_valid_before,
                                              not_valid_after, &der_cert)) {
        DLOG(ERROR) << "Unable to create x509 cert for client";
        *error = ERR_ORIGIN_BOUND_CERT_GENERATION_FAILED;
        return result.Pass();
      }

      if (!key->ExportEncryptedPrivateKey(ServerBoundCertService::kEPKIPassword,
                                          1, &private_key_info)) {
        DLOG(ERROR) << "Unable to export private key";
        *error = ERR_PRIVATE_KEY_EXPORT_FAILED;
        return result.Pass();
      }
      break;
    }
    default:
      NOTREACHED();
      *error = ERR_INVALID_ARGUMENT;
      return result.Pass();
  }

  // TODO(rkn): Perhaps ExportPrivateKey should be changed to output a
  // std::string* to prevent this copying.
  std::string key_out(private_key_info.begin(), private_key_info.end());

  result.reset(new ServerBoundCertStore::ServerBoundCert(
      server_identifier, type, not_valid_before, not_valid_after, key_out,
      der_cert));
  UMA_HISTOGRAM_CUSTOM_TIMES("DomainBoundCerts.GenerateCertTime",
                             base::TimeTicks::Now() - start,
                             base::TimeDelta::FromMilliseconds(1),
                             base::TimeDelta::FromMinutes(5),
                             50);
  *error = OK;
  return result.Pass();
}

}  // namespace

// Represents the output and result callback of a request.
class ServerBoundCertServiceRequest {
 public:
  ServerBoundCertServiceRequest(base::TimeTicks request_start,
                                const CompletionCallback& callback,
                                SSLClientCertType* type,
                                std::string* private_key,
                                std::string* cert)
      : request_start_(request_start),
        callback_(callback),
        type_(type),
        private_key_(private_key),
        cert_(cert) {
  }

  // Ensures that the result callback will never be made.
  void Cancel() {
    RecordGetDomainBoundCertResult(ASYNC_CANCELLED);
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
    switch (error) {
      case OK: {
        base::TimeDelta request_time = base::TimeTicks::Now() - request_start_;
        UMA_HISTOGRAM_CUSTOM_TIMES("DomainBoundCerts.GetCertTimeAsync",
                                   request_time,
                                   base::TimeDelta::FromMilliseconds(1),
                                   base::TimeDelta::FromMinutes(5),
                                   50);
        RecordGetCertTime(request_time);
        RecordGetDomainBoundCertResult(ASYNC_SUCCESS);
        break;
      }
      case ERR_KEY_GENERATION_FAILED:
        RecordGetDomainBoundCertResult(ASYNC_FAILURE_KEYGEN);
        break;
      case ERR_ORIGIN_BOUND_CERT_GENERATION_FAILED:
        RecordGetDomainBoundCertResult(ASYNC_FAILURE_CREATE_CERT);
        break;
      case ERR_PRIVATE_KEY_EXPORT_FAILED:
        RecordGetDomainBoundCertResult(ASYNC_FAILURE_EXPORT_KEY);
        break;
      case ERR_INSUFFICIENT_RESOURCES:
        RecordGetDomainBoundCertResult(WORKER_FAILURE);
        break;
      default:
        RecordGetDomainBoundCertResult(ASYNC_FAILURE_UNKNOWN);
        break;
    }
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
  base::TimeTicks request_start_;
  CompletionCallback callback_;
  SSLClientCertType* type_;
  std::string* private_key_;
  std::string* cert_;
};

// ServerBoundCertServiceWorker runs on a worker thread and takes care of the
// blocking process of performing key generation. Will take care of deleting
// itself once Start() is called.
class ServerBoundCertServiceWorker {
 public:
  typedef base::Callback<void(
      const std::string&,
      int,
      scoped_ptr<ServerBoundCertStore::ServerBoundCert>)> WorkerDoneCallback;

  ServerBoundCertServiceWorker(
      const std::string& server_identifier,
      SSLClientCertType type,
      const WorkerDoneCallback& callback)
      : server_identifier_(server_identifier),
        type_(type),
        serial_number_(base::RandInt(0, std::numeric_limits<int>::max())),
        origin_loop_(base::MessageLoopProxy::current()),
        callback_(callback) {
  }

  bool Start(const scoped_refptr<base::TaskRunner>& task_runner) {
    DCHECK(origin_loop_->RunsTasksOnCurrentThread());

    return task_runner->PostTask(
        FROM_HERE,
        base::Bind(&ServerBoundCertServiceWorker::Run, base::Owned(this)));
  }

 private:
  void Run() {
    // Runs on a worker thread.
    int error = ERR_FAILED;
    scoped_ptr<ServerBoundCertStore::ServerBoundCert> cert =
        GenerateCert(server_identifier_, type_, serial_number_, &error);
    DVLOG(1) << "GenerateCert " << server_identifier_ << " " << type_
             << " returned " << error;
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
    origin_loop_->PostTask(FROM_HERE,
                           base::Bind(callback_, server_identifier_, error,
                                      base::Passed(&cert)));
  }

  const std::string server_identifier_;
  const SSLClientCertType type_;
  // Note that serial_number_ must be initialized on a non-worker thread
  // (see documentation for GenerateCert).
  uint32 serial_number_;
  scoped_refptr<base::SequencedTaskRunner> origin_loop_;
  WorkerDoneCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ServerBoundCertServiceWorker);
};

// A ServerBoundCertServiceJob is a one-to-one counterpart of an
// ServerBoundCertServiceWorker. It lives only on the ServerBoundCertService's
// origin message loop.
class ServerBoundCertServiceJob {
 public:
  ServerBoundCertServiceJob(SSLClientCertType type)
      : type_(type) {
  }

  ~ServerBoundCertServiceJob() {
    if (!requests_.empty())
      DeleteAllCanceled();
  }

  SSLClientCertType type() const { return type_; }

  void AddRequest(ServerBoundCertServiceRequest* request) {
    requests_.push_back(request);
  }

  void HandleResult(int error,
                    SSLClientCertType type,
                    const std::string& private_key,
                    const std::string& cert) {
    PostAll(error, type, private_key, cert);
  }

 private:
  void PostAll(int error,
               SSLClientCertType type,
               const std::string& private_key,
               const std::string& cert) {
    std::vector<ServerBoundCertServiceRequest*> requests;
    requests_.swap(requests);

    for (std::vector<ServerBoundCertServiceRequest*>::iterator
         i = requests.begin(); i != requests.end(); i++) {
      (*i)->Post(error, type, private_key, cert);
      // Post() causes the ServerBoundCertServiceRequest to delete itself.
    }
  }

  void DeleteAllCanceled() {
    for (std::vector<ServerBoundCertServiceRequest*>::iterator
         i = requests_.begin(); i != requests_.end(); i++) {
      if ((*i)->canceled()) {
        delete *i;
      } else {
        LOG(DFATAL) << "ServerBoundCertServiceRequest leaked!";
      }
    }
  }

  std::vector<ServerBoundCertServiceRequest*> requests_;
  SSLClientCertType type_;
};

// static
const char ServerBoundCertService::kEPKIPassword[] = "";

ServerBoundCertService::RequestHandle::RequestHandle()
    : service_(NULL),
      request_(NULL) {}

ServerBoundCertService::RequestHandle::~RequestHandle() {
  Cancel();
}

void ServerBoundCertService::RequestHandle::Cancel() {
  if (request_) {
    service_->CancelRequest(request_);
    request_ = NULL;
    callback_.Reset();
  }
}

void ServerBoundCertService::RequestHandle::RequestStarted(
    ServerBoundCertService* service,
    ServerBoundCertServiceRequest* request,
    const CompletionCallback& callback) {
  DCHECK(request_ == NULL);
  service_ = service;
  request_ = request;
  callback_ = callback;
}

void ServerBoundCertService::RequestHandle::OnRequestComplete(int result) {
  request_ = NULL;
  // Running the callback might delete |this|, so we can't touch any of our
  // members afterwards. Reset callback_ first.
  base::ResetAndReturn(&callback_).Run(result);
}

ServerBoundCertService::ServerBoundCertService(
    ServerBoundCertStore* server_bound_cert_store,
    const scoped_refptr<base::TaskRunner>& task_runner)
    : server_bound_cert_store_(server_bound_cert_store),
      task_runner_(task_runner),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      requests_(0),
      cert_store_hits_(0),
      inflight_joins_(0) {
  base::Time start = base::Time::Now();
  base::Time end = start + base::TimeDelta::FromDays(
          kValidityPeriodInDays + kSystemTimeValidityBufferInDays);
  is_system_time_valid_ = x509_util::IsSupportedValidityRange(start, end);
}

ServerBoundCertService::~ServerBoundCertService() {
  STLDeleteValues(&inflight_);
}

//static
std::string ServerBoundCertService::GetDomainForHost(const std::string& host) {
  std::string domain =
      RegistryControlledDomainService::GetDomainAndRegistry(host);
  if (domain.empty())
    return host;
  return domain;
}

int ServerBoundCertService::GetDomainBoundCert(
    const std::string& origin,
    const std::vector<uint8>& requested_types,
    SSLClientCertType* type,
    std::string* private_key,
    std::string* cert,
    const CompletionCallback& callback,
    RequestHandle* out_req) {
  DVLOG(1) << __FUNCTION__ << " " << origin << " "
           << (requested_types.empty() ? -1 : requested_types[0])
           << (requested_types.size() > 1 ? "..." : "");
  DCHECK(CalledOnValidThread());
  base::TimeTicks request_start = base::TimeTicks::Now();

  if (callback.is_null() || !private_key || !cert || origin.empty() ||
      requested_types.empty()) {
    RecordGetDomainBoundCertResult(INVALID_ARGUMENT);
    return ERR_INVALID_ARGUMENT;
  }

  std::string domain = GetDomainForHost(GURL(origin).host());
  if (domain.empty()) {
    RecordGetDomainBoundCertResult(INVALID_ARGUMENT);
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
    RecordGetDomainBoundCertResult(UNSUPPORTED_TYPE);
    // None of the requested types are supported.
    return ERR_CLIENT_AUTH_CERT_TYPE_UNSUPPORTED;
  }

  requests_++;

  // See if an identical request is currently in flight.
  ServerBoundCertServiceJob* job = NULL;
  std::map<std::string, ServerBoundCertServiceJob*>::const_iterator j;
  j = inflight_.find(domain);
  if (j != inflight_.end()) {
    // An identical request is in flight already. We'll just attach our
    // callback.
    job = j->second;
    // Check that the job is for an acceptable type of cert.
    if (std::find(requested_types.begin(), requested_types.end(), job->type())
        == requested_types.end()) {
      DVLOG(1) << "Found inflight job of wrong type " << job->type()
               << " for " << domain;
      // If we get here, the server is asking for different types of certs in
      // short succession.  This probably means the server is broken or
      // misconfigured.  Since we only store one type of cert per domain, we
      // are unable to handle this well.  Just return an error and let the first
      // job finish.
      RecordGetDomainBoundCertResult(TYPE_MISMATCH);
      return ERR_ORIGIN_BOUND_CERT_GENERATION_TYPE_MISMATCH;
    }
    inflight_joins_++;

    ServerBoundCertServiceRequest* request = new ServerBoundCertServiceRequest(
        request_start,
        base::Bind(&RequestHandle::OnRequestComplete,
                   base::Unretained(out_req)),
        type, private_key, cert);
    job->AddRequest(request);
    out_req->RequestStarted(this, request, callback);
    return ERR_IO_PENDING;
  }

  // Check if a domain bound cert of an acceptable type already exists for this
  // domain, and that it has not expired.
  base::Time expiration_time;
  if (server_bound_cert_store_->GetServerBoundCert(
      domain,
      type,
      &expiration_time,
      private_key,
      cert,
      base::Bind(&ServerBoundCertService::GotServerBoundCert,
                 weak_ptr_factory_.GetWeakPtr()))) {
    if (*type != CLIENT_CERT_INVALID_TYPE) {
      // Sync lookup found a cert.
      if (CertIsValid(domain, *type, expiration_time)) {
        DVLOG(1) << "Cert store had valid cert for " << domain
                 << " of type " << *type;
        cert_store_hits_++;
        RecordGetDomainBoundCertResult(SYNC_SUCCESS);
        base::TimeDelta request_time = base::TimeTicks::Now() - request_start;
        UMA_HISTOGRAM_TIMES("DomainBoundCerts.GetCertTimeSync", request_time);
        RecordGetCertTime(request_time);
        return OK;
      }
    }

    // Sync lookup did not find a cert, or it found an expired one.  Start
    // generating a new one.
    ServerBoundCertServiceWorker* worker = new ServerBoundCertServiceWorker(
        domain,
        preferred_type,
        base::Bind(&ServerBoundCertService::GeneratedServerBoundCert,
                   weak_ptr_factory_.GetWeakPtr()));
    if (!worker->Start(task_runner_)) {
      delete worker;
      // TODO(rkn): Log to the NetLog.
      LOG(ERROR) << "ServerBoundCertServiceWorker couldn't be started.";
      RecordGetDomainBoundCertResult(WORKER_FAILURE);
      return ERR_INSUFFICIENT_RESOURCES;
    }
  }

  // We are either waiting for async DB lookup, or waiting for cert generation.
  // Create a job & request to track it.
  job = new ServerBoundCertServiceJob(preferred_type);
  inflight_[domain] = job;

  ServerBoundCertServiceRequest* request = new ServerBoundCertServiceRequest(
      request_start,
      base::Bind(&RequestHandle::OnRequestComplete, base::Unretained(out_req)),
      type, private_key, cert);
  job->AddRequest(request);
  out_req->RequestStarted(this, request, callback);
  return ERR_IO_PENDING;
}

void ServerBoundCertService::GotServerBoundCert(
    const std::string& server_identifier,
    SSLClientCertType type,
    base::Time expiration_time,
    const std::string& key,
    const std::string& cert) {
  DCHECK(CalledOnValidThread());

  std::map<std::string, ServerBoundCertServiceJob*>::iterator j;
  j = inflight_.find(server_identifier);
  if (j == inflight_.end()) {
    NOTREACHED();
    return;
  }
  ServerBoundCertServiceJob* job = j->second;

  if (type != CLIENT_CERT_INVALID_TYPE) {
    // Async DB lookup found a cert.
    if (CertIsValid(server_identifier, type, expiration_time)) {
      DVLOG(1) << "Cert store had valid cert for " << server_identifier
               << " of type " << type;
      cert_store_hits_++;
      // ServerBoundCertServiceRequest::Post will do the histograms and stuff.
      HandleResult(OK, server_identifier, type, key, cert);
      return;
    }
  }

  // Async lookup did not find a cert, or it found an expired one.  Start
  // generating a new one.
  ServerBoundCertServiceWorker* worker = new ServerBoundCertServiceWorker(
      server_identifier,
      job->type(),
      base::Bind(&ServerBoundCertService::GeneratedServerBoundCert,
                 weak_ptr_factory_.GetWeakPtr()));
  if (!worker->Start(task_runner_)) {
    delete worker;
    // TODO(rkn): Log to the NetLog.
    LOG(ERROR) << "ServerBoundCertServiceWorker couldn't be started.";
    HandleResult(ERR_INSUFFICIENT_RESOURCES, server_identifier,
                 CLIENT_CERT_INVALID_TYPE, "", "");
    return;
  }
}

ServerBoundCertStore* ServerBoundCertService::GetCertStore() {
  return server_bound_cert_store_.get();
}

void ServerBoundCertService::CancelRequest(ServerBoundCertServiceRequest* req) {
  DCHECK(CalledOnValidThread());
  req->Cancel();
}

void ServerBoundCertService::GeneratedServerBoundCert(
    const std::string& server_identifier,
    int error,
    scoped_ptr<ServerBoundCertStore::ServerBoundCert> cert) {
  DCHECK(CalledOnValidThread());

  if (error == OK) {
    // TODO(mattm): we should just Pass() the cert object to
    // SetServerBoundCert().
    server_bound_cert_store_->SetServerBoundCert(
        cert->server_identifier(), cert->type(), cert->creation_time(),
        cert->expiration_time(), cert->private_key(), cert->cert());

    HandleResult(error, server_identifier, cert->type(), cert->private_key(),
                 cert->cert());
  } else {
    HandleResult(error, server_identifier, CLIENT_CERT_INVALID_TYPE, "", "");
  }
}

void ServerBoundCertService::HandleResult(
    int error,
    const std::string& server_identifier,
    SSLClientCertType type,
    const std::string& private_key,
    const std::string& cert) {
  DCHECK(CalledOnValidThread());

  std::map<std::string, ServerBoundCertServiceJob*>::iterator j;
  j = inflight_.find(server_identifier);
  if (j == inflight_.end()) {
    NOTREACHED();
    return;
  }
  ServerBoundCertServiceJob* job = j->second;
  inflight_.erase(j);

  job->HandleResult(error, type, private_key, cert);
  delete job;
}

int ServerBoundCertService::cert_count() {
  return server_bound_cert_store_->GetCertCount();
}

}  // namespace net
