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
#include "components/autofill_assistant/browser/payment_information.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/state.h"
#include "components/autofill_assistant/browser/ui_delegate.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace autofill_assistant {

// Controller to control autofill assistant UI.
class UiController {
 public:
  virtual ~UiController() = default;

  // Called when the controller has entered a new state.
  virtual void OnStateChanged(AutofillAssistantState new_state) = 0;

  // Report that the status message has changed.
  virtual void OnStatusMessageChanged(const std::string& message) = 0;

  // Shuts down Autofill Assistant: hide the UI and frees any associated state.
  //
  // Warning: this indirectly deletes the caller.
  virtual void Shutdown(Metrics::DropOutReason reason) = 0;

  // Shuts down Autofill Assistant and closes Chrome.
  virtual void Close() = 0;

  // Show UI to ask user to make a choice between different chips.
  virtual void SetChips(std::unique_ptr<std::vector<Chip>> chips) = 0;

  // Remove all chips from the UI.
  virtual void ClearChips() = 0;

  // Get payment information (through similar to payment request UX) to fill
  // forms.
  virtual void GetPaymentInformation(
      payments::mojom::PaymentOptionsPtr payment_options,
      base::OnceCallback<void(std::unique_ptr<PaymentInformation>)> callback,
      const std::string& title,
      const std::vector<std::string>& supported_basic_card_networks) = 0;

  // Called when details have changed. Details will be null if they have been
  // cleared.
  virtual void OnDetailsChanged(const Details* details) = 0;

  // Show the progress bar and set it at |progress|%.
  virtual void ShowProgressBar(int progress) = 0;

  // Hide the progress bar.
  virtual void HideProgressBar() = 0;

  // Updates the area of the visible viewport that is accessible.
  //
  // If |enabled| is false, the visible viewport is accessible.
  //
  // |areas| is expressed in coordinates relative to the width or height of the
  // visible viewport, as a number between 0 and 1. It can be empty.
  virtual void UpdateTouchableArea(bool enabled,
                                   const std::vector<RectF>& areas) = 0;

 protected:
  UiController() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_H_
