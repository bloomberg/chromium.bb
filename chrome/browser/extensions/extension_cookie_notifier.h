// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIE_NOTIFIER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIE_NOTIFIER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/network/public/interfaces/cookie_manager.mojom.h"

class Profile;

namespace net {
class CanonicalCookie;
}

// Sends cookie-change notifications on the UI thread via
// chrome::NOTIFICATION_COOKIE_CHANGED_FOR_EXTENSIONS for all cookie
// changes associated with the given profile.
class ExtensionCookieNotifier
    : public network::mojom::CookieChangeNotification {
 public:
  explicit ExtensionCookieNotifier(Profile* profile);
  ~ExtensionCookieNotifier() override;

 private:
  // network::mojom::CookieChangeNotification implementation.
  void OnCookieChanged(const net::CanonicalCookie& cookie,
                       network::mojom::CookieChangeCause cause) override;

  Profile* profile_;
  std::unique_ptr<mojo::Binding<network::mojom::CookieChangeNotification>>
      binding_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionCookieNotifier);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIE_NOTIFIER_H_
