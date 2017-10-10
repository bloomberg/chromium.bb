// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"
#include "components/payments/core/journey_logger.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "third_party/WebKit/public/platform/modules/payments/payment_request.mojom.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace payments {

class ContentPaymentRequestDelegate;
class PaymentRequestWebContentsManager;

// This class manages the interaction between the renderer (through the
// PaymentRequestClient and Mojo stub implementation) and the UI (through the
// PaymentRequestDelegate). The API user (merchant) specification (supported
// payment methods, required information, order details) is stored in
// PaymentRequestSpec, and the current user selection state (and related data)
// is stored in PaymentRequestSpec.
class PaymentRequest : public mojom::PaymentRequest,
                       public PaymentRequestSpec::Observer,
                       public PaymentRequestState::Delegate {
 public:
  class ObserverForTest {
   public:
    virtual void OnCanMakePaymentCalled() = 0;
    virtual void OnCanMakePaymentReturned() = 0;
    virtual void OnNotSupportedError() = 0;
    virtual void OnConnectionTerminated() = 0;
    virtual void OnAbortCalled() = 0;

   protected:
    virtual ~ObserverForTest() {}
  };

  PaymentRequest(content::RenderFrameHost* render_frame_host,
                 content::WebContents* web_contents,
                 std::unique_ptr<ContentPaymentRequestDelegate> delegate,
                 PaymentRequestWebContentsManager* manager,
                 mojo::InterfaceRequest<mojom::PaymentRequest> request,
                 ObserverForTest* observer_for_testing);
  ~PaymentRequest() override;

  // mojom::PaymentRequest
  void Init(mojom::PaymentRequestClientPtr client,
            std::vector<mojom::PaymentMethodDataPtr> method_data,
            mojom::PaymentDetailsPtr details,
            mojom::PaymentOptionsPtr options) override;
  void Show() override;
  void UpdateWith(mojom::PaymentDetailsPtr details) override;
  void Abort() override;
  void Complete(mojom::PaymentComplete result) override;
  void CanMakePayment() override;

  // PaymentRequestSpec::Observer:
  void OnSpecUpdated() override {}

  // PaymentRequestState::Delegate:
  void OnPaymentResponseAvailable(mojom::PaymentResponsePtr response) override;
  void OnShippingOptionIdSelected(std::string shipping_option_id) override;
  void OnShippingAddressSelected(mojom::PaymentAddressPtr address) override;

  // Called when the user explicitly cancelled the flow. Will send a message
  // to the renderer which will indirectly destroy this object (through
  // OnConnectionTerminated).
  void UserCancelled();

  // Called when the frame attached to this PaymentRequest is navigating away,
  // but before the PaymentRequest is destroyed.
  void DidStartNavigation(bool is_user_initiated);

  // As a result of a browser-side error or renderer-initiated mojo channel
  // closure (e.g. there was an error on the renderer side, or payment was
  // successful), this method is called. It is responsible for cleaning up,
  // such as possibly closing the dialog.
  void OnConnectionTerminated();

  // Called when the user clicks on the "Pay" button.
  void Pay();

  content::WebContents* web_contents() { return web_contents_; }

  PaymentRequestSpec* spec() { return spec_.get(); }
  PaymentRequestState* state() { return state_.get(); }

 private:
  // Only records the abort reason if it's the first completion for this Payment
  // Request. This is necessary since the aborts cascade into one another with
  // the first one being the most precise.
  void RecordFirstAbortReason(JourneyLogger::AbortReason completion_status);

  // The PaymentRequestState::CanMakePaymentCallback. Checks for query quota and
  // may send QUERY_QUOTA_EXCEEDED.
  void CanMakePaymentCallback(bool can_make_payment);

  // Sends either CAN_MAKE_PAYMENT or CANNOT_MAKE_PAYMENT to the renderer,
  // depending on |can_make_payment| value. Never sends QUERY_QUOTA_EXCEEDED.
  // Does not check query quota, but does check for incognito mode. If
  // |warn_localhost_or_file| is true, then sends WARNING_CAN_MAKE_PAYMENT or
  // WARNING_CANNOT_MAKE_PAYMENT version of the values instead.
  void RespondToCanMakePaymentQuery(bool can_make_payment,
                                    bool warn_localhost_or_file);

  content::WebContents* web_contents_;
  std::unique_ptr<ContentPaymentRequestDelegate> delegate_;
  // |manager_| owns this PaymentRequest.
  PaymentRequestWebContentsManager* manager_;
  mojo::Binding<mojom::PaymentRequest> binding_;
  mojom::PaymentRequestClientPtr client_;

  std::unique_ptr<PaymentRequestSpec> spec_;
  std::unique_ptr<PaymentRequestState> state_;

  // The RFC 6454 origin of the top level frame that has invoked PaymentRequest
  // API. This is what the user sees in the address bar.
  const GURL top_level_origin_;

  // The RFC 6454 origin of the frame that has invoked PaymentRequest API. This
  // can be either the main frame or an iframe.
  const GURL frame_origin_;

  // May be null, must outlive this object.
  ObserverForTest* observer_for_testing_;

  JourneyLogger journey_logger_;

  // Whether a completion was already recorded for this Payment Request.
  bool has_recorded_completion_ = false;

  base::WeakPtrFactory<PaymentRequest> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequest);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_H_
