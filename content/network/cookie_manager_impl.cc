// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/network/cookie_manager_impl.h"

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
  explicit PredicateWrapper(mojom::CookieDeletionFilterPtr filter)
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

  bool Predicate(const net::CanonicalCookie& cookie) {
    // Ignore begin/end times; they're handled by method args.
    bool result = true;
    if (use_excluding_domains_)
      result &= (excluding_domains_.count(cookie.Domain()) == 0);

    if (use_including_domains_)
      result &= (including_domains_.count(cookie.Domain()) != 0);

    if (session_control_ !=
        mojom::CookieDeletionSessionControl::IGNORE_CONTROL) {
      // Relies on the assumption that there are only three values possible for
      // session_control.
      result &= (cookie.IsPersistent() ==
                 (session_control_ ==
                  mojom::CookieDeletionSessionControl::PERSISTENT_COOKIES));
    }
    return result;
  }

 private:
  bool use_excluding_domains_;
  std::set<std::string> excluding_domains_;

  bool use_including_domains_;
  std::set<std::string> including_domains_;

  mojom::CookieDeletionSessionControl session_control_;

  DISALLOW_COPY_AND_ASSIGN(PredicateWrapper);
};

mojom::CookieChangeCause ChangeCauseTranslation(
    net::CookieStore::ChangeCause net_cause) {
  switch (net_cause) {
    case net::CookieStore::ChangeCause::INSERTED:
      return mojom::CookieChangeCause::INSERTED;
    case net::CookieStore::ChangeCause::EXPLICIT:
    case net::CookieStore::ChangeCause::EXPLICIT_DELETE_BETWEEN:
    case net::CookieStore::ChangeCause::EXPLICIT_DELETE_PREDICATE:
    case net::CookieStore::ChangeCause::EXPLICIT_DELETE_SINGLE:
    case net::CookieStore::ChangeCause::EXPLICIT_DELETE_CANONICAL:
      return mojom::CookieChangeCause::EXPLICIT;
    case net::CookieStore::ChangeCause::UNKNOWN_DELETION:
      return mojom::CookieChangeCause::UNKNOWN_DELETION;
    case net::CookieStore::ChangeCause::OVERWRITE:
      return mojom::CookieChangeCause::OVERWRITE;
    case net::CookieStore::ChangeCause::EXPIRED:
      return mojom::CookieChangeCause::EXPIRED;
    case net::CookieStore::ChangeCause::EVICTED:
      return mojom::CookieChangeCause::EVICTED;
    case net::CookieStore::ChangeCause::EXPIRED_OVERWRITE:
      return mojom::CookieChangeCause::EXPIRED_OVERWRITE;
  }
  NOTREACHED();
  return mojom::CookieChangeCause::EXPLICIT;
}

}  // namespace

CookieManagerImpl::NotificationRegistration::NotificationRegistration() {}

CookieManagerImpl::NotificationRegistration::~NotificationRegistration() {}

CookieManagerImpl::CookieManagerImpl(net::CookieStore* cookie_store)
    : cookie_store_(cookie_store) {}

CookieManagerImpl::~CookieManagerImpl() {}

void CookieManagerImpl::AddRequest(mojom::CookieManagerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void CookieManagerImpl::GetAllCookies(GetAllCookiesCallback callback) {
  cookie_store_->GetAllCookiesAsync(std::move(callback));
}

void CookieManagerImpl::GetCookieList(const GURL& url,
                                      const net::CookieOptions& cookie_options,
                                      GetCookieListCallback callback) {
  cookie_store_->GetCookieListWithOptionsAsync(url, cookie_options,
                                               std::move(callback));
}

void CookieManagerImpl::SetCanonicalCookie(
    const net::CanonicalCookie& cookie,
    bool secure_source,
    bool modify_http_only,
    SetCanonicalCookieCallback callback) {
  cookie_store_->SetCanonicalCookieAsync(
      base::MakeUnique<net::CanonicalCookie>(cookie), secure_source,
      modify_http_only, std::move(callback));
}

void CookieManagerImpl::DeleteCookies(mojom::CookieDeletionFilterPtr filter,
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
                 base::MakeUnique<PredicateWrapper>(std::move(filter))),
      std::move(callback));
}

void CookieManagerImpl::RequestNotification(
    const GURL& url,
    const std::string& name,
    mojom::CookieChangeNotificationPtr notification_pointer) {
  std::unique_ptr<NotificationRegistration> notification_registration(
      base::MakeUnique<NotificationRegistration>());
  notification_registration->notification_pointer =
      std::move(notification_pointer);

  notification_registration->subscription = cookie_store_->AddCallbackForCookie(
      url, name,
      base::Bind(&CookieManagerImpl::CookieChanged,
                 // base::Unretained is safe as destruction of the
                 // CookieManagerImpl will also destroy the
                 // notifications_registered list (which this object will be
                 // inserted into, below), which will destroy the
                 // CookieChangedSubscription, unregistering the callback.
                 base::Unretained(this),
                 // base::Unretained is safe as destruction of the
                 // NotificationRegistration will also destroy the
                 // CookieChangedSubscription, unregistering the callback.
                 base::Unretained(notification_registration.get())));

  notification_registration->notification_pointer.set_connection_error_handler(
      base::BindOnce(&CookieManagerImpl::NotificationPipeBroken,
                     // base::Unretained is safe as destruction of the
                     // CookieManagerImpl will also destroy the
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

void CookieManagerImpl::CookieChanged(NotificationRegistration* registration,
                                      const net::CanonicalCookie& cookie,
                                      net::CookieStore::ChangeCause cause) {
  registration->notification_pointer->OnCookieChanged(
      cookie, ChangeCauseTranslation(cause));
}

void CookieManagerImpl::NotificationPipeBroken(
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

void CookieManagerImpl::CloneInterface(
    mojom::CookieManagerRequest new_interface) {
  AddRequest(std::move(new_interface));
}

}  // namespace content
