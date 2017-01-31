// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PAYMENT_APP_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_PAYMENT_APP_PROVIDER_H_

#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "content/common/content_export.h"

namespace content {

class BrowserContext;

// This is providing the service worker based payment app related APIs to
// Chrome layer. This class is a singleton, the instance of which can be
// retrieved using the static GetInstance() method.
// All methods must be called on the UI thread.
class CONTENT_EXPORT PaymentAppProvider {
 public:
  // This static function is actually implemented in PaymentAppProviderImpl.cc.
  // Please see: content/browser/payments/payment_app_provider_impl.cc
  static PaymentAppProvider* GetInstance();

  // The ManifestWithID is a pair of the service worker registration id and
  // the payment app manifest data associated with it.
  using ManifestWithID =
      std::pair<int64_t, payments::mojom::PaymentAppManifestPtr>;
  using Manifests = std::vector<ManifestWithID>;
  using GetAllManifestsCallback = base::Callback<void(Manifests)>;

  // Should be accessed only on the UI thread.
  virtual void GetAllManifests(BrowserContext* browser_context,
                               const GetAllManifestsCallback& callback) = 0;
  virtual void InvokePaymentApp(
      BrowserContext* browser_context,
      int64_t registration_id,
      payments::mojom::PaymentAppRequestPtr app_request) = 0;

 protected:
  virtual ~PaymentAppProvider() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PAYMENT_APP_PROVIDER_H_
