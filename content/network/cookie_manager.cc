// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/cookie_manager.h"

#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_options.h"
#include "url/gurl.h"

namespace content {

namespace {

// Class to wrap a CookieDeletionFilterPtr and provide a predicate for
// use by DeleteAllCreatedBetweenWithPredicateAsync.
class PredicateWrapper {
 public:
  explicit PredicateWrapper(network::mojom::CookieDeletionFilterPtr filter)
      : use_excluding_domains_(filter->excluding_domains.has_value()),
        excluding_domains_(filter->excluding_domains.has_value()
                               ? std::set<std::string>(
                                     filter->excluding_domains.value().begin(),
                                     filter->excluding_domains.value().end())
                               : std::set<std::string>()),
        use_including_domains_(filter->including_domains.has_value()),
        including_domains_(filter->including_domains.has_value()
                               ? std::set<std::string>(
                                     filter->including_domains.value().begin(),
                                     filter->including_domains.value().end())
                               : std::set<std::string>()),
        session_control_(filter->session_control) {}

  // Return true if the given cookie should be deleted.
  bool Predicate(const net::CanonicalCookie& cookie) {
    // Ignore begin/end times; they're handled by method args.
    bool result = true;
    // Delete if the cookie is not in excluding_domains_.
    if (use_excluding_domains_)
      result &= !DomainMatches(cookie, excluding_domains_);

    // Delete if the cookie is in including_domains_.
    if (use_including_domains_)
      result &= DomainMatches(cookie, including_domains_);

    // Delete if the cookie is not the correct persistent or session type.
    if (session_control_ !=
        network::mojom::CookieDeletionSessionControl::IGNORE_CONTROL) {
      // Relies on the assumption that there are only three values possible for
      // session_control.
      result &=
          (cookie.IsPersistent() ==
           (session_control_ ==
            network::mojom::CookieDeletionSessionControl::PERSISTENT_COOKIES));
    }
    return result;
  }

