// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/test/url_request_post_interceptor.h"
#include "base/file_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_simple_job.h"
#include "net/url_request/url_request_test_util.h"

using content::BrowserThread;

// Returns an empty response body for any request.
class URLRequestMockJob : public net::URLRequestSimpleJob {
 public:
  URLRequestMockJob(net::URLRequest* request,
                    net::NetworkDelegate* network_delegate)
      : net::URLRequestSimpleJob(request, network_delegate) {}

 protected:
  virtual int GetData(std::string* mime_type,
                      std::string* charset,
                      std::string* data,
                      const net::CompletionCallback& callback) const OVERRIDE {
    mime_type->assign("text/plain");
    charset->assign("US-ASCII");
    data->clear();
    return net::OK;
  }

 private:
  virtual ~URLRequestMockJob() {}

  DISALLOW_COPY_AND_ASSIGN(URLRequestMockJob);
};

class URLRequestPostInterceptor::Delegate
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  Delegate(const std::string& scheme, const std::string& hostname)
      : hit_count_(0), scheme_(scheme), hostname_(hostname) {}

  void Register() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    net::URLRequestFilter::GetInstance()->AddHostnameProtocolHandler(
        scheme_, hostname_,
        scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>(this));
  }

  void Unregister() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    net::URLRequestFilter::GetInstance()->
      RemoveHostnameHandler(scheme_, hostname_);
  }

  // Returns how many requests have been intercepted and captured.
  int GetHitCount() const {
    base::AutoLock auto_lock(hit_count_lock_);
    DCHECK(hit_count_ == static_cast<int>(requests_.size()));
    return hit_count_;
  }

  std::vector<std::string> GetRequests() const {
    base::AutoLock auto_lock(hit_count_lock_);
    return requests_;
  }

 private:
  virtual ~Delegate() {}

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    if (!request->has_upload())
      return NULL;

    const net::UploadDataStream* stream = request->get_upload();
    const net::UploadBytesElementReader* reader =
        stream->element_readers()[0]->AsBytesReader();
    const int size = reader->length();
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(size));
    const std::string request_body(reader->bytes());

    {
      base::AutoLock auto_lock(hit_count_lock_);
      ++hit_count_;
      requests_.push_back(request_body);
    }

    return new URLRequestMockJob(request, network_delegate);
  }

  mutable base::Lock hit_count_lock_;
  mutable int hit_count_;
  mutable std::vector<std::string> requests_;

  const std::string scheme_;
  const std::string hostname_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

URLRequestPostInterceptor::URLRequestPostInterceptor(
    const std::string& scheme,
    const std::string& hostname)
    : delegate_(new Delegate(scheme, hostname)) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&Delegate::Register,
                                     base::Unretained(delegate_)));
}

URLRequestPostInterceptor::~URLRequestPostInterceptor() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&Delegate::Unregister,
                                     base::Unretained(delegate_)));
}

int URLRequestPostInterceptor::GetHitCount() const {
  return delegate_->GetHitCount();
}

std::vector<std::string> URLRequestPostInterceptor::GetRequests() const {
  return delegate_->GetRequests();
}

std::string URLRequestPostInterceptor::GetRequestsAsString() const {
  std::vector<std::string> requests(GetRequests());

  std::string s = "Requests are:";

  int i = 0;
  for (std::vector<std::string>::const_iterator it = requests.begin();
      it != requests.end(); ++it) {
    s.append(base::StringPrintf("\n  (%d): %s", ++i, it->c_str()));
  }

  return s;
}

