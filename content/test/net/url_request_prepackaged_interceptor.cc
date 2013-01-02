// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/net/url_request_prepackaged_interceptor.h"

#include "base/file_util.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_filter.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace content {

namespace {

class URLRequestPrepackagedJob : public net::URLRequestFileJob {
 public:
  URLRequestPrepackagedJob(net::URLRequest* request,
                           net::NetworkDelegate* network_delegate,
                           const FilePath& file_path)
      : net::URLRequestFileJob(request, network_delegate, file_path) {}

  virtual int GetResponseCode() const { return 200; }

 private:
  virtual ~URLRequestPrepackagedJob() {}

  DISALLOW_COPY_AND_ASSIGN(URLRequestPrepackagedJob);
};

}  // namespace

class URLRequestPrepackagedInterceptor::Delegate
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  Delegate() : hit_count_(0) {}
  virtual ~Delegate() {}

  void Register() {
    net::URLRequestFilter::GetInstance()->AddHostnameProtocolHandler(
        "http", "localhost",
        scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>(this));
  }

  static void Unregister() {
    net::URLRequestFilter::GetInstance()->RemoveHostnameHandler("http",
                                                                "localhost");
  }

  // When requests for |url| arrive, respond with the contents of |path|. The
  // hostname of |url| must be "localhost" to avoid DNS lookups, and the scheme
  // must be "http".
  void SetResponse(const GURL& url,
                   const FilePath& path,
                   bool ignore_query) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    // It's ok to do a blocking disk access on this thread; this class
    // is just used for tests.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    EXPECT_TRUE(file_util::PathExists(path));
    if (ignore_query) {
      ignore_query_responses_[url] = path;
    } else {
      responses_[url] = path;
    }
  }

  // Returns how many requests have been issued that have a stored reply.
  int GetHitCount() const {
    base::AutoLock auto_lock(hit_count_lock_);
    return hit_count_;
  }

 private:
  typedef std::map<GURL, FilePath> ResponseMap;

  // When computing matches, this ignores the query parameters of the url.
  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (request->url().scheme() != "http" ||
        request->url().host() != "localhost") {
      return NULL;
    }

    ResponseMap::const_iterator it = responses_.find(request->url());
    if (it == responses_.end()) {
      // Search for this request's url, ignoring any query parameters.
      GURL url = request->url();
      if (url.has_query()) {
        GURL::Replacements replacements;
        replacements.ClearQuery();
        url = url.ReplaceComponents(replacements);
      }
      it = ignore_query_responses_.find(url);
      if (it == ignore_query_responses_.end())
        return NULL;
    }
    {
      base::AutoLock auto_lock(hit_count_lock_);
      ++hit_count_;
    }

    return new URLRequestPrepackagedJob(request,
                                        network_delegate,
                                        it->second);
  }

  ResponseMap responses_;
  ResponseMap ignore_query_responses_;

  mutable base::Lock hit_count_lock_;
  mutable int hit_count_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};


URLRequestPrepackagedInterceptor::URLRequestPrepackagedInterceptor()
    : delegate_(new Delegate) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&Delegate::Register,
                                     base::Unretained(delegate_)));
}

URLRequestPrepackagedInterceptor::~URLRequestPrepackagedInterceptor() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&Delegate::Unregister));
}

void URLRequestPrepackagedInterceptor::SetResponse(const GURL& url,
                                                   const FilePath& path) {
  CHECK_EQ("http", url.scheme());
  CHECK_EQ("localhost", url.host());
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&Delegate::SetResponse,
                                     base::Unretained(delegate_), url, path,
                                     false));
}

void URLRequestPrepackagedInterceptor::SetResponseIgnoreQuery(
    const GURL& url,
    const FilePath& path) {
  CHECK_EQ("http", url.scheme());
  CHECK_EQ("localhost", url.host());
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&Delegate::SetResponse,
                                     base::Unretained(delegate_), url, path,
                                     true));
}

int URLRequestPrepackagedInterceptor::GetHitCount() {
  return delegate_->GetHitCount();
}

}  // namespace content
