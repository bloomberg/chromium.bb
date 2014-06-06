// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/net/url_request_prepackaged_interceptor.h"

#include "base/file_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace content {

namespace {

class URLRequestPrepackagedJob : public net::URLRequestFileJob {
 public:
  URLRequestPrepackagedJob(net::URLRequest* request,
                           net::NetworkDelegate* network_delegate,
                           const base::FilePath& file_path)
      : net::URLRequestFileJob(
            request, network_delegate, file_path,
            content::BrowserThread::GetBlockingPool()->
                GetTaskRunnerWithShutdownBehavior(
                    base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)) {}

  virtual int GetResponseCode() const OVERRIDE { return 200; }

 private:
  virtual ~URLRequestPrepackagedJob() {}

  DISALLOW_COPY_AND_ASSIGN(URLRequestPrepackagedJob);
};

}  // namespace

class URLRequestPrepackagedInterceptor::Delegate
    : public net::URLRequestInterceptor {
 public:
  Delegate(const std::string& scheme, const std::string& hostname)
    : scheme_(scheme), hostname_(hostname), hit_count_(0) {}
  virtual ~Delegate() {}

  void Register() {
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        scheme_, hostname_,
        scoped_ptr<net::URLRequestInterceptor>(this));
  }

  static void Unregister(
      const std::string& scheme,
      const std::string& hostname) {
    net::URLRequestFilter::GetInstance()->RemoveHostnameHandler(scheme,
                                                                hostname);
  }

  // When requests for |url| arrive, respond with the contents of |path|. The
  // hostname and scheme of |url| must match the corresponding parameters
  // passed as constructor arguments.
  void SetResponse(const GURL& url,
                   const base::FilePath& path,
                   bool ignore_query) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    // It's ok to do a blocking disk access on this thread; this class
    // is just used for tests.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    EXPECT_TRUE(base::PathExists(path));
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
  typedef std::map<GURL, base::FilePath> ResponseMap;

  // When computing matches, this ignores the query parameters of the url.
  virtual net::URLRequestJob* MaybeInterceptRequest(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (request->url().scheme() != scheme_ ||
        request->url().host() != hostname_) {
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

  const std::string scheme_;
  const std::string hostname_;

  ResponseMap responses_;
  ResponseMap ignore_query_responses_;

  mutable base::Lock hit_count_lock_;
  mutable int hit_count_;

  DISALLOW_COPY_AND_ASSIGN(Delegate);
};


URLRequestPrepackagedInterceptor::URLRequestPrepackagedInterceptor(
    const std::string& scheme,
    const std::string& hostname)
    : scheme_(scheme),
      hostname_(hostname),
      delegate_(new Delegate(scheme, hostname)) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&Delegate::Register,
                                     base::Unretained(delegate_)));
}

URLRequestPrepackagedInterceptor::~URLRequestPrepackagedInterceptor() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&Delegate::Unregister,
                                     scheme_,
                                     hostname_));
}

void URLRequestPrepackagedInterceptor::SetResponse(
    const GURL& url,
    const base::FilePath& path) {
  CHECK_EQ(scheme_, url.scheme());
  CHECK_EQ(hostname_, url.host());
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&Delegate::SetResponse,
                                     base::Unretained(delegate_), url, path,
                                     false));
}

void URLRequestPrepackagedInterceptor::SetResponseIgnoreQuery(
    const GURL& url,
    const base::FilePath& path) {
  CHECK_EQ(scheme_, url.scheme());
  CHECK_EQ(hostname_, url.host());
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&Delegate::SetResponse,
                                     base::Unretained(delegate_), url, path,
                                     true));
}

int URLRequestPrepackagedInterceptor::GetHitCount() {
  return delegate_->GetHitCount();
}


URLLocalHostRequestPrepackagedInterceptor
  ::URLLocalHostRequestPrepackagedInterceptor()
    : URLRequestPrepackagedInterceptor("http", "localhost") {
}

}  // namespace content
