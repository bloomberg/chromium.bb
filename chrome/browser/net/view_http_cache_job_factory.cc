// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/view_http_cache_job_factory.h"

#include "base/message_loop.h"
#include "chrome/common/url_constants.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_simple_job.h"
#include "net/url_request/view_cache_helper.h"

namespace {

// A job subclass that dumps an HTTP cache entry.
class ViewHttpCacheJob : public URLRequestJob {
 public:
  explicit ViewHttpCacheJob(URLRequest* request)
      : URLRequestJob(request), data_offset_(0),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            callback_(this, &ViewHttpCacheJob::OnIOComplete)) {}

  virtual void Start();
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual bool GetCharset(std::string* charset);
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read);

 private:
  ~ViewHttpCacheJob() {}

  // Called when ViewCacheHelper completes the operation.
  void OnIOComplete(int result);

  std::string data_;
  int data_offset_;
  net::ViewCacheHelper cache_helper_;
  net::CompletionCallbackImpl<ViewHttpCacheJob> callback_;
};

void ViewHttpCacheJob::Start() {
  if (!request_)
    return;

  std::string cache_key =
      request_->url().spec().substr(strlen(chrome::kNetworkViewCacheURL));

  int rv;
  if (cache_key.empty()) {
    rv = cache_helper_.GetContentsHTML(request_->context(),
                                       chrome::kNetworkViewCacheURL, &data_,
                                       &callback_);
  } else {
    rv = cache_helper_.GetEntryInfoHTML(cache_key, request_->context(),
                                        &data_, &callback_);
  }

  if (rv != net::ERR_IO_PENDING) {
    // Start reading asynchronously so that all error reporting and data
    // callbacks happen as they would for network requests.
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &ViewHttpCacheJob::OnIOComplete, rv));
  }
}

bool ViewHttpCacheJob::GetMimeType(std::string* mime_type) const {
  mime_type->assign("text/html");
  return true;
}

bool ViewHttpCacheJob::GetCharset(std::string* charset) {
  charset->assign("UTF-8");
  return true;
}

bool ViewHttpCacheJob::ReadRawData(net::IOBuffer* buf, int buf_size,
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

void ViewHttpCacheJob::OnIOComplete(int result) {
  DCHECK_EQ(net::OK, result);

  // Notify that the headers are complete.
  NotifyHeadersComplete();
}

}  // namespace.

// Static.
bool ViewHttpCacheJobFactory::IsSupportedURL(const GURL& url) {
  return StartsWithASCII(url.spec(), chrome::kNetworkViewCacheURL,
                         true /*case_sensitive*/);
}

// Static.
URLRequestJob* ViewHttpCacheJobFactory::CreateJobForRequest(
    URLRequest* request) {
  return new ViewHttpCacheJob(request);
}
