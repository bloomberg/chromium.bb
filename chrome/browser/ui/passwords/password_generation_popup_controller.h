// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_CONTROLLER_H_

#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/autofill_popup_view_delegate.h"

namespace gfx {
class Range;
}

class PasswordGenerationPopupController
    : public autofill::AutofillPopupViewDelegate {
 public:
  enum GenerationState {
    // Generated password is offered in the popup but not filled yet.
    kOfferGeneration,
    // The generated password was accepted.
    kEditGeneratedPassword,
  };

  // Called by the view when the password was accepted.
  virtual void PasswordAccepted() = 0;

  // Called by the view when the saved passwords link is clicked.
  // TODO(crbug.com/862269): Remove when "Smart Lock" is gone.
  virtual void OnSavedPasswordsLinkClicked() = 0;

  // Return the minimum allowable width for the popup.
  virtual int GetMinimumWidth() = 0;

  // Accessors
  virtual GenerationState state() const = 0;
  virtual bool password_selected() const = 0;
  virtual const base::string16& password() const = 0;

  // Translated strings
  virtual base::string16 SuggestedText() = 0;
  virtual const base::string16& HelpText() = 0;
  // TODO(crbug.com/862269): Remove when "Smart Lock" is gone.
  virtual gfx::Range HelpTextLinkRange() = 0;

 protected:
  ~PasswordGenerationPopupController() override = default;
};

#endif  // CHROME_BROWSER_UI_PASSWORDS_PASSWORD_GENERATION_POPUP_CONTROLLER_H_
