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
#include "components/autofill_assistant/browser/info_box.h"
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

  // Report that the set of suggestions has changed.
  virtual void OnSuggestionsChanged(const std::vector<Chip>& suggestions);

  // Report that the set of actions has changed.
  virtual void OnActionsChanged(const std::vector<Chip>& actions);

  // Gets or clears request for payment information.
  virtual void OnPaymentRequestChanged(const PaymentRequestOptions* options);

  // Called when details have changed. Details will be null if they have been
  // cleared.
  virtual void OnDetailsChanged(const Details* details);

  // Called when info box has changed. |info_box| will be null if it has been
  // cleared.
  virtual void OnInfoBoxChanged(const InfoBox* info_box);

  // Called when the current progress has changed. Progress, is expressed as a
  // percentage.
  virtual void OnProgressChanged(int progress);

  // Called when the current progress bar visibility has changed. If |visible|
  // is true, then the bar is now shown.
  virtual void OnProgressVisibilityChanged(bool visible);

  // Updates the area of the visible viewport that is accessible when the
  // overlay state is OverlayState::PARTIAL.
  //
  // |rectangles| contains one element per configured rectangles, though these
  // can correspond to empty rectangles. Coordinates are relative to the width
  // or height of the visible viewport, as a number between 0 and 1.
  virtual void OnTouchableAreaChanged(const std::vector<RectF>& rectangles);

  // Called when the viewport resize flag has changed.
  virtual void OnResizeViewportChanged(bool resize_viewport);

  // Called when the peek mode has changed.
  virtual void OnPeekModeChanged(ConfigureBottomSheetProto::PeekMode peek_mode);
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_H_
