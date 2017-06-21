// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/public/util/testing/generic_url_request_mocks.h"

#include <utility>

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/http/http_response_headers.h"

namespace net {
class URLRequestJob;
}  // namespace net

namespace headless {

// MockGenericURLRequestJobDelegate
MockGenericURLRequestJobDelegate::MockGenericURLRequestJobDelegate()
    : main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

MockGenericURLRequestJobDelegate::~MockGenericURLRequestJobDelegate() {}

// GenericURLRequestJob::Delegate methods:
void MockGenericURLRequestJobDelegate::OnPendingRequest(
    PendingRequest* pending_request) {
  // Simulate the client acknowledging the callback from a different thread.
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MockGenericURLRequestJobDelegate::ApplyPolicy,
                            base::Unretained(this), pending_request));
}

void MockGenericURLRequestJobDelegate::SetPolicy(Policy policy) {
  policy_ = std::move(policy);
}

void MockGenericURLRequestJobDelegate::ApplyPolicy(
    PendingRequest* pending_request) {
  if (policy_.is_null()) {
    pending_request->AllowRequest();
  } else {
    policy_.Run(pending_request);
  }
}

void MockGenericURLRequestJobDelegate::OnResourceLoadFailed(
    const Request* request,
    net::Error error) {}

void MockGenericURLRequestJobDelegate::OnResourceLoadComplete(
    const Request* request,
    const GURL& final_url,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    const char* body,
    size_t body_size) {}

// MockCookieStore
MockCookieStore::MockCookieStore() {}
MockCookieStore::~MockCookieStore() {}

void MockCookieStore::SetCookieWithOptionsAsync(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    const SetCookiesCallback& callback) {
  CHECK(false);
}

void MockCookieStore::SetCookieWithDetailsAsync(
    const GURL& url,
    const std::string& name,
    const std::string& value,
    const std::string& domain,
    const std::string& path,
    base::Time creation_time,
    base::Time expiration_time,
    base::Time last_access_time,
    bool secure,
    bool http_only,
    net::CookieSameSite same_site,
    net::CookiePriority priority,
    const SetCookiesCallback& callback) {
  CHECK(false);
}

void MockCookieStore::SetCanonicalCookieAsync(
    std::unique_ptr<net::CanonicalCookie> cookie,
    bool secure_source,
    bool can_modify_httponly,
    const SetCookiesCallback& callback) {
  CHECK(false);
}

void MockCookieStore::GetCookiesWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    const GetCookiesCallback& callback) {
  CHECK(false);
}

void MockCookieStore::GetCookieListWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    const GetCookieListCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&MockCookieStore::SendCookies,
                            base::Unretained(this), url, options, callback));
}

void MockCookieStore::GetAllCookiesAsync(
    const GetCookieListCallback& callback) {
  CHECK(false);
}

void MockCookieStore::DeleteCookieAsync(const GURL& url,
                                        const std::string& cookie_name,
                                        const base::Closure& callback) {
  CHECK(false);
}

void MockCookieStore::DeleteCanonicalCookieAsync(
    const net::CanonicalCookie& cookie,
    const DeleteCallback& callback) {
  CHECK(false);
}

void MockCookieStore::DeleteAllCreatedBetweenAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const DeleteCallback& callback) {
  CHECK(false);
}

void MockCookieStore::DeleteAllCreatedBetweenWithPredicateAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const CookiePredicate& predicate,
    const DeleteCallback& callback) {
  CHECK(false);
}

void MockCookieStore::DeleteSessionCookiesAsync(const DeleteCallback&) {
  CHECK(false);
}

void MockCookieStore::FlushStore(const base::Closure& callback) {
  CHECK(false);
}

void MockCookieStore::SetForceKeepSessionState() {
  CHECK(false);
}

std::unique_ptr<net::CookieStore::CookieChangedSubscription>
MockCookieStore::AddCallbackForCookie(const GURL& url,
                                      const std::string& name,
                                      const CookieChangedCallback& callback) {
  CHECK(false);
  return nullptr;
}

bool MockCookieStore::IsEphemeral() {
  CHECK(false);
  return true;
}

void MockCookieStore::SendCookies(const GURL& url,
                                  const net::CookieOptions& options,
                                  const GetCookieListCallback& callback) {
  net::CookieList result;
  for (const auto& cookie : cookies_) {
    if (cookie.IncludeForRequestURL(url, options))
      result.push_back(cookie);
  }
  callback.Run(result);
}

// MockURLRequestDelegate
MockURLRequestDelegate::MockURLRequestDelegate() {}
MockURLRequestDelegate::~MockURLRequestDelegate() {}

void MockURLRequestDelegate::OnResponseStarted(net::URLRequest* request) {}
void MockURLRequestDelegate::OnReadCompleted(net::URLRequest* request,
                                             int bytes_read) {}
const std::string& MockURLRequestDelegate::response_data() const {
  return response_data_;
}

const net::IOBufferWithSize* MockURLRequestDelegate::metadata() const {
  return nullptr;
}

}  // namespace headless
