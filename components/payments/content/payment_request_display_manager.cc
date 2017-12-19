// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_display_manager.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/payments/content/content_payment_request_delegate.h"

namespace payments {

class PaymentRequest;

PaymentRequestDisplayManager::DisplayHandle::DisplayHandle(
    PaymentRequestDisplayManager* display_manager)
    : display_manager_(display_manager) {
  display_manager_->SetHandleAlive(true);
}

PaymentRequestDisplayManager::DisplayHandle::~DisplayHandle() {
  display_manager_->SetHandleAlive(false);
}

void PaymentRequestDisplayManager::DisplayHandle::Show(
    ContentPaymentRequestDelegate* delegate,
    PaymentRequest* request) {
  DCHECK(request);
  DCHECK(delegate);

  delegate->ShowDialog(request);
}

PaymentRequestDisplayManager::PaymentRequestDisplayManager()
    : handle_alive_(false) {}

PaymentRequestDisplayManager::~PaymentRequestDisplayManager() {}

std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle>
PaymentRequestDisplayManager::TryShow() {
  std::unique_ptr<PaymentRequestDisplayManager::DisplayHandle> handle = nullptr;
  if (!handle_alive_) {
    handle =
        base::MakeUnique<PaymentRequestDisplayManager::DisplayHandle>(this);
  }

  return handle;
}

void PaymentRequestDisplayManager::SetHandleAlive(bool handle_alive) {
  handle_alive_ = handle_alive;
}

}  // namespace payments
