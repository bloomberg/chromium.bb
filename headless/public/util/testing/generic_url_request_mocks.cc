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

MockGenericURLRequestJobDelegate::~MockGenericURLRequestJobDelegate() = default;

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
MockCookieStore::MockCookieStore() = default;
MockCookieStore::~MockCookieStore() = default;

void MockCookieStore::SetCookieWithOptionsAsync(
    const GURL& url,
    const std::string& cookie_line,
    const net::CookieOptions& options,
    SetCookiesCallback callback) {
  CHECK(false);
}

void MockCookieStore::SetCanonicalCookieAsync(
    std::unique_ptr<net::CanonicalCookie> cookie,
    bool secure_source,
    bool can_modify_httponly,
    SetCookiesCallback callback) {
  cookies_.push_back(std::move(*cookie));
}

void MockCookieStore::GetCookiesWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookiesCallback callback) {
  CHECK(false);
}

void MockCookieStore::GetCookieListWithOptionsAsync(
    const GURL& url,
    const net::CookieOptions& options,
    GetCookieListCallback callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockCookieStore::SendCookies, base::Unretained(this), url,
                     options, std::move(callback)));
}

void MockCookieStore::GetAllCookiesAsync(GetCookieListCallback callback) {
  CHECK(false);
}

void MockCookieStore::DeleteCookieAsync(const GURL& url,
                                        const std::string& cookie_name,
                                        base::OnceClosure callback) {
  CHECK(false);
}

void MockCookieStore::DeleteCanonicalCookieAsync(
    const net::CanonicalCookie& cookie,
    DeleteCallback callback) {
  CHECK(false);
}

void MockCookieStore::DeleteAllCreatedBetweenAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    DeleteCallback callback) {
  CHECK(false);
}

void MockCookieStore::DeleteAllCreatedBetweenWithPredicateAsync(
    const base::Time& delete_begin,
    const base::Time& delete_end,
    const CookiePredicate& predicate,
    DeleteCallback callback) {
  CHECK(false);
}

void MockCookieStore::DeleteSessionCookiesAsync(DeleteCallback) {
  CHECK(false);
}

void MockCookieStore::FlushStore(base::OnceClosure callback) {
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

std::unique_ptr<net::CookieStore::CookieChangedSubscription>
MockCookieStore::AddCallbackForAllChanges(
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
                                  GetCookieListCallback callback) {
  net::CookieList result;
  for (const auto& cookie : cookies_) {
    if (cookie.IncludeForRequestURL(url, options))
      result.push_back(cookie);
  }
  std::move(callback).Run(result);
}

// MockURLRequestDelegate
MockURLRequestDelegate::MockURLRequestDelegate() = default;
MockURLRequestDelegate::~MockURLRequestDelegate() = default;

void MockURLRequestDelegate::OnResponseStarted(net::URLRequest* request,
                                               int net_error) {}
void MockURLRequestDelegate::OnReadCompleted(net::URLRequest* request,
                                             int bytes_read) {}
const std::string& MockURLRequestDelegate::response_data() const {
  return response_data_;
}

const net::IOBufferWithSize* MockURLRequestDelegate::metadata() const {
  return nullptr;
}

}  // namespace headless
