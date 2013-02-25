// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_

#include <vector>

#include "base/string16.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

namespace content {
class WebContents;
struct NativeWebKeyboardEvent;
}

namespace gfx {
class Rect;
}

namespace ui {
class ComboboxModel;
class MenuModel;
}

namespace autofill {

// This class defines the interface to the controller that the dialog view sees.
class AutofillDialogController {
 public:

  // Strings -------------------------------------------------------------------

  virtual string16 DialogTitle() const = 0;
  virtual string16 AccountChooserText() const = 0;
  virtual string16 EditSuggestionText() const = 0;
  virtual string16 UseBillingForShippingText() const = 0;
  virtual string16 CancelButtonText() const = 0;
  virtual string16 ConfirmButtonText() const = 0;
  virtual string16 CancelSignInText() const = 0;
  virtual string16 SaveLocallyText() const = 0;
  virtual string16 ProgressBarText() const = 0;

  // State ---------------------------------------------------------------------

  // Whether the user is known to be signed in.
  virtual DialogSignedInState SignedInState() const = 0;

  // Whether or not Wallet can be used for this transaction. Returns false if
  // any network/Wallet errors occur while running this dialog.
  virtual bool CanPayWithWallet() const = 0;

  // Whether the account chooser is enabled (clickable).
  virtual bool AccountChooserEnabled() const = 0;

  // Whether to show the checkbox to save data locally (in Autofill).
  virtual bool ShouldOfferToSaveInChrome() const = 0;

  // Detail inputs -------------------------------------------------------------

  // Whether the section is currently active (i.e. should be shown).
  virtual bool SectionIsActive(DialogSection section) const = 0;

  // Returns the set of inputs the page has requested which fall under
  // |section|.
  virtual const DetailInputs& RequestedFieldsForSection(DialogSection section)
      const = 0;

  // Returns the combobox model for inputs of type |type|, or NULL if the input
  // should be a text field.
  virtual ui::ComboboxModel* ComboboxModelForAutofillType(
      AutofillFieldType type) = 0;

  // Returns the model for suggestions for fields that fall under |section|.
  virtual ui::MenuModel* MenuModelForSection(DialogSection section) = 0;

  virtual string16 LabelForSection(DialogSection section) const = 0;
  virtual string16 SuggestionTextForSection(DialogSection section) = 0;
  virtual gfx::Image SuggestionIconForSection(DialogSection section) = 0;
  virtual void EditClickedForSection(DialogSection section) = 0;

  // Returns an icon to be displayed along with the input for the given type.
  // |user_input| is the current text in the textfield.
  virtual gfx::Image IconForField(AutofillFieldType type,
                                  const string16& user_input) const = 0;

  // Decides whether input of |value| is valid for a field of type |type|.
  virtual bool InputIsValid(AutofillFieldType type,
                            const string16& value) = 0;

  // Called when the user changes the contents of a text field or activates it
  // (by focusing and then clicking it). |was_edit| is true when the function
  // was called in response to the user editing the text field.
  virtual void UserEditedOrActivatedInput(const DetailInput* input,
                                          DialogSection section,
                                          gfx::NativeView parent_view,
                                          const gfx::Rect& content_bounds,
                                          const string16& field_contents,
                                          bool was_edit) = 0;

  // The view forwards keypresses in text inputs. Returns true if there should
  // be no further processing of the event.
  virtual bool HandleKeyPressEventInInput(
      const content::NativeWebKeyboardEvent& event) = 0;

  // Called when focus has changed position within the view.
  virtual void FocusMoved() = 0;

  // Miscellany ----------------------------------------------------------------

  // Called when the view has been closed. The value for |action| indicates
  // whether the Autofill operation should be aborted.
  virtual void ViewClosed(DialogAction action) = 0;

  // Returns any dialog notification that should currently be showing.
  virtual DialogNotification CurrentNotification() const = 0;

  // Begins the flow to sign into Wallet.
  virtual void StartSignInFlow() = 0;

  // Marks the signin flow into Wallet complete.
  virtual void EndSignInFlow() = 0;

  // Returns the profile for this dialog.
  virtual Profile* profile() = 0;

  // The web contents that prompted the dialog.
  virtual content::WebContents* web_contents() = 0;

 protected:
  virtual ~AutofillDialogController();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
