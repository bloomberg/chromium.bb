// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_external_delegate.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
namespace {

enum AccessAddressBookEventType {
  // An Autofill entry was shown that prompts the user to give Chrome access to
  // the user's Address Book.
  SHOWED_ACCESS_ADDRESS_BOOK_ENTRY = 0,

  // The user selected the Autofill entry which prompts Chrome to access the
  // user's Address Book.
  SELECTED_ACCESS_ADDRESS_BOOK_ENTRY = 1,

  // Always keep this at the end.
  ACCESS_ADDRESS_BOOK_ENTRY_MAX,
};

// Emits an entry for the histogram.
void EmitHistogram(AccessAddressBookEventType type) {
  UMA_HISTOGRAM_ENUMERATION(
      "Autofill.MacAddressBook", type, ACCESS_ADDRESS_BOOK_ENTRY_MAX);
}

}  // namespace
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

namespace autofill {

namespace {

bool ShouldAutofill(const FormFieldData& form_field) {
  return form_field.should_autocomplete ||
         !base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kRespectAutocompleteOffForAutofill);
}

}  // namespace

AutofillExternalDelegate::AutofillExternalDelegate(AutofillManager* manager,
                                                   AutofillDriver* driver)
    : manager_(manager),
      driver_(driver),
      query_id_(0),
      display_warning_if_disabled_(false),
      has_suggestion_(false),
      has_shown_popup_for_current_edit_(false),
      should_show_scan_credit_card_(false),
      has_shown_address_book_prompt(false),
      weak_ptr_factory_(this) {
  DCHECK(manager);
}

AutofillExternalDelegate::~AutofillExternalDelegate() {}

void AutofillExternalDelegate::OnQuery(int query_id,
                                       const FormData& form,
                                       const FormFieldData& field,
                                       const gfx::RectF& element_bounds,
                                       bool display_warning_if_disabled) {
  if (!query_form_.SameFormAs(form))
    has_shown_address_book_prompt = false;

  query_form_ = form;
  query_field_ = field;
  display_warning_if_disabled_ = display_warning_if_disabled;
  query_id_ = query_id;
  element_bounds_ = element_bounds;
  should_show_scan_credit_card_ =
      manager_->ShouldShowScanCreditCard(query_form_, query_field_);
}

void AutofillExternalDelegate::OnSuggestionsReturned(
    int query_id,
    const std::vector<Suggestion>& input_suggestions) {
  if (query_id != query_id_)
    return;

  std::vector<Suggestion> suggestions(input_suggestions);

  // Add or hide warnings as appropriate.
  ApplyAutofillWarnings(&suggestions);

  // Add a separator to go between the values and menu items.
  suggestions.push_back(Suggestion());
  suggestions.back().frontend_id = POPUP_ITEM_ID_SEPARATOR;

  if (should_show_scan_credit_card_) {
    Suggestion scan_credit_card(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_SCAN_CREDIT_CARD));
    scan_credit_card.frontend_id = POPUP_ITEM_ID_SCAN_CREDIT_CARD;
    scan_credit_card.icon = base::ASCIIToUTF16("scanCreditCardIcon");
    suggestions.push_back(scan_credit_card);

    if (!has_shown_popup_for_current_edit_) {
      AutofillMetrics::LogScanCreditCardPromptMetric(
          AutofillMetrics::SCAN_CARD_ITEM_SHOWN);
    }
  }

  // Only include "Autofill Options" special menu item if we have Autofill
  // suggestions.
  has_suggestion_ = false;
  for (size_t i = 0; i < suggestions.size(); ++i) {
    if (suggestions[i].frontend_id > 0) {
      has_suggestion_ = true;
      break;
    }
  }

  if (has_suggestion_)
    ApplyAutofillOptions(&suggestions);

  // Remove the separator if it is the last element.
  DCHECK_GT(suggestions.size(), 0U);
  if (suggestions.back().frontend_id == POPUP_ITEM_ID_SEPARATOR)
    suggestions.pop_back();

  // If anything else is added to modify the values after inserting the data
  // list, AutofillPopupControllerImpl::UpdateDataListValues will need to be
  // updated to match.
  InsertDataListValues(&suggestions);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  if (suggestions.empty() &&
      manager_->ShouldShowAccessAddressBookSuggestion(query_form_,
                                                      query_field_)) {
    Suggestion mac_contacts(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_ACCESS_MAC_CONTACTS));
    mac_contacts.icon = base::ASCIIToUTF16("macContactsIcon");
    mac_contacts.frontend_id = POPUP_ITEM_ID_MAC_ACCESS_CONTACTS;

    if (!has_shown_address_book_prompt) {
      has_shown_address_book_prompt = true;
      EmitHistogram(SHOWED_ACCESS_ADDRESS_BOOK_ENTRY);
      manager_->ShowedAccessAddressBookPrompt();
    }
  }
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

  if (suggestions.empty()) {
    // No suggestions, any popup currently showing is obsolete.
    manager_->client()->HideAutofillPopup();
    return;
  }

  // Send to display.
  if (query_field_.is_focusable) {
    manager_->client()->ShowAutofillPopup(element_bounds_,
                                          query_field_.text_direction,
                                          suggestions,
                                          GetWeakPtr());
  }
}

