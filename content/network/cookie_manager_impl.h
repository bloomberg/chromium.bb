// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_NETWORK_COOKIE_MANAGER_IMPL_H_
#define CONTENT_NETWORK_COOKIE_MANAGER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/cookies/cookie_store.h"
#include "services/network/public/interfaces/cookie_manager.mojom.h"

class GURL;

namespace content {

// Wrap a cookie store in an implementation of the mojo cookie interface.

// This is an IO thread object; all methods on this object must be called on
// the IO thread.  Note that this does not restrict the locations from which
// mojo messages may be sent to the object.
class CONTENT_EXPORT CookieManagerImpl : public network::mojom::CookieManager {
 public:
  // Construct a CookieService that can serve mojo requests for the underlying
  // cookie store.  |*cookie_store| must outlive this object.
  explicit CookieManagerImpl(net::CookieStore* cookie_store);

  ~CookieManagerImpl() override;

  // Bind a cookie request to this object.  Mojo messages
  // coming through the associated pipe will be served by this object.
  void AddRequest(network::mojom::CookieManagerRequest request);

  // TODO(rdsmith): Add a verion of AddRequest that does renderer-appropriate
  // security checks on bindings coming through that interface.

  // mojom::CookieManager
  void GetAllCookies(GetAllCookiesCallback callback) override;
  void GetCookieList(const GURL& url,
                     const net::CookieOptions& cookie_options,
                     GetCookieListCallback callback) override;
  void SetCanonicalCookie(const net::CanonicalCookie& cookie,
                          bool secure_source,
                          bool modify_http_only,
                          SetCanonicalCookieCallback callback) override;
  void DeleteCookies(network::mojom::CookieDeletionFilterPtr filter,
                     DeleteCookiesCallback callback) override;
  void RequestNotification(const GURL& url,
                           const std::string& name,
                           network::mojom::CookieChangeNotificationPtr
                               notification_pointer) override;
  void CloneInterface(
      network::mojom::CookieManagerRequest new_interface) override;

  uint32_t GetClientsBoundForTesting() const { return bindings_.size(); }
  uint32_t GetNotificationsBoundForTesting() const {
    return notifications_registered_.size();
  }

 private:
  struct NotificationRegistration {
    NotificationRegistration();
    ~NotificationRegistration();

    // Owns the callback registration in the store.
    std::unique_ptr<net::CookieStore::CookieChangedSubscription> subscription;

    // Pointer on which to send notifications.
    network::mojom::CookieChangeNotificationPtr notification_pointer;

    DISALLOW_COPY_AND_ASSIGN(NotificationRegistration);
  };

  // Used to hook callbacks
  void CookieChanged(NotificationRegistration* registration,
                     const net::CanonicalCookie& cookie,
                     net::CookieStore::ChangeCause cause);

  // Handles connection errors on notification pipes.
  void NotificationPipeBroken(NotificationRegistration* registration);

  net::CookieStore* const cookie_store_;
  mojo::BindingSet<network::mojom::CookieManager> bindings_;
  std::vector<std::unique_ptr<NotificationRegistration>>
      notifications_registered_;

  DISALLOW_COPY_AND_ASSIGN(CookieManagerImpl);
};

}  // namespace content

#endif  // CONTENT_NETWORK_COOKIE_MANAGER_IMPL_H_
