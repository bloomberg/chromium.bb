// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_PROVIDER_IMPL_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "content/common/content_export.h"
#include "content/public/browser/payment_app_provider.h"

namespace content {

class CONTENT_EXPORT PaymentAppProviderImpl : public PaymentAppProvider {
 public:
  static PaymentAppProviderImpl* GetInstance();

  // PaymentAppProvider implementation:
  // Should be accessed only on the UI thread.
  void GetAllPaymentApps(BrowserContext* browser_context,
                         GetAllPaymentAppsCallback callback) override;
  void InvokePaymentApp(BrowserContext* browser_context,
                        int64_t registration_id,
                        payments::mojom::PaymentRequestEventDataPtr event_data,
                        InvokePaymentAppCallback callback) override;
  void CanMakePayment(BrowserContext* browser_context,
                      int64_t registration_id,
                      payments::mojom::CanMakePaymentEventDataPtr event_data,
                      PaymentEventResultCallback callback) override;
  void AbortPayment(BrowserContext* browser_context,
                    int64_t registration_id,
                    PaymentEventResultCallback callback) override;

 private:
  PaymentAppProviderImpl();
  ~PaymentAppProviderImpl() override;

  friend struct base::DefaultSingletonTraits<PaymentAppProviderImpl>;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppProviderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_PROVIDER_IMPL_H_
