// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_web_contents_manager.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/core/payment_request_delegate.h"
#include "content/public/browser/web_contents.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(payments::PaymentRequestWebContentsManager);

namespace payments {

PaymentRequestWebContentsManager::~PaymentRequestWebContentsManager() {}

PaymentRequestWebContentsManager*
PaymentRequestWebContentsManager::GetOrCreateForWebContents(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  // CreateForWebContents does nothing if the manager instance already exists.
  PaymentRequestWebContentsManager::CreateForWebContents(web_contents);
  return PaymentRequestWebContentsManager::FromWebContents(web_contents);
}

void PaymentRequestWebContentsManager::CreatePaymentRequest(
    content::RenderFrameHost* render_frame_host,
    content::WebContents* web_contents,
    std::unique_ptr<PaymentRequestDelegate> delegate,
    mojo::InterfaceRequest<payments::mojom::PaymentRequest> request,
    PaymentRequest::ObserverForTest* observer_for_testing) {
  auto new_request = base::MakeUnique<PaymentRequest>(
      render_frame_host, web_contents, std::move(delegate), this,
      std::move(request), observer_for_testing);
  PaymentRequest* request_ptr = new_request.get();
  payment_requests_.insert(std::make_pair(request_ptr, std::move(new_request)));
}

void PaymentRequestWebContentsManager::DestroyRequest(PaymentRequest* request) {
  if (request == showing_)
    showing_ = nullptr;
  payment_requests_.erase(request);
}

bool PaymentRequestWebContentsManager::CanShow(PaymentRequest* request) {
  DCHECK(request);
  DCHECK(payment_requests_.find(request) != payment_requests_.end());
  if (!showing_) {
    showing_ = request;
    return true;
  } else {
    return false;
  }
}

PaymentRequestWebContentsManager::PaymentRequestWebContentsManager(
    content::WebContents* web_contents)
    : showing_(nullptr) {}

}  // namespace payments
