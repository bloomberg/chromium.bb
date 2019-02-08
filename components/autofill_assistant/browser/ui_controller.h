// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "components/autofill_assistant/browser/chip.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/payment_request.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace autofill_assistant {

// Controller to control autofill assistant UI.
//
// TODO(crbug/925947): Rename this class to Observer and make treat it as a real
// observer in the Controller.
class UiController {
 public:
  UiController();
  virtual ~UiController();

  // Called when the controller has entered a new state.
  virtual void OnStateChanged(AutofillAssistantState new_state);

  // Report that the status message has changed.
  virtual void OnStatusMessageChanged(const std::string& message);

  // Autofill Assistant is about to be shut down for this tab.
  //
  // Pointer to UIDelegate will become invalid as soon as this method has
  // returned.
  virtual void WillShutdown(Metrics::DropOutReason reason);

  // Report that the set of chips has changed.
  virtual void OnChipsChanged(const std::vector<Chip>& chips);

  // Gets or clears request for payment information.
  virtual void OnPaymentRequestChanged(const PaymentRequestOptions* options);

  // Called when details have changed. Details will be null if they have been
  // cleared.
  virtual void OnDetailsChanged(const Details* details);

  // Called when the current progress has changed. Progress, is expressed as a
  // percentage.
  virtual void OnProgressChanged(int progress);

  // Updates the area of the visible viewport that is accessible when the
  // overlay state is OverlayState::PARTIAL.
  //
  // |areas| is expressed in coordinates relative to the width or height of the
  // visible viewport, as a number between 0 and 1. It can be empty.
  virtual void OnTouchableAreaChanged(const std::vector<RectF>& areas);
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_H_
