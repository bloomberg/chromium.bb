// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_PUBLIC_UTIL_TESTING_GENERIC_URL_REQUEST_MOCKS_H_
#define HEADLESS_PUBLIC_UTIL_TESTING_GENERIC_URL_REQUEST_MOCKS_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "headless/public/util/generic_url_request_job.h"
#include "headless/public/util/testing/generic_url_request_mocks.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job_factory.h"

namespace headless {

class MockGenericURLRequestJobDelegate : public GenericURLRequestJob::Delegate {
 public:
  MockGenericURLRequestJobDelegate();
  ~MockGenericURLRequestJobDelegate() override;

  // GenericURLRequestJob::Delegate methods:
  void OnResourceLoadFailed(const Request* request, net::Error error) override;
  void OnResourceLoadComplete(
      const Request* request,
      const GURL& final_url,
      scoped_refptr<net::HttpResponseHeaders> response_headers,
      const char* body,
      size_t body_size) override;

 private:
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockGenericURLRequestJobDelegate);
};

// TODO(alexclarke): We may be able to replace this with the CookieMonster.
class MockCookieStore : public net::CookieStore {
 public:
  MockCookieStore();
  ~MockCookieStore() override;

  // net::CookieStore implementation:
  void SetCookieWithOptionsAsync(const GURL& url,
                                 const std::string& cookie_line,
                                 const net::CookieOptions& options,
                                 SetCookiesCallback callback) override;

  void SetCanonicalCookieAsync(std::unique_ptr<net::CanonicalCookie> cookie,
                               bool secure_source,
                               bool modify_http_only,
                               SetCookiesCallback callback) override;

  void GetCookiesWithOptionsAsync(const GURL& url,
                                  const net::CookieOptions& options,
                                  GetCookiesCallback callback) override;

  void GetCookieListWithOptionsAsync(const GURL& url,
                                     const net::CookieOptions& options,
                                     GetCookieListCallback callback) override;

  void GetAllCookiesAsync(GetCookieListCallback callback) override;

  void DeleteCookieAsync(const GURL& url,
                         const std::string& cookie_name,
                         base::OnceClosure callback) override;

  void DeleteCanonicalCookieAsync(const net::CanonicalCookie& cookie,
                                  DeleteCallback callback) override;

  void DeleteAllCreatedBetweenAsync(const base::Time& delete_begin,
                                    const base::Time& delete_end,
                                    DeleteCallback callback) override;

  void DeleteAllCreatedBetweenWithPredicateAsync(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      const CookiePredicate& predicate,
      DeleteCallback callback) override;

  void DeleteSessionCookiesAsync(DeleteCallback) override;

  void FlushStore(base::OnceClosure callback) override;

  void SetForceKeepSessionState() override;

  std::unique_ptr<CookieChangedSubscription> AddCallbackForCookie(
      const GURL& url,
      const std::string& name,
      const CookieChangedCallback& callback) override;

  std::unique_ptr<CookieChangedSubscription> AddCallbackForAllChanges(
      const CookieChangedCallback& callback) override;

  bool IsEphemeral() override;

  net::CookieList* cookies() { return &cookies_; }

 private:
  void SendCookies(const GURL& url,
                   const net::CookieOptions& options,
                   GetCookieListCallback callback);

  net::CookieList cookies_;

  DISALLOW_COPY_AND_ASSIGN(MockCookieStore);
};

class MockURLRequestDelegate : public net::URLRequest::Delegate {
 public:
  MockURLRequestDelegate();
  ~MockURLRequestDelegate() override;

  void OnResponseStarted(net::URLRequest* request, int net_error) override;
  void OnReadCompleted(net::URLRequest* request, int bytes_read) override;
  const std::string& response_data() const;
  const net::IOBufferWithSize* metadata() const;

 private:
  std::string response_data_;

  DISALLOW_COPY_AND_ASSIGN(MockURLRequestDelegate);
};

}  // namespace headless

#endif  // HEADLESS_PUBLIC_UTIL_TESTING_GENERIC_URL_REQUEST_MOCKS_H_
