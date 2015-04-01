// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_delegate.h"

#include "base/logging.h"
#include "base/profiler/scoped_tracker.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/proxy/proxy_info.h"
#include "net/url_request/url_request.h"

namespace net {

int NetworkDelegate::NotifyBeforeURLRequest(
    URLRequest* request, const CompletionCallback& callback,
    GURL* new_url) {
  DCHECK(CalledOnValidThread());
  DCHECK(request);
  DCHECK(!callback.is_null());
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnBeforeURLRequest"));
  return OnBeforeURLRequest(request, callback, new_url);
}

void NetworkDelegate::NotifyResolveProxy(
    const GURL& url,
    int load_flags,
    const ProxyService& proxy_service,
    ProxyInfo* result) {
  DCHECK(CalledOnValidThread());
  DCHECK(result);
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnResolveProxy"));
  OnResolveProxy(url, load_flags, proxy_service, result);
}

void NetworkDelegate::NotifyProxyFallback(
    const ProxyServer& bad_proxy,
    int net_error) {
  DCHECK(CalledOnValidThread());
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnProxyFallback"));
  OnProxyFallback(bad_proxy, net_error);
}

int NetworkDelegate::NotifyBeforeSendHeaders(
    URLRequest* request, const CompletionCallback& callback,
    HttpRequestHeaders* headers) {
  DCHECK(CalledOnValidThread());
  DCHECK(headers);
  DCHECK(!callback.is_null());
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnBeforeSendHeaders"));
  return OnBeforeSendHeaders(request, callback, headers);
}

void NetworkDelegate::NotifyBeforeSendProxyHeaders(
    URLRequest* request,
    const ProxyInfo& proxy_info,
    HttpRequestHeaders* headers) {
  DCHECK(CalledOnValidThread());
  DCHECK(headers);
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnBeforeSendProxyHeaders"));
  OnBeforeSendProxyHeaders(request, proxy_info, headers);
}

void NetworkDelegate::NotifySendHeaders(URLRequest* request,
                                        const HttpRequestHeaders& headers) {
  DCHECK(CalledOnValidThread());
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnSendHeaders"));
  OnSendHeaders(request, headers);
}

int NetworkDelegate::NotifyHeadersReceived(
    URLRequest* request,
    const CompletionCallback& callback,
    const HttpResponseHeaders* original_response_headers,
    scoped_refptr<HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  DCHECK(CalledOnValidThread());
  DCHECK(original_response_headers);
  DCHECK(!callback.is_null());
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnHeadersReceived"));
  return OnHeadersReceived(request,
                           callback,
                           original_response_headers,
                           override_response_headers,
                           allowed_unsafe_redirect_url);
}

void NetworkDelegate::NotifyResponseStarted(URLRequest* request) {
  DCHECK(CalledOnValidThread());
  DCHECK(request);
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnResponseStarted"));
  OnResponseStarted(request);
}

void NetworkDelegate::NotifyRawBytesRead(const URLRequest& request,
                                         int bytes_read) {
  DCHECK(CalledOnValidThread());
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnRawBytesRead"));
  OnRawBytesRead(request, bytes_read);
}

void NetworkDelegate::NotifyBeforeRedirect(URLRequest* request,
                                           const GURL& new_location) {
  DCHECK(CalledOnValidThread());
  DCHECK(request);
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnBeforeRedirect"));
  OnBeforeRedirect(request, new_location);
}

void NetworkDelegate::NotifyCompleted(URLRequest* request, bool started) {
  DCHECK(CalledOnValidThread());
  DCHECK(request);
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION("423948 NetworkDelegate::OnCompleted"));
  OnCompleted(request, started);
}

void NetworkDelegate::NotifyURLRequestDestroyed(URLRequest* request) {
  DCHECK(CalledOnValidThread());
  DCHECK(request);
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnURLRequestDestroyed"));
  OnURLRequestDestroyed(request);
}

void NetworkDelegate::NotifyPACScriptError(int line_number,
                                           const base::string16& error) {
  DCHECK(CalledOnValidThread());
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnPACScriptError"));
  OnPACScriptError(line_number, error);
}

NetworkDelegate::AuthRequiredResponse NetworkDelegate::NotifyAuthRequired(
    URLRequest* request,
    const AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    AuthCredentials* credentials) {
  DCHECK(CalledOnValidThread());
  return OnAuthRequired(request, auth_info, callback, credentials);
}

bool NetworkDelegate::CanGetCookies(const URLRequest& request,
                                    const CookieList& cookie_list) {
  DCHECK(CalledOnValidThread());
  DCHECK(!(request.load_flags() & net::LOAD_DO_NOT_SEND_COOKIES));
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnCanGetCookies"));
  return OnCanGetCookies(request, cookie_list);
}

bool NetworkDelegate::CanSetCookie(const URLRequest& request,
                                   const std::string& cookie_line,
                                   CookieOptions* options) {
  DCHECK(CalledOnValidThread());
  DCHECK(!(request.load_flags() & net::LOAD_DO_NOT_SAVE_COOKIES));
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnCanSetCookie"));
  return OnCanSetCookie(request, cookie_line, options);
}

bool NetworkDelegate::CanAccessFile(const URLRequest& request,
                                    const base::FilePath& path) const {
  DCHECK(CalledOnValidThread());
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnCanAccessFile"));
  return OnCanAccessFile(request, path);
}

bool NetworkDelegate::CanThrottleRequest(const URLRequest& request) const {
  DCHECK(CalledOnValidThread());
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnCanThrottleRequest"));
  return OnCanThrottleRequest(request);
}

bool NetworkDelegate::CanEnablePrivacyMode(
    const GURL& url,
    const GURL& first_party_for_cookies) const {
  DCHECK(CalledOnValidThread());
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnCanEnablePrivacyMode"));
  return OnCanEnablePrivacyMode(url, first_party_for_cookies);
}

bool NetworkDelegate::FirstPartyOnlyCookieExperimentEnabled() const {
  return OnFirstPartyOnlyCookieExperimentEnabled();
}

bool NetworkDelegate::CancelURLRequestWithPolicyViolatingReferrerHeader(
    const URLRequest& request,
    const GURL& target_url,
    const GURL& referrer_url) const {
  DCHECK(CalledOnValidThread());
  // TODO(vadimt): Remove ScopedTracker below once crbug.com/423948 is fixed.
  tracked_objects::ScopedTracker tracking_profile(
      FROM_HERE_WITH_EXPLICIT_FUNCTION(
          "423948 NetworkDelegate::OnCancelURLRequestWithPolicy..."));
  return OnCancelURLRequestWithPolicyViolatingReferrerHeader(
      request, target_url, referrer_url);
}

}  // namespace net
