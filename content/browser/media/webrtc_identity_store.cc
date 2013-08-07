// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/webrtc_identity_store.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/threading/worker_pool.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_util.h"
#include "url/gurl.h"

namespace content {

struct WebRTCIdentityRequestResult {
  int error;
  std::string certificate;
  std::string private_key;
};

static void GenerateIdentityWorker(const std::string& common_name,
                                   WebRTCIdentityRequestResult* result) {
  result->error = net::OK;
  int serial_number = base::RandInt(0, std::numeric_limits<int>::max());

  scoped_ptr<crypto::RSAPrivateKey> key(crypto::RSAPrivateKey::Create(1024));
  if (!key.get()) {
    DLOG(ERROR) << "Unable to create key pair for client";
    result->error = net::ERR_KEY_GENERATION_FAILED;
    return;
  }

  base::Time now = base::Time::Now();
  bool success =
      net::x509_util::CreateSelfSignedCert(key.get(),
                                          "CN=" + common_name,
                                          serial_number,
                                          now,
                                          now + base::TimeDelta::FromDays(30),
                                          &result->certificate);
  if (!success) {
    DLOG(ERROR) << "Unable to create x509 cert for client";
    result->error = net::ERR_SELF_SIGNED_CERT_GENERATION_FAILED;
    return;
  }

  std::vector<uint8> private_key_info;
  if (!key->ExportPrivateKey(&private_key_info)) {
    DLOG(ERROR) << "Unable to export private key";
    result->error = net::ERR_PRIVATE_KEY_EXPORT_FAILED;
    return;
  }

  result->private_key =
      std::string(private_key_info.begin(), private_key_info.end());
}

// The class represents an identity request internal to WebRTCIdentityStore.
// It has a one-to-one mapping to the external version of the request
// WebRTCIdentityRequestHandle, which is the target of the
// WebRTCIdentityRequest's completion callback.
// It's deleted automatically when the request is completed.
class WebRTCIdentityRequest {
 public:
  WebRTCIdentityRequest(const WebRTCIdentityStore::CompletionCallback& callback)
      : callback_(callback) {}

  void Cancel() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    callback_.Reset();
  }

 private:
  friend class WebRTCIdentityStore;

  void Post(WebRTCIdentityRequestResult* result) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (callback_.is_null())
      return;
    callback_.Run(result->error, result->certificate, result->private_key);
    // "this" will be deleted after this point.
  }

  WebRTCIdentityStore::CompletionCallback callback_;
};

// The class represents an identity request which calls back to the external
// client when the request completes.
// Its lifetime is tied with the Callback held by the corresponding
// WebRTCIdentityRequest.
class WebRTCIdentityRequestHandle {
 public:
  WebRTCIdentityRequestHandle(
      WebRTCIdentityStore* store,
      const WebRTCIdentityStore::CompletionCallback& callback)
      : store_(store), request_(NULL), callback_(callback) {}

 private:
  friend class WebRTCIdentityStore;

  // Cancel the request.  Does nothing if the request finished or was already
  // cancelled.
  void Cancel() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (!request_)
      return;

    callback_.Reset();
    WebRTCIdentityRequest* request = request_;
    request_ = NULL;
    // "this" will be deleted after the following call, because "this" is
    // owned by the Callback held by |request|.
    request->Cancel();
  }

  void OnRequestStarted(WebRTCIdentityRequest* request) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    DCHECK(request);
    request_ = request;
  }

  void OnRequestComplete(int error,
                         const std::string& certificate,
                         const std::string& private_key) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    DCHECK(request_);
    request_ = NULL;
    base::ResetAndReturn(&callback_).Run(error, certificate, private_key);
  }

  WebRTCIdentityStore* store_;
  WebRTCIdentityRequest* request_;
  WebRTCIdentityStore::CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(WebRTCIdentityRequestHandle);
};

WebRTCIdentityStore::WebRTCIdentityStore()
    : task_runner_(base::WorkerPool::GetTaskRunner(true)) {}

WebRTCIdentityStore::~WebRTCIdentityStore() {}

base::Closure WebRTCIdentityStore::RequestIdentity(
    const GURL& origin,
    const std::string& identity_name,
    const std::string& common_name,
    const CompletionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  WebRTCIdentityRequestHandle* handle =
      new WebRTCIdentityRequestHandle(this, callback);

  WebRTCIdentityRequest* request = new WebRTCIdentityRequest(base::Bind(
      &WebRTCIdentityRequestHandle::OnRequestComplete, base::Owned(handle)));
  handle->OnRequestStarted(request);

  WebRTCIdentityRequestResult* result = new WebRTCIdentityRequestResult();
  if (!task_runner_->PostTaskAndReply(
          FROM_HERE,
          base::Bind(&GenerateIdentityWorker, common_name, result),
          base::Bind(&WebRTCIdentityRequest::Post,
                     base::Owned(request),
                     base::Owned(result))))
    return base::Closure();

  return base::Bind(&WebRTCIdentityRequestHandle::Cancel,
                    base::Unretained(handle));
}

void WebRTCIdentityStore::SetTaskRunnerForTesting(
    const scoped_refptr<base::TaskRunner>& task_runner) {
  task_runner_ = task_runner;
}

}  // namespace content
