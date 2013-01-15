// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_delegate.h"
#include "content/public/browser/keyboard_listener.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/ssl_status.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/native_widget_types.h"

class AutofillPopupControllerImpl;
class FormGroup;
class GURL;
class Profile;

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

namespace autofill {

class AutofillDialogView;

// This class drives the dialog that appears when a site uses the imperative
// autocomplete API to fill out a form.
class AutofillDialogController : public AutofillPopupDelegate,
                                 public content::NotificationObserver,
                                 public SuggestionsMenuModelDelegate,
                                 public content::KeyboardListener {
 public:
  AutofillDialogController(
      content::WebContents* contents,
      const FormData& form_structure,
      const GURL& source_url,
      const content::SSLStatus& ssl_status,
      const base::Callback<void(const FormStructure*)>& callback);
  virtual ~AutofillDialogController();

  void Show();

  // Top bar / notification area related ---------------------------------------

  string16 DialogTitle() const;
  DialogNotification Notification() const;

  // Input related -------------------------------------------------------------

  string16 LabelForSection(DialogSection section) const;
  string16 UseBillingForShippingText() const;
  string16 SignInText() const;
  string16 CancelSignInText() const;
  // Returns the set of inputs the page has requested which fall under
  // |section|.
  const DetailInputs& RequestedFieldsForSection(DialogSection section) const;
  // Returns the combobox model for inputs of type |type|, or NULL if the input
  // should be a text field.
  ui::ComboboxModel* ComboboxModelForAutofillType(AutofillFieldType type);
  // Returns the model for suggestions for fields that fall under |section|.
  ui::MenuModel* MenuModelForSection(DialogSection section);
  string16 SuggestionTextForSection(DialogSection section);
  bool InputIsValid(const DetailInput* input, const string16& value);

  // Begins the flow to sign into Wallet.
  void StartSignInFlow();

  // Marks the signin flow into Wallet complete.
  void EndSignInFlow();

  // Called by the view when the user changes the contents of a text field.
  void UserEditedInput(const DetailInput* input,
                       DialogSection section,
                       gfx::NativeView view,
                       const gfx::Rect& content_bounds,
                       const string16& field_contents);

  // Called when focus has changed position within the view.
  void FocusMoved();

  // Submit / confirm related --------------------------------------------------

  // TODO(dbeam): move Wallet checkbox to notification area to match mocks.
  string16 WalletOptionText() const;
  string16 CancelButtonText() const;
  string16 ConfirmButtonText() const;
  bool ConfirmButtonEnabled() const;

  // Called when the view has been closed. The value for |action| indicates
  // whether the Autofill operation should be aborted.
  void ViewClosed(DialogAction action);

  // AutofillPopupDelegate implementation.
  virtual void DidSelectSuggestion(int identifier) OVERRIDE;
  virtual void DidAcceptSuggestion(const string16& value,
                                   int identifier) OVERRIDE;
  virtual void RemoveSuggestion(const string16& value,
                                int identifier) OVERRIDE;
  virtual void ClearPreviewedForm() OVERRIDE;
  virtual void ControllerDestroyed() OVERRIDE;

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // SuggestionsMenuModelDelegate implementation.
  virtual void SuggestionItemSelected(const SuggestionsMenuModel& model)
      OVERRIDE;

  // KeyboardListener implementation.
  // The view should forward keypresses in text inputs to the controller.
  virtual bool HandleKeyPressEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;

  content::WebContents* web_contents() { return contents_; }

  Profile* profile() { return profile_; }

 private:
  // Determines whether |input| and |field| match.
  typedef base::Callback<bool(const DetailInput& input,
                              const AutofillField& field)> InputFieldComparator;

  // Whether or not the current request wants credit info back.
  bool RequestingCreditCardInfo() const;

  // Whether the information input in this dialog will be securely transmitted
  // to the requesting site.
  bool TransmissionWillBeSecure() const;

  // Initializes |suggested_email_| et al.
  void GenerateSuggestionsModels();

  // Returns whether |profile| is complete, i.e. can fill out all the relevant
  // address info. Incomplete profiles will not be displayed in the dropdown
  // menu.
  bool IsCompleteProfile(const AutofillProfile& profile);

  // Fills in |section|-related fields in |output_| according to the state of
  // |view_|.
  void FillOutputForSection(DialogSection section);
  // As above, but uses |compare| to determine whether a DetailInput matches
  // a field.
  void FillOutputForSectionWithComparator(DialogSection section,
                                          const InputFieldComparator& compare);

  // Fills in |form_structure_| using |form_group|. Utility method for
  // FillOutputForSection.
  void FillFormStructureForSection(const FormGroup& form_group,
                                   size_t variant,
                                   DialogSection section,
                                   const InputFieldComparator& compare);

  // Gets the SuggestionsMenuModel for |section|.
  SuggestionsMenuModel* SuggestionsMenuModelForSection(DialogSection section);
  // And the reverse.
  DialogSection SectionForSuggestionsMenuModel(
      const SuggestionsMenuModel& model);

  // Loads profiles that can suggest data for |type|. |field_contents| is the
  // part the user has already typed. |inputs| is the rest of section.
  // Identifying info is loaded into the last three outparams as well as
  // |popup_guids_|.
  void GetProfileSuggestions(
      AutofillFieldType type,
      const string16& field_contents,
      const DetailInputs& inputs,
      std::vector<string16>* popup_values,
      std::vector<string16>* popup_labels,
      std::vector<string16>* popup_icons);

  // Returns the PersonalDataManager for |profile_|.
  PersonalDataManager* GetManager();

  // Like RequestedFieldsForSection, but returns a pointer.
  DetailInputs* MutableRequestedFieldsForSection(DialogSection section);

  // The |profile| for |contents_|.
  Profile* const profile_;

  // The WebContents where the Autofill action originated.
  content::WebContents* const contents_;

  FormStructure form_structure_;

  // Whether the URL visible to the user when this dialog was requested to be
  // invoked is the same as |source_url_|.
  bool invoked_from_same_origin_;

  // The URL of the invoking site.
  GURL source_url_;

  // The SSL info from the invoking site.
  content::SSLStatus ssl_status_;

  base::Callback<void(const FormStructure*)> callback_;

  // The fields for billing and shipping which the page has actually requested.
  DetailInputs requested_email_fields_;
  DetailInputs requested_cc_fields_;
  DetailInputs requested_billing_fields_;
  DetailInputs requested_shipping_fields_;

  // Models for the credit card expiration inputs.
  MonthComboboxModel cc_exp_month_combobox_model_;
  YearComboboxModel cc_exp_year_combobox_model_;

  // Models for the suggestion views.
  SuggestionsMenuModel suggested_email_;
  SuggestionsMenuModel suggested_cc_;
  SuggestionsMenuModel suggested_billing_;
  SuggestionsMenuModel suggested_shipping_;

  // The GUIDs for the currently showing unverified profiles popup.
  std::vector<PersonalDataManager::GUIDPair> popup_guids_;

  AutofillPopupControllerImpl* popup_controller_;

  // The section for which |popup_controller_| is currently showing a popup
  // (if any).
  DialogSection section_showing_popup_;

  scoped_ptr<AutofillDialogView> view_;

  // A NotificationRegistrar for tracking the completion of sign-in.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogController);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
