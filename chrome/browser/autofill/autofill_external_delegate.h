// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/autofill/password_autofill_manager.h"
#include "chrome/common/form_data.h"
#include "chrome/common/form_field_data.h"
#include "chrome/common/password_form_fill_data.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "ui/gfx/rect.h"

class AutofillManager;

namespace gfx {
class Rect;
}

namespace content {
class WebContents;
}

// TODO(csharp): A lot of the logic in this class is copied from autofillagent.
// Once Autofill is moved out of WebKit this class should be the only home for
// this logic. See http://crbug.com/51644

// Delegate for external processing of Autocomplete and Autofill
// display and selection.
class AutofillExternalDelegate
    : public content::WebContentsUserData<AutofillExternalDelegate>,
      public content::NotificationObserver {
 public:
  // Creates an AutofillExternalDelegate and attaches it to the specified
  // contents; the second argument is an AutofillManager managing Autofill for
  // that WebContents.
  //
  // This call is implemented per-platform.
  static void CreateForWebContentsAndManager(content::WebContents* web_contents,
                                             AutofillManager* autofill_manager);

  virtual ~AutofillExternalDelegate();

  // When using an external Autofill delegate.  Allows Chrome to tell
  // WebKit which Autofill selection has been chosen.
  virtual void SelectAutofillSuggestionAtIndex(int unique_id);

  // Records and associates a query_id with web form data.  Called
  // when the renderer posts an Autofill query to the browser. |bounds|
  // is window relative. |display_warning_if_disabled| tells us if we should
  // display warnings (such as autofill is disabled, but had suggestions).
  // We might not want to display the warning if a website has disabled
  // Autocomplete because they have their own popup, and showing our popup
  // on to of theirs would be a poor user experience.
  virtual void OnQuery(int query_id,
                       const FormData& form,
                       const FormFieldData& field,
                       const gfx::Rect& bounds,
                       bool display_warning_if_disabled);

  // Records query results and correctly formats them before sending them off
  // to be displayed.  Called when an Autofill query result is available.
  virtual void OnSuggestionsReturned(
      int query_id,
      const std::vector<string16>& autofill_values,
      const std::vector<string16>& autofill_labels,
      const std::vector<string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids);

  // Show password suggestions in the popup.
  void OnShowPasswordSuggestions(const std::vector<string16>& suggestions,
                                 const FormFieldData& field,
                                 const gfx::Rect& bounds);

  // Set the data list value associated with the current field.
  void SetCurrentDataListValues(const std::vector<string16>& autofill_values,
                                const std::vector<string16>& autofill_labels,
                                const std::vector<string16>& autofill_icons,
                                const std::vector<int>& autofill_unique_ids);

  // Remove the given Autocomplete entry from the DB.
  virtual void RemoveAutocompleteEntry(const string16& value);

  // Remove the given Autofill profile or credit credit.
  virtual void RemoveAutofillProfileOrCreditCard(int unique_id);

  // Inform the delegate that the text field editing has ended. This is
  // used to help record the metrics of when a new popup is shown.
  void DidEndTextFieldEditing();

  // Inform the delegate that an Autofill suggestion has been chosen. Returns
  // true if the suggestion was selected.
  bool DidAcceptAutofillSuggestions(const string16& value,
                                    int unique_id,
                                    unsigned index);

  // Informs the delegate that the Autofill previewed form should be cleared.
  virtual void ClearPreviewedForm();

  // Hide the Autofill poup.
  virtual void HideAutofillPopup();

  // Returns the delegate to its starting state by removing any page specific
  // values or settings.
  void Reset();

  // Inform the Password Manager of a filled form.
  void AddPasswordFormMapping(
      const FormFieldData& form,
      const PasswordFormFillData& fill_data);

 protected:
  friend class content::WebContentsUserData<AutofillExternalDelegate>;
  AutofillExternalDelegate(content::WebContents* web_contents,
                           AutofillManager* autofill_manager);

  // Displays the Autofill results to the user with an external Autofill popup
  // that lives completely in the browser.  The suggestions have been correctly
  // formatted by this point.
  virtual void ApplyAutofillSuggestions(
      const std::vector<string16>& autofill_values,
      const std::vector<string16>& autofill_labels,
      const std::vector<string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids) = 0;

  // Handle platform-dependent hiding.
  virtual void HideAutofillPopupInternal() = 0;

  // Create and position the popup given the bounds of the element it is
  // popping up for.
  virtual void CreatePopupForElement(const gfx::Rect& element_bounds) = 0;

  // Return the web_contents associated with this delegate.
  content::WebContents* web_contents() { return web_contents_; }

  // Return the bounds of the field currently selected.
  const gfx::Rect& field_bounds() { return field_bounds_; }

  bool popup_visible() const { return popup_visible_; }

 private:
  // Fills the form with the Autofill data corresponding to |unique_id|.
  // If |is_preview| is true then this is just a preview to show the user what
  // would be selected and if |is_preview| is false then the user has selected
  // this data.
  void FillAutofillFormData(int unique_id, bool is_preview);

  // Handle applying any Autofill warnings to the Autofill popup.
  void ApplyAutofillWarnings(std::vector<string16>* autofill_values,
                             std::vector<string16>* autofill_labels,
                             std::vector<string16>* autofill_icons,
                             std::vector<int>* autofill_unique_ids);

  // Handle applying any Autofill option listings to the Autofill popup.
  // This function should only get called when there is at least one
  // multi-field suggestion in the list of suggestions.
  void ApplyAutofillOptions(std::vector<string16>* autofill_values,
                            std::vector<string16>* autofill_labels,
                            std::vector<string16>* autofill_icons,
                            std::vector<int>* autofill_unique_ids);

  // Insert the data list values at the start of the given list, including
  // any required separators.
  void InsertDataListValues(std::vector<string16>* autofill_values,
                            std::vector<string16>* autofill_labels,
                            std::vector<string16>* autofill_icons,
                            std::vector<int>* autofill_unique_ids);

  // content::NotificationObserver method override.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::WebContents* web_contents_;  // weak; owns me.
  AutofillManager* autofill_manager_;  // weak.

  // Password Autofill manager, handles all password-related Autofilling.
  PasswordAutofillManager password_autofill_manager_;

  // The ID of the last request sent for form field Autofill.  Used to ignore
  // out of date responses.
  int autofill_query_id_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  // The current form and field selected by Autofill.
  FormData autofill_query_form_;
  FormFieldData autofill_query_field_;

  // Should we display a warning if Autofill is disabled?
  bool display_warning_if_disabled_;

  // Have we already shown Autofill suggestions for the field the user is
  // currently editing?  Used to keep track of state for metrics logging.
  bool has_shown_autofill_popup_for_current_edit_;

  // Used to indicate if a popup is currently being shown or not.
  bool popup_visible_;

  // The bounds of the field currently selected.
  gfx::Rect field_bounds_;

  // The current data list values.
  std::vector<string16> data_list_values_;
  std::vector<string16> data_list_labels_;
  std::vector<string16> data_list_icons_;
  std::vector<int> data_list_unique_ids_;

  DISALLOW_COPY_AND_ASSIGN(AutofillExternalDelegate);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_H_
