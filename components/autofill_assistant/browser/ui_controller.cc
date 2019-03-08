// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/ui_controller.h"

#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"

namespace autofill_assistant {

UiController::UiController() = default;
UiController::~UiController() = default;

void UiController::OnStateChanged(AutofillAssistantState new_state) {}
void UiController::OnStatusMessageChanged(const std::string& message) {}
void UiController::WillShutdown(Metrics::DropOutReason reason) {}
void UiController::OnSuggestionsChanged(const std::vector<Chip>& suggestions) {}
void UiController::OnActionsChanged(const std::vector<Chip>& actions) {}
void UiController::OnPaymentRequestChanged(
    const PaymentRequestOptions* options) {}
void UiController::OnDetailsChanged(const Details* details) {}
void UiController::OnProgressChanged(int progress) {}
void UiController::OnProgressVisibilityChanged(bool visible) {}
void UiController::OnTouchableAreaChanged(const std::vector<RectF>& areas) {}

}  // namespace autofill_assistant
