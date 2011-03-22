// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/view_http_cache_job_factory.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/task.h"
#include "chrome/common/url_constants.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_simple_job.h"
#include "net/url_request/view_cache_helper.h"

namespace {

// A job subclass that dumps an HTTP cache entry.
class ViewHttpCacheJob : public net::URLRequestJob {
 public:
  explicit ViewHttpCacheJob(net::URLRequest* request)
      : net::URLRequestJob(request),
        core_(new Core),
        ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            callback_(NewCallback(this,
                                  &ViewHttpCacheJob::OnStartCompleted))) {}

  virtual void Start();
  virtual void Kill();
  virtual bool GetMimeType(std::string* mime_type) const {
    return core_->GetMimeType(mime_type);
  }
  virtual bool GetCharset(std::string* charset) {
    return core_->GetCharset(charset);
  }
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read) {
    return core_->ReadRawData(buf, buf_size, bytes_read);
  }

 private:
  class Core : public base::RefCounted<Core> {
   public:
    Core()
        : data_offset_(0),
          ALLOW_THIS_IN_INITIALIZER_LIST(
              callback_(this, &Core::OnIOComplete)),
          user_callback_(NULL) {}

    int Start(const net::URLRequest& request, Callback0::Type* callback);

    // Prevents it from invoking its callback. It will self-delete.
    void Orphan() {
      DCHECK(user_callback_);
      user_callback_ = NULL;
    }

    bool GetMimeType(std::string* mime_type) const;
    bool GetCharset(std::string* charset);
    bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read);

   private:
    friend class base::RefCounted<Core>;

    ~Core() {}

    // Called when ViewCacheHelper completes the operation.
    void OnIOComplete(int result);

    std::string data_;
    int data_offset_;
    net::ViewCacheHelper cache_helper_;
    net::CompletionCallbackImpl<Core> callback_;
    Callback0::Type* user_callback_;

    DISALLOW_COPY_AND_ASSIGN(Core);
  };

  ~ViewHttpCacheJob() {}

  void StartAsync();
  void OnStartCompleted();

  scoped_refptr<Core> core_;
  ScopedRunnableMethodFactory<ViewHttpCacheJob> method_factory_;
  scoped_ptr<Callback0::Type> callback_;

  DISALLOW_COPY_AND_ASSIGN(ViewHttpCacheJob);
};

void ViewHttpCacheJob::Start() {
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(&ViewHttpCacheJob::StartAsync));
}

void ViewHttpCacheJob::Kill() {
  method_factory_.RevokeAll();
  core_->Orphan();
  core_ = NULL;
  net::URLRequestJob::Kill();
}

void ViewHttpCacheJob::StartAsync() {
  DCHECK(request());

  if (!request())
    return;

  int rv = core_->Start(*request(), callback_.get());
  if (rv != net::ERR_IO_PENDING) {
    DCHECK_EQ(net::OK, rv);
    OnStartCompleted();
  }
}

void ViewHttpCacheJob::OnStartCompleted() {
  NotifyHeadersComplete();
}

int ViewHttpCacheJob::Core::Start(const net::URLRequest& request,
                                  Callback0::Type* callback) {
  DCHECK(callback);
  DCHECK(!user_callback_);

  AddRef();  // Released on OnIOComplete().
  std::string cache_key =
      request.url().spec().substr(strlen(chrome::kNetworkViewCacheURL));

  int rv;
  if (cache_key.empty()) {
    rv = cache_helper_.GetContentsHTML(request.context(),
                                       chrome::kNetworkViewCacheURL, &data_,
                                       &callback_);
  } else {
    rv = cache_helper_.GetEntryInfoHTML(cache_key, request.context(),
                                        &data_, &callback_);
  }

  if (rv == net::ERR_IO_PENDING)
    user_callback_ = callback;

  return rv;
}

bool ViewHttpCacheJob::Core::GetMimeType(std::string* mime_type) const {
  mime_type->assign("text/html");
  return true;
}

bool ViewHttpCacheJob::Core::GetCharset(std::string* charset) {
  charset->assign("UTF-8");
  return true;
}

bool ViewHttpCacheJob::Core::ReadRawData(net::IOBuffer* buf,
                                         int buf_size,
                                         int* bytes_read) {
  DCHECK(bytes_read);
  int remaining = static_cast<int>(data_.size()) - data_offset_;
  if (buf_size > remaining)
    buf_size = remaining;
  memcpy(buf->data(), data_.data() + data_offset_, buf_size);
  data_offset_ += buf_size;
  *bytes_read = buf_size;
  return true;
}

void ViewHttpCacheJob::Core::OnIOComplete(int result) {
  DCHECK_EQ(net::OK, result);

  if (user_callback_)
    user_callback_->Run();

  // We may be holding the last reference to this job. Do not access |this|
  // after Release().
  Release();  // Acquired on Start().
}

}  // namespace.

// Static.
bool ViewHttpCacheJobFactory::IsSupportedURL(const GURL& url) {
  return StartsWithASCII(url.spec(), chrome::kNetworkViewCacheURL,
                         true /*case_sensitive*/);
}

// Static.
net::URLRequestJob* ViewHttpCacheJobFactory::CreateJobForRequest(
    net::URLRequest* request) {
  return new ViewHttpCacheJob(request);
}
