// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_PROVIDER_IMPL_H_

#include <string>

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/payments/content/payment_app.mojom.h"
#include "content/common/content_export.h"
#include "content/public/browser/payment_app_provider.h"

namespace content {

class BrowserContext;

class CONTENT_EXPORT PaymentAppProviderImpl : public PaymentAppProvider {
 public:
  static PaymentAppProviderImpl* GetInstance();

  // PaymentAppProvider implementation:
  // Should be accessed only on the UI thread.
  void GetAllManifests(BrowserContext* browser_context,
                       const GetAllManifestsCallback& callback) override;
  void InvokePaymentApp(
      BrowserContext* browser_context,
      int64_t registration_id,
      payments::mojom::PaymentAppRequestPtr app_request) override;

 private:
  PaymentAppProviderImpl();
  ~PaymentAppProviderImpl() override;

  friend struct base::DefaultSingletonTraits<PaymentAppProviderImpl>;

  DISALLOW_COPY_AND_ASSIGN(PaymentAppProviderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_PROVIDER_IMPL_H_