 private:
  // Return true if the eTLD+1 of the domain matches any of the strings
  // in |match_domains|, false otherwise.
  bool DomainMatches(const net::CanonicalCookie& cookie,
                     const std::set<std::string>& match_domains) {
    std::string effective_domain(
        net::registry_controlled_domains::GetDomainAndRegistry(
            // GetDomainAndRegistry() is insensitive to leading dots, i.e.
            // to host/domain cookie distinctions.
            cookie.Domain(),
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
    // If the cookie's domain is is not parsed as belonging to a registry
    // (e.g. for IP addresses or internal hostnames) an empty string will be
    // returned.  In this case, use the domain in the cookie.
    if (effective_domain.empty())
      effective_domain = cookie.Domain();

    return match_domains.count(effective_domain) != 0;
  }

  bool use_excluding_domains_;
  std::set<std::string> excluding_domains_;

  bool use_including_domains_;
  std::set<std::string> including_domains_;

  network::mojom::CookieDeletionSessionControl session_control_;

  DISALLOW_COPY_AND_ASSIGN(PredicateWrapper);
};

network::mojom::CookieChangeCause ChangeCauseTranslation(
    net::CookieStore::ChangeCause net_cause) {
  switch (net_cause) {
    case net::CookieStore::ChangeCause::INSERTED:
      return network::mojom::CookieChangeCause::INSERTED;
    case net::CookieStore::ChangeCause::EXPLICIT:
    case net::CookieStore::ChangeCause::EXPLICIT_DELETE_BETWEEN:
    case net::CookieStore::ChangeCause::EXPLICIT_DELETE_PREDICATE:
    case net::CookieStore::ChangeCause::EXPLICIT_DELETE_SINGLE:
    case net::CookieStore::ChangeCause::EXPLICIT_DELETE_CANONICAL:
      return network::mojom::CookieChangeCause::EXPLICIT;
    case net::CookieStore::ChangeCause::UNKNOWN_DELETION:
      return network::mojom::CookieChangeCause::UNKNOWN_DELETION;
    case net::CookieStore::ChangeCause::OVERWRITE:
      return network::mojom::CookieChangeCause::OVERWRITE;
    case net::CookieStore::ChangeCause::EXPIRED:
      return network::mojom::CookieChangeCause::EXPIRED;
    case net::CookieStore::ChangeCause::EVICTED:
      return network::mojom::CookieChangeCause::EVICTED;
    case net::CookieStore::ChangeCause::EXPIRED_OVERWRITE:
      return network::mojom::CookieChangeCause::EXPIRED_OVERWRITE;
  }
  NOTREACHED();
  return network::mojom::CookieChangeCause::EXPLICIT;
}

}  // namespace

CookieManager::NotificationRegistration::NotificationRegistration() {}

CookieManager::NotificationRegistration::~NotificationRegistration() {}

CookieManager::CookieManager(net::CookieStore* cookie_store)
    : cookie_store_(cookie_store) {}

CookieManager::~CookieManager() {}

void CookieManager::AddRequest(network::mojom::CookieManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void CookieManager::GetAllCookies(GetAllCookiesCallback callback) {
  cookie_store_->GetAllCookiesAsync(std::move(callback));
}

void CookieManager::GetCookieList(const GURL& url,
                                  const net::CookieOptions& cookie_options,
                                  GetCookieListCallback callback) {
  cookie_store_->GetCookieListWithOptionsAsync(url, cookie_options,
                                               std::move(callback));
}

void CookieManager::SetCanonicalCookie(const net::CanonicalCookie& cookie,
                                       bool secure_source,
                                       bool modify_http_only,
                                       SetCanonicalCookieCallback callback) {
  cookie_store_->SetCanonicalCookieAsync(
      std::make_unique<net::CanonicalCookie>(cookie), secure_source,
      modify_http_only, std::move(callback));
}

void CookieManager::DeleteCookies(
    network::mojom::CookieDeletionFilterPtr filter,
    DeleteCookiesCallback callback) {
  base::Time start_time;
  base::Time end_time;

  if (filter->created_after_time.has_value())
    start_time = filter->created_after_time.value();

  if (filter->created_before_time.has_value())
    end_time = filter->created_before_time.value();

  cookie_store_->DeleteAllCreatedBetweenWithPredicateAsync(
      start_time, end_time,
      base::Bind(&PredicateWrapper::Predicate,
                 std::make_unique<PredicateWrapper>(std::move(filter))),
      std::move(callback));
}

void CookieManager::RequestNotification(
    const GURL& url,
    const std::string& name,
    network::mojom::CookieChangeNotificationPtr notification_pointer) {
  std::unique_ptr<NotificationRegistration> notification_registration(
      std::make_unique<NotificationRegistration>());
  notification_registration->notification_pointer =
      std::move(notification_pointer);

  notification_registration->subscription = cookie_store_->AddCallbackForCookie(
      url, name,
      base::Bind(&CookieManager::CookieChanged,
                 // base::Unretained is safe as destruction of the
                 // CookieManager will also destroy the
                 // notifications_registered list (which this object will be
                 // inserted into, below), which will destroy the
                 // CookieChangedSubscription, unregistering the callback.
                 base::Unretained(this),
                 // base::Unretained is safe as destruction of the
                 // NotificationRegistration will also destroy the
                 // CookieChangedSubscription, unregistering the callback.
                 base::Unretained(notification_registration.get())));

  notification_registration->notification_pointer.set_connection_error_handler(
      base::BindOnce(&CookieManager::NotificationPipeBroken,
                     // base::Unretained is safe as destruction of the
                     // CookieManager will also destroy the
                     // notifications_registered list (which this object will be
                     // inserted into, below), which will destroy the
                     // notification_pointer, rendering this callback moot.
                     base::Unretained(this),
                     // base::Unretained is safe as destruction of the
                     // NotificationRegistration will also destroy the
                     // CookieChangedSubscription, unregistering the callback.
                     base::Unretained(notification_registration.get())));

  notifications_registered_.push_back(std::move(notification_registration));
}

void CookieManager::CookieChanged(NotificationRegistration* registration,
                                  const net::CanonicalCookie& cookie,
                                  net::CookieStore::ChangeCause cause) {
  registration->notification_pointer->OnCookieChanged(
      cookie, ChangeCauseTranslation(cause));
}

void CookieManager::NotificationPipeBroken(
    NotificationRegistration* registration) {
  for (auto it = notifications_registered_.begin();
       it != notifications_registered_.end(); ++it) {
    if (it->get() == registration) {
      // It isn't expected this will be a common enough operation for
      // the performance of std::vector::erase() to matter.
      notifications_registered_.erase(it);
      return;
    }
  }
  // A broken connection error should never be raised for an unknown pipe.
  NOTREACHED();
}

void CookieManager::CloneInterface(
    network::mojom::CookieManagerRequest new_interface) {
  AddRequest(std::move(new_interface));
}

}  // namespace content
