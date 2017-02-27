// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_PAYMENT_REQUEST_WEB_CONTENTS_MANAGER_H_
#define COMPONENTS_PAYMENTS_PAYMENT_REQUEST_WEB_CONTENTS_MANAGER_H_

#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "components/payments/content/payment_request.mojom.h"
#include "content/public/browser/web_contents_user_data.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace content {
class WebContents;
}

namespace payments {

class PaymentRequest;
class PaymentRequestDelegate;

// This class owns the PaymentRequest associated with a given WebContents.
//
// Responsible for creating PaymentRequest's and retaining ownership. No request
// pointers are currently available because the request manages its interactions
// with UI and renderer. The PaymentRequest may call DestroyRequest() to signal
// it is ready to die. Otherwise it gets destroyed when the WebContents (thus
// this class) goes away.
class PaymentRequestWebContentsManager
    : public content::WebContentsUserData<PaymentRequestWebContentsManager> {
 public:
  ~PaymentRequestWebContentsManager() override;

  // Retrieves the instance of PaymentRequestWebContentsManager that was
  // attached to the specified WebContents.  If no instance was attached,
  // creates one, and attaches it to the specified WebContents.
  static PaymentRequestWebContentsManager* GetOrCreateForWebContents(
      content::WebContents* web_contents);

  // Creates the PaymentRequest that will interact with this |web_contents|.
  void CreatePaymentRequest(
      content::WebContents* web_contents,
      std::unique_ptr<PaymentRequestDelegate> delegate,
      mojo::InterfaceRequest<payments::mojom::PaymentRequest> request);

  // Destroys the given |request|.
  void DestroyRequest(PaymentRequest* request);

 private:
  explicit PaymentRequestWebContentsManager(content::WebContents* web_contents);
  friend class content::WebContentsUserData<PaymentRequestWebContentsManager>;
  friend class PaymentRequestInteractiveTestBase;

  // Owns all the PaymentRequest for this WebContents. Since the
  // PaymentRequestWebContentsManager's lifetime is tied to the WebContents,
  // these requests only get destroyed when the WebContents goes away, or when
  // the requests themselves call DestroyRequest().
  std::unordered_map<PaymentRequest*, std::unique_ptr<PaymentRequest>>
      payment_requests_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestWebContentsManager);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_PAYMENT_REQUEST_WEB_CONTENTS_MANAGER_H_