void AutofillExternalDelegate::SetCurrentDataListValues(
    const std::vector<base::string16>& data_list_values,
    const std::vector<base::string16>& data_list_labels) {
  data_list_values_ = data_list_values;
  data_list_labels_ = data_list_labels;

  manager_->client()->UpdateAutofillPopupDataListValues(data_list_values,
                                                        data_list_labels);
}

void AutofillExternalDelegate::OnPopupShown() {
  manager_->DidShowSuggestions(
      has_suggestion_ && !has_shown_popup_for_current_edit_,
      query_form_,
      query_field_);
  has_shown_popup_for_current_edit_ |= has_suggestion_;
}

void AutofillExternalDelegate::OnPopupHidden() {
  driver_->PopupHidden();
}

void AutofillExternalDelegate::DidSelectSuggestion(
    const base::string16& value,
    int identifier) {
  ClearPreviewedForm();

  // Only preview the data if it is a profile.
  if (identifier > 0)
    FillAutofillFormData(identifier, true);
  else if (identifier == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY)
    driver_->RendererShouldPreviewFieldWithValue(value);
}

void AutofillExternalDelegate::DidAcceptSuggestion(const base::string16& value,
                                                   int identifier) {
  if (identifier == POPUP_ITEM_ID_AUTOFILL_OPTIONS) {
    // User selected 'Autofill Options'.
    manager_->ShowAutofillSettings();
  } else if (identifier == POPUP_ITEM_ID_CLEAR_FORM) {
    // User selected 'Clear form'.
    driver_->RendererShouldClearFilledForm();
  } else if (identifier == POPUP_ITEM_ID_PASSWORD_ENTRY) {
    NOTREACHED();  // Should be handled elsewhere.
  } else if (identifier == POPUP_ITEM_ID_DATALIST_ENTRY) {
    driver_->RendererShouldAcceptDataListSuggestion(value);
  } else if (identifier == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY) {
    // User selected an Autocomplete, so we fill directly.
    driver_->RendererShouldFillFieldWithValue(value);
  } else if (identifier == POPUP_ITEM_ID_MAC_ACCESS_CONTACTS) {
#if defined(OS_MACOSX) && !defined(OS_IOS)
    EmitHistogram(SELECTED_ACCESS_ADDRESS_BOOK_ENTRY);
    UMA_HISTOGRAM_SPARSE_SLOWLY(
        "Autofill.MacAddressBook.NumShowsBeforeSelected",
        manager_->AccessAddressBookPromptCount());

    // User wants to give Chrome access to user's address book.
    manager_->AccessAddressBook();

    // There is no deterministic method for deciding whether a blocking dialog
    // was presented. The following comments and code assume that a blocking
    // dialog was presented, but still behave correctly if no dialog was
    // presented.

    // A blocking dialog was presented, and the user has already responded to
    // the dialog. The presentation of the dialog added an NSEvent to the
    // NSRunLoop which will cause all windows to lose focus. When the NSEvent
    // is processed, it will be sent to the renderer which will cause the text
    // field to lose focus. This returns an IPC to Chrome which will dismiss
    // the Autofill popup. We post a task which we expect to run after the
    // NSEvent has been processed by the NSRunLoop. It pings the renderer,
    // which returns an IPC acknowledging the ping.  At that time, redisplay
    // the popup. FIFO processing of IPCs ensures that all side effects of the
    // NSEvent will have been processed.

    // 10ms sits nicely under the 16ms threshold for 60 fps, and likely gives
    // the NSApplication run loop sufficient time to process the NSEvent. In
    // testing, a delay of 0ms was always sufficient.
    base::TimeDelta delay(base::TimeDelta::FromMilliseconds(10));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&AutofillExternalDelegate::PingRenderer, GetWeakPtr()),
        delay);
#else
    NOTREACHED();
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)
  } else if (identifier == POPUP_ITEM_ID_SCAN_CREDIT_CARD) {
    manager_->client()->ScanCreditCard(base::Bind(
        &AutofillExternalDelegate::OnCreditCardScanned, GetWeakPtr()));
  } else {
    FillAutofillFormData(identifier, false);
  }

  if (should_show_scan_credit_card_) {
    AutofillMetrics::LogScanCreditCardPromptMetric(
        identifier == POPUP_ITEM_ID_SCAN_CREDIT_CARD
            ? AutofillMetrics::SCAN_CARD_ITEM_SELECTED
            : AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED);
  }

  manager_->client()->HideAutofillPopup();
}

