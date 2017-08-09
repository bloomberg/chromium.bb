// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PAYMENT_APP_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_PAYMENT_APP_PROVIDER_H_

#include <stdint.h>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/stored_payment_app.h"
#include "third_party/WebKit/public/platform/modules/payments/payment_app.mojom.h"

namespace content {

class BrowserContext;

// This is providing the service worker based payment app related APIs to
// Chrome layer. This class is a singleton, the instance of which can be
// retrieved using the static GetInstance() method.
// All methods must be called on the UI thread.
//
// Design Doc:
//   https://docs.google.com/document/d/1rWsvKQAwIboN2ZDuYYAkfce8GF27twi4UHTt0hcbyxQ/edit?usp=sharing
class CONTENT_EXPORT PaymentAppProvider {
 public:
  // This static function is actually implemented in PaymentAppProviderImpl.cc.
  // Please see: content/browser/payments/payment_app_provider_impl.cc
  static PaymentAppProvider* GetInstance();

  using PaymentApps = std::map<int64_t, std::unique_ptr<StoredPaymentApp>>;
  using GetAllPaymentAppsCallback = base::OnceCallback<void(PaymentApps)>;
  using InvokePaymentAppCallback =
      base::OnceCallback<void(payments::mojom::PaymentHandlerResponsePtr)>;
  using PaymentEventResultCallback = base::OnceCallback<void(bool)>;

  // Should be accessed only on the UI thread.
  virtual void GetAllPaymentApps(BrowserContext* browser_context,
                                 GetAllPaymentAppsCallback callback) = 0;
  virtual void InvokePaymentApp(
      BrowserContext* browser_context,
      int64_t registration_id,
      payments::mojom::PaymentRequestEventDataPtr event_data,
      InvokePaymentAppCallback callback) = 0;
  virtual void CanMakePayment(
      BrowserContext* browser_context,
      int64_t registration_id,
      payments::mojom::CanMakePaymentEventDataPtr event_data,
      PaymentEventResultCallback callback) = 0;
  virtual void AbortPayment(BrowserContext* browser_context,
                            int64_t registration_id,
                            PaymentEventResultCallback callback) = 0;

 protected:
  virtual ~PaymentAppProvider() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PAYMENT_APP_PROVIDER_H_
