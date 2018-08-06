// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_UI_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_UI_CONTROLLER_H_

#include "components/autofill_assistant/browser/assistant_ui_delegate.h"

#include <string>

namespace autofill_assistant {
// Controller to control autofill assistant UI.
class AssistantUiController {
 public:
  virtual ~AssistantUiController() = default;

  // Set assistant UI delegate called by assistant UI controller.
  virtual void SetUiDelegate(AssistantUiDelegate* ui_delegate) = 0;

  // Show status message on the bottom bar.
  virtual void ShowStatusMessage(const std::string& message) = 0;

  // Show the overlay.
  virtual void ShowOverlay() = 0;

  // Hide the overlay.
  virtual void HideOverlay() = 0;

 protected:
  AssistantUiController() = default;
};
}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_UI_CONTROLLER_H_