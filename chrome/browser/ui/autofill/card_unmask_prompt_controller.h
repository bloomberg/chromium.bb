// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_CONTROLLER_H_

#include "base/strings/string16.h"

namespace content {
class WebContents;
}

namespace autofill {

class CardUnmaskPromptController {
 public:
  // Interaction.
  virtual void OnUnmaskDialogClosed() = 0;
  virtual void OnUnmaskResponse(const base::string16& cvc) = 0;

  // State.
  virtual content::WebContents* GetWebContents() = 0;
  virtual base::string16 GetWindowTitle() const = 0;
  virtual base::string16 GetInstructionsMessage() const = 0;
  virtual int GetCvcImageRid() const = 0;
  virtual bool InputTextIsValid(const base::string16& input_text) const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_CARD_UNMASK_PROMPT_CONTROLLER_H_
