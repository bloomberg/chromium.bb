// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_MANAGER_H_
#define CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_MANAGER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_app.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "url/gurl.h"

namespace content {

class PaymentAppContextImpl;

class CONTENT_EXPORT PaymentAppManager
    : public NON_EXPORTED_BASE(payments::mojom::PaymentAppManager) {
 public:
  PaymentAppManager(
      PaymentAppContextImpl* payment_app_context,
      mojo::InterfaceRequest<payments::mojom::PaymentAppManager> request);

  ~PaymentAppManager() override;

 private:
  friend class PaymentAppContentUnitTestBase;

  // payments::mojom::PaymentAppManager methods:
  void Init(const std::string& scope) override;
  void SetManifest(payments::mojom::PaymentAppManifestPtr manifest,
                   const SetManifestCallback& callback) override;
  void GetManifest(const GetManifestCallback& callback) override;

  // Called when an error is detected on binding_.
  void OnConnectionError();

  // PaymentAppContextImpl owns PaymentAppManager
  PaymentAppContextImpl* payment_app_context_;

  GURL scope_;
  mojo::Binding<payments::mojom::PaymentAppManager> binding_;
  base::WeakPtrFactory<PaymentAppManager> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(PaymentAppManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PAYMENTS_PAYMENT_APP_MANAGER_H_
