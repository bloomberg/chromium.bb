// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/restricted_cookie_manager_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"

namespace content {

RestrictedCookieManagerImpl::RestrictedCookieManagerImpl(
    net::CookieStore* cookie_store,
    int render_process_id,
    int render_frame_id)
    : cookie_store_(cookie_store),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      weak_ptr_factory_(this) {}

RestrictedCookieManagerImpl::~RestrictedCookieManagerImpl() = default;

void RestrictedCookieManagerImpl::GetAllForUrl(
    const GURL& url,
    const GURL& site_for_cookies,
    network::mojom::CookieManagerGetOptionsPtr options,
    GetAllForUrlCallback callback) {
  // TODO(pwnall): Replicate the call to
  //               ChildProcessSecurityPolicy::CanAccessDataForOrigin() in
  //               RenderFrameMessageFilter::GetCookies.

  net::CookieOptions net_options;
  if (net::registry_controlled_domains::SameDomainOrHost(
          url, site_for_cookies,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    // TODO(mkwst): This check ought to further distinguish between frames
    // initiated in a strict or lax same-site context.
    net_options.set_same_site_cookie_mode(
        net::CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
  } else {
    net_options.set_same_site_cookie_mode(
        net::CookieOptions::SameSiteCookieMode::DO_NOT_INCLUDE);
  }

  cookie_store_->GetCookieListWithOptionsAsync(
      url, net_options,
      base::BindOnce(
          &RestrictedCookieManagerImpl::CookieListToGetAllForUrlCallback,
          weak_ptr_factory_.GetWeakPtr(), url, site_for_cookies,
          std::move(options), std::move(callback)));
}

void RestrictedCookieManagerImpl::CookieListToGetAllForUrlCallback(
    const GURL& url,
    const GURL& site_for_cookies,
    network::mojom::CookieManagerGetOptionsPtr options,
    GetAllForUrlCallback callback,
    const net::CookieList& cookie_list) {
  // TODO(pwnall): Replicate the security checks in
  //               RenderFrameMessageFilter::CheckPolicyForCookies

  // Avoid unused member warnings until these are used for security checks.
  (void)(render_frame_id_);
  (void)(render_process_id_);

  std::vector<net::CanonicalCookie> result;
  result.reserve(cookie_list.size());
  for (size_t i = 0; i < cookie_list.size(); ++i) {
    // TODO(pwnall): Parsing and canonicalization for net::CanonicalCookie.
    const net::CanonicalCookie& cookie = cookie_list[i];
    // TODO(pwnall): Check cookie against options.
    result.emplace_back(cookie);
  }
  std::move(callback).Run(std::move(result));
}

void RestrictedCookieManagerImpl::SetCanonicalCookie(
    const net::CanonicalCookie& cookie,
    const GURL& url,
    const GURL& site_for_cookies,
    SetCanonicalCookieCallback callback) {
  // TODO(pwnall): Replicate the call to
  //               ChildProcessSecurityPolicy::CanAccessDataForOrigin() in
  //               RenderFrameMessageFilter::SetCookie.

  // TODO(pwnall): Validate the CanonicalCookie fields.
  // TODO(pwnall): Replicate the AllowSetCookie check in
  //               RenderFrameMessageFilter::SetCookie.
  base::Time now = base::Time::NowFromSystemTime();
  // TODO(pwnall): Reason about whether it makes sense to allow a renderer to
  //               set these fields.
  net::CookieSameSite cookie_same_site_mode = net::CookieSameSite::STRICT_MODE;
  net::CookiePriority cookie_priority = net::COOKIE_PRIORITY_DEFAULT;
  auto sanitized_cookie = base::MakeUnique<net::CanonicalCookie>(
      cookie.Name(), cookie.Value(), cookie.Domain(), cookie.Path(), now,
      cookie.ExpiryDate(), now, cookie.IsSecure(), cookie.IsHttpOnly(),
      cookie_same_site_mode, cookie_priority);

  // TODO(pwnall): secure_source should depend on url, and might depend on the
  //               renderer.
  bool secure_source = true;
  bool modify_http_only = false;
  cookie_store_->SetCanonicalCookieAsync(std::move(sanitized_cookie),
                                         secure_source, modify_http_only,
                                         std::move(callback));
}

}  // namespace content
