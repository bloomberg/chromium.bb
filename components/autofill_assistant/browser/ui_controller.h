// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_H_

#include "components/autofill_assistant/browser/ui_delegate.h"

#include <string>

#include "base/callback_forward.h"

namespace autofill_assistant {
// Controller to control autofill assistant UI.
class UiController {
 public:
  virtual ~UiController() = default;

  // Set assistant UI delegate called by assistant UI controller.
  virtual void SetUiDelegate(UiDelegate* ui_delegate) = 0;

  // Show status message on the bottom bar.
  virtual void ShowStatusMessage(const std::string& message) = 0;

  // Show the overlay.
  virtual void ShowOverlay() = 0;

  // Hide the overlay.
  virtual void HideOverlay() = 0;

  // Show UI to ask user to choose an address in personal data manager. GUID of
  // the chosen address will be returned through callback if succeed, otherwise
  // empty string is returned.
  virtual void ChooseAddress(
      base::OnceCallback<void(const std::string&)> callback) = 0;

  // Show UI to ask user to choose a card in personal data manager. GUID of the
  // chosen card will be returned through callback if succeed, otherwise empty
  // string is returned.
  virtual void ChooseCard(
      base::OnceCallback<void(const std::string&)> callback) = 0;

 protected:
  UiController() = default;
};
}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_CONTROLLER_H_
