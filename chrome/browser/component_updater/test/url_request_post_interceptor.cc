// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/test/url_request_post_interceptor.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_simple_job.h"
#include "net/url_request/url_request_test_util.h"

using content::BrowserThread;

namespace component_updater {

// Returns a canned response.
class URLRequestMockJob : public net::URLRequestSimpleJob {
 public:
  URLRequestMockJob(net::URLRequest* request,
                    net::NetworkDelegate* network_delegate,
                    const std::string& response)
      : net::URLRequestSimpleJob(request, network_delegate),
        response_(response) {}

 protected:
  virtual int GetResponseCode() const OVERRIDE { return 200; }

  virtual int GetData(std::string* mime_type,
                      std::string* charset,
                      std::string* data,
                      const net::CompletionCallback& callback) const OVERRIDE {
    mime_type->assign("text/plain");
    charset->assign("US-ASCII");
    data->assign(response_);
    return net::OK;
  }

 private:
  virtual ~URLRequestMockJob() {}

  std::string response_;
  DISALLOW_COPY_AND_ASSIGN(URLRequestMockJob);
};

URLRequestPostInterceptor::URLRequestPostInterceptor(const GURL& url)
    : url_(url), hit_count_(0) {
}

URLRequestPostInterceptor::~URLRequestPostInterceptor() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ClearExpectations();
}

void URLRequestPostInterceptor::ClearExpectations() {
  while (!expectations_.empty()) {
    Expectation expectation(expectations_.front());
    delete expectation.first;
    expectations_.pop();
  }
}

GURL URLRequestPostInterceptor::GetUrl() const {
  return url_;
}

bool URLRequestPostInterceptor::ExpectRequest(
    class RequestMatcher* request_matcher) {
  expectations_.push(std::make_pair(request_matcher, ""));
  return true;
}

bool URLRequestPostInterceptor::ExpectRequest(
    class RequestMatcher* request_matcher,
    const base::FilePath& filepath) {
  std::string response;
  if (filepath.empty() || !base::ReadFileToString(filepath, &response))
    return false;
  expectations_.push(std::make_pair(request_matcher, response));
  return true;
}

int URLRequestPostInterceptor::GetHitCount() const {
  base::AutoLock auto_lock(interceptor_lock_);
  return hit_count_;
}

int URLRequestPostInterceptor::GetCount() const {
  base::AutoLock auto_lock(interceptor_lock_);
  return static_cast<int>(requests_.size());
}

std::vector<std::string> URLRequestPostInterceptor::GetRequests() const {
  base::AutoLock auto_lock(interceptor_lock_);
  return requests_;
}

std::string URLRequestPostInterceptor::GetRequestsAsString() const {
  std::vector<std::string> requests(GetRequests());

  std::string s = "Requests are:";

  int i = 0;
  for (std::vector<std::string>::const_iterator it = requests.begin();
       it != requests.end();
       ++it) {
    s.append(base::StringPrintf("\n  (%d): %s", ++i, it->c_str()));
  }

  return s;
}

void URLRequestPostInterceptor::Reset() {
  base::AutoLock auto_lock(interceptor_lock_);
  hit_count_ = 0;
  requests_.clear();
  ClearExpectations();
}

class URLRequestPostInterceptor::Delegate : public net::URLRequestInterceptor {
 public:
  Delegate(const std::string& scheme, const std::string& hostname)
      : scheme_(scheme), hostname_(hostname) {}

  void Register() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        scheme_, hostname_, scoped_ptr<net::URLRequestInterceptor>(this));
  }

  void Unregister() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    for (InterceptorMap::iterator it = interceptors_.begin();
         it != interceptors_.end();
         ++it)
      delete (*it).second;
    net::URLRequestFilter::GetInstance()->RemoveHostnameHandler(scheme_,
                                                                hostname_);
  }

  void OnCreateInterceptor(URLRequestPostInterceptor* interceptor) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    CHECK(interceptors_.find(interceptor->GetUrl()) == interceptors_.end());

    interceptors_.insert(std::make_pair(interceptor->GetUrl(), interceptor));
  }

 private:
  virtual ~Delegate() {}

  virtual net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    // Only intercepts POST.
    if (!request->has_upload())
      return NULL;

    GURL url = request->url();
    if (url.has_query()) {
      GURL::Replacements replacements;
      replacements.ClearQuery();
      url = url.ReplaceComponents(replacements);
    }

    InterceptorMap::const_iterator it(interceptors_.find(url));
    if (it == interceptors_.end())
      return NULL;

    // There is an interceptor hooked up for this url. Read the request body,
    // check the existing expectations, and handle the matching case by
    // popping the expectation off the queue, counting the match, and
    // returning a mock object to serve the canned response.
    URLRequestPostInterceptor* interceptor(it->second);

    const net::UploadDataStream* stream = request->get_upload();
    const net::UploadBytesElementReader* reader =
        stream->element_readers()[0]->AsBytesReader();
    const int size = reader->length();
    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(size));
    const std::string request_body(reader->bytes());

    {
      base::AutoLock auto_lock(interceptor->interceptor_lock_);
      interceptor->requests_.push_back(request_body);
      if (interceptor->expectations_.empty())
        return NULL;
      const URLRequestPostInterceptor::Expectation& expectation(
          interceptor->expectations_.front());
      if (expectation.first->Match(request_body)) {
        const std::string response(expectation.second);
        delete expectation.first;
        interceptor->expectations_.pop();
        ++interceptor->hit_count_;

        return new URLRequestMockJob(request, network_delegate, response);
      }
    }

    return NULL;
  }

  typedef std::map<GURL, URLRequestPostInterceptor*> InterceptorMap;
  InterceptorMap interceptors_;

  const std::string scheme_;
  const std::string hostname_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};

URLRequestPostInterceptorFactory::URLRequestPostInterceptorFactory(
    const std::string& scheme,
    const std::string& hostname)
    : scheme_(scheme),
      hostname_(hostname),
      delegate_(new URLRequestPostInterceptor::Delegate(scheme, hostname)) {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&URLRequestPostInterceptor::Delegate::Register,
                 base::Unretained(delegate_)));
}

URLRequestPostInterceptorFactory::~URLRequestPostInterceptorFactory() {
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&URLRequestPostInterceptor::Delegate::Unregister,
                 base::Unretained(delegate_)));
}

URLRequestPostInterceptor* URLRequestPostInterceptorFactory::CreateInterceptor(
    const base::FilePath& filepath) {
  const GURL base_url(
      base::StringPrintf("%s://%s", scheme_.c_str(), hostname_.c_str()));
  GURL absolute_url(base_url.Resolve(filepath.MaybeAsASCII()));
  URLRequestPostInterceptor* interceptor(
      new URLRequestPostInterceptor(absolute_url));
  bool res = BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&URLRequestPostInterceptor::Delegate::OnCreateInterceptor,
                 base::Unretained(delegate_),
                 base::Unretained(interceptor)));
  if (!res) {
    delete interceptor;
    return NULL;
  }

  return interceptor;
}

}  // namespace component_updater
