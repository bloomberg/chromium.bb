// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_DELEGATE_H_

#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "components/autofill/core/browser/detail_input.h"
#include "components/autofill/core/browser/dialog_section.h"
#include "components/autofill/core/browser/field_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/range/range.h"

class FieldValueMap;
class GURL;
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

typedef std::map<ServerFieldType, gfx::Image> FieldIconMap;

// This class defines the interface to the controller that the dialog view sees.
class AutofillDialogViewDelegate {
 public:
  // Strings -------------------------------------------------------------------

  virtual base::string16 DialogTitle() const = 0;
  virtual base::string16 EditSuggestionText() const = 0;
  virtual base::string16 CancelButtonText() const = 0;
  virtual base::string16 ConfirmButtonText() const = 0;
  virtual base::string16 SaveLocallyText() const = 0;
  virtual base::string16 SaveLocallyTooltip() const = 0;

  // State ---------------------------------------------------------------------

  // Whether to show the checkbox to save data locally (in Autofill).
  virtual bool ShouldOfferToSaveInChrome() const = 0;

  // Whether the checkbox to save data locally should be checked initially.
  virtual bool ShouldSaveInChrome() const = 0;

  // Which dialog buttons should be visible.
  virtual int GetDialogButtons() const = 0;

  // Whether or not the |button| should be enabled.
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const = 0;

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
      ServerFieldType type) = 0;

  // Returns the model for suggestions for fields that fall under |section|.
  // This may return NULL, in which case no menu should be shown for that
  // section.
  virtual ui::MenuModel* MenuModelForSection(DialogSection section) = 0;

  // Returns the label text used to describe the section (i.e. Billing).
  virtual base::string16 LabelForSection(DialogSection section) const = 0;

  // Returns the current state of suggestions for |section|.
  virtual SuggestionState SuggestionStateForSection(DialogSection section) = 0;

  // Returns the icons to be displayed along with the given |user_inputs| in a
  // section.
  virtual FieldIconMap IconsForFields(
      const FieldValueMap& user_inputs) const = 0;

  // Returns true if the value of this field |type| controls the icons for the
  // rest of the fields in a section.
  virtual bool FieldControlsIcons(ServerFieldType type) const = 0;

  // Returns a tooltip for the given field, or an empty string if none exists.
  virtual base::string16 TooltipForField(ServerFieldType type) const = 0;

  // Decides whether input of |value| is valid for a field of type |type|. If
  // valid, the returned string will be empty. Otherwise it will contain an
  // error message.
  virtual base::string16 InputValidityMessage(DialogSection section,
                                        ServerFieldType type,
                                        const base::string16& value) = 0;

  // Decides whether the combination of all |inputs| is valid, returns a
  // map of field types to validity messages.
  virtual ValidityMessages InputsAreValid(DialogSection section,
                                          const FieldValueMap& inputs) = 0;

  // Called when the user edits or activates a textfield or combobox.
  // |was_edit| is true when the function was called in response to the user
  // editing the input.
  virtual void UserEditedOrActivatedInput(DialogSection section,
                                          ServerFieldType type,
                                          gfx::NativeView parent_view,
                                          const gfx::Rect& content_bounds,
                                          const base::string16& field_contents,
                                          bool was_edit) = 0;

  // The view forwards keypresses in text inputs. Returns true if there should
  // be no further processing of the event.
  virtual bool HandleKeyPressEventInInput(
      const content::NativeWebKeyboardEvent& event) = 0;

  // Called when focus has changed position within the view.
  virtual void FocusMoved() = 0;

  // Whether the view is allowed to show a validation error bubble.
  virtual bool ShouldShowErrorBubble() const = 0;

  // Miscellany ----------------------------------------------------------------

  // Called when the view has been closed.
  virtual void ViewClosed() = 0;

  // Returns dialog notifications that the view should currently be showing in
  // order from top to bottom.
  virtual std::vector<DialogNotification> CurrentNotifications() = 0;

  // Called when a generic link has been clicked in the dialog. Opens the URL
  // out-of-line.
  virtual void LinkClicked(const GURL& url) = 0;

  // Called when the view has been cancelled.
  virtual void OnCancel() = 0;

  // Called when the view has been accepted.
  virtual void OnAccept() = 0;

  // Returns the profile for this dialog.
  virtual Profile* profile() = 0;

  // The web contents that prompted the dialog.
  virtual content::WebContents* GetWebContents() = 0;

 protected:
  virtual ~AutofillDialogViewDelegate();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_DELEGATE_H_