void AutofillExternalDelegate::RemoveSuggestion(const base::string16& value,
                                                int identifier) {
  if (identifier > 0)
    manager_->RemoveAutofillProfileOrCreditCard(identifier);
  else
    manager_->RemoveAutocompleteEntry(query_field_.name, value);
}

void AutofillExternalDelegate::DidEndTextFieldEditing() {
  manager_->client()->HideAutofillPopup();

  has_shown_popup_for_current_edit_ = false;
}

void AutofillExternalDelegate::ClearPreviewedForm() {
  driver_->RendererShouldClearPreviewedForm();
}

void AutofillExternalDelegate::Reset() {
  manager_->client()->HideAutofillPopup();
}

void AutofillExternalDelegate::OnPingAck() {
  // Reissue the most recent query, which will reopen the Autofill popup.
  manager_->OnQueryFormFieldAutofill(query_id_,
                                     query_form_,
                                     query_field_,
                                     element_bounds_,
                                     display_warning_if_disabled_);
}

base::WeakPtr<AutofillExternalDelegate> AutofillExternalDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillExternalDelegate::OnCreditCardScanned(
    const base::string16& card_number,
    int expiration_month,
    int expiration_year) {
  manager_->FillCreditCardForm(
      query_id_, query_form_, query_field_,
      CreditCard(card_number, expiration_month, expiration_year));
}

void AutofillExternalDelegate::FillAutofillFormData(int unique_id,
                                                    bool is_preview) {
  // If the selected element is a warning we don't want to do anything.
  if (unique_id == POPUP_ITEM_ID_WARNING_MESSAGE)
    return;

  AutofillDriver::RendererFormDataAction renderer_action = is_preview ?
      AutofillDriver::FORM_DATA_ACTION_PREVIEW :
      AutofillDriver::FORM_DATA_ACTION_FILL;

  DCHECK(driver_->RendererIsAvailable());
  // Fill the values for the whole form.
  manager_->FillOrPreviewForm(renderer_action,
                              query_id_,
                              query_form_,
                              query_field_,
                              unique_id);
}

void AutofillExternalDelegate::ApplyAutofillWarnings(
    std::vector<Suggestion>* suggestions) {
  if (!ShouldAutofill(query_field_)) {
    // Autofill is disabled.  If there were some profile or credit card
    // suggestions to show, show a warning instead.  Otherwise, clear out the
    // list of suggestions.
    if (!suggestions->empty() && (*suggestions)[0].frontend_id > 0) {
      // If Autofill is disabled and we had suggestions, show a warning instead.
      suggestions->assign(1, Suggestion(
          l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_FORM_DISABLED)));
      (*suggestions)[0].frontend_id = POPUP_ITEM_ID_WARNING_MESSAGE;
    } else {
      suggestions->clear();
    }
  } else if (suggestions->size() > 1 &&
             (*suggestions)[0].frontend_id == POPUP_ITEM_ID_WARNING_MESSAGE) {
    // If we received a warning instead of suggestions from Autofill but regular
    // suggestions from autocomplete, don't show the Autofill warning.
    suggestions->erase(suggestions->begin());
  }

  // If we were about to show a warning and we shouldn't, don't.
  if (!suggestions->empty() &&
      (*suggestions)[0].frontend_id == POPUP_ITEM_ID_WARNING_MESSAGE &&
      !display_warning_if_disabled_) {
    suggestions->clear();
  }
}

void AutofillExternalDelegate::ApplyAutofillOptions(
    std::vector<Suggestion>* suggestions) {
  // The form has been auto-filled, so give the user the chance to clear the
  // form.  Append the 'Clear form' menu item.
  if (query_field_.is_autofilled) {
    suggestions->push_back(Suggestion(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM)));
    suggestions->back().frontend_id = POPUP_ITEM_ID_CLEAR_FORM;
  }

  // Append the 'Chrome Autofill options' menu item;
  suggestions->push_back(Suggestion(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_OPTIONS_POPUP)));
  suggestions->back().frontend_id = POPUP_ITEM_ID_AUTOFILL_OPTIONS;
}

void AutofillExternalDelegate::InsertDataListValues(
    std::vector<Suggestion>* suggestions) {
  if (data_list_values_.empty())
    return;

  // Insert the separator between the datalist and Autofill values (if there
  // are any).
  if (!suggestions->empty()) {
    suggestions->insert(suggestions->begin(), Suggestion());
    (*suggestions)[0].frontend_id = POPUP_ITEM_ID_SEPARATOR;
  }

  // Insert the datalist elements at the beginning.
  suggestions->insert(suggestions->begin(), data_list_values_.size(),
                      Suggestion());
  for (size_t i = 0; i < data_list_values_.size(); i++) {
    (*suggestions)[i].value = data_list_values_[i];
    (*suggestions)[i].label = data_list_labels_[i];
    (*suggestions)[i].frontend_id = POPUP_ITEM_ID_DATALIST_ENTRY;
  }
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
void AutofillExternalDelegate::PingRenderer() {
  driver_->PingRenderer();
}
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

}  // namespace autofill
