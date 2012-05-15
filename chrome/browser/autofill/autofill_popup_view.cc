// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_popup_view.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "grit/webkit_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAutofillClient.h"

using WebKit::WebAutofillClient;

namespace {

// Used to indicate that no line is currently selected by the user.
const int kNoSelection = -1;

struct DataResource {
  const char* name;
  int id;
};

const DataResource kDataResources[] = {
  { "americanExpressCC", IDR_AUTOFILL_CC_AMEX },
  { "dinersCC", IDR_AUTOFILL_CC_DINERS },
  { "discoverCC", IDR_AUTOFILL_CC_DISCOVER },
  { "genericCC", IDR_AUTOFILL_CC_GENERIC },
  { "jcbCC", IDR_AUTOFILL_CC_JCB },
  { "masterCardCC", IDR_AUTOFILL_CC_MASTERCARD },
  { "soloCC", IDR_AUTOFILL_CC_SOLO },
  { "visaCC", IDR_AUTOFILL_CC_VISA },
};

}  // end namespace

AutofillPopupView::AutofillPopupView(
    content::WebContents* web_contents,
    AutofillExternalDelegate* external_delegate)
    : external_delegate_(external_delegate),
      selected_line_(kNoSelection) {
  if (!web_contents)
    return;

  registrar_.Add(this,
                 content::NOTIFICATION_WEB_CONTENTS_HIDDEN,
                 content::Source<content::WebContents>(web_contents));
  registrar_.Add(
      this,
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::Source<content::NavigationController>(
          &(web_contents->GetController())));
}

AutofillPopupView::~AutofillPopupView() {}

void AutofillPopupView::Hide() {
  HideInternal();

  external_delegate_->ClearPreviewedForm();
}

void AutofillPopupView::Show(const std::vector<string16>& autofill_values,
                             const std::vector<string16>& autofill_labels,
                             const std::vector<string16>& autofill_icons,
                             const std::vector<int>& autofill_unique_ids) {
  autofill_values_ = autofill_values;
  autofill_labels_ = autofill_labels;
  autofill_icons_ = autofill_icons;
  autofill_unique_ids_ = autofill_unique_ids;

  ShowInternal();
}

void AutofillPopupView::SetSelectedLine(int selected_line) {
  if (selected_line_ == selected_line)
    return;

  if (selected_line_ != kNoSelection)
    InvalidateRow(selected_line_);

  if (selected_line != kNoSelection)
    InvalidateRow(selected_line);

  selected_line_ = selected_line;

  if (selected_line_ != kNoSelection) {
    external_delegate_->SelectAutofillSuggestionAtIndex(
        autofill_unique_ids_[selected_line_]);
  }
}

void AutofillPopupView::SelectNextLine() {
  int new_selected_line = selected_line_ + 1;

  if (new_selected_line == static_cast<int>(autofill_values_.size()))
    new_selected_line = 0;

  SetSelectedLine(new_selected_line);
}

void AutofillPopupView::SelectPreviousLine() {
  int new_selected_line = selected_line_ - 1;

  if (new_selected_line <= kNoSelection)
    new_selected_line = autofill_values_.size() - 1;

  SetSelectedLine(new_selected_line);
}

bool AutofillPopupView::AcceptSelectedLine() {
  if (selected_line_ == kNoSelection)
    return false;

  DCHECK_GE(selected_line_, 0);
  DCHECK_LT(selected_line_, static_cast<int>(autofill_values_.size()));

  return external_delegate()->DidAcceptAutofillSuggestions(
      autofill_values_[selected_line_],
      autofill_unique_ids_[selected_line_],
      selected_line_);
}

bool AutofillPopupView::RemoveSelectedLine() {
  if (selected_line_ == kNoSelection)
    return false;

  DCHECK_GE(selected_line_, 0);
  DCHECK_LT(selected_line_, static_cast<int>(autofill_values_.size()));

  if (!CanDelete(autofill_unique_ids_[selected_line_]))
    return false;

  if (autofill_unique_ids_[selected_line_] > 0) {
    external_delegate()->RemoveAutofillProfileOrCreditCard(
        autofill_unique_ids_[selected_line_]);
  } else {
    external_delegate()->RemoveAutocompleteEntry(
        autofill_values_[selected_line_]);
  }

  // Remove the deleted element.
  autofill_values_.erase(autofill_values_.begin() + selected_line_);
  autofill_labels_.erase(autofill_labels_.begin() + selected_line_);
  autofill_icons_.erase(autofill_icons_.begin() + selected_line_);
  autofill_unique_ids_.erase(autofill_unique_ids_.begin() + selected_line_);

  // Resize the popup.
  ResizePopup();

  SetSelectedLine(kNoSelection);

  if (!HasAutofillEntries())
    Hide();

  return true;
}

bool AutofillPopupView::IsSeparatorIndex(int index) {
  // TODO(csharp): Use WebAutofillClient::MenuItemIDSeparator instead.
  // http://crbug.com/125001
  return (index > 0 && autofill_unique_ids_[index - 1] >= 0 &&
          autofill_unique_ids_[index] < 0);
}

int AutofillPopupView::GetIconResourceID(const string16& resource_name) {
  for (size_t i = 0; i < arraysize(kDataResources); ++i) {
    if (resource_name == ASCIIToUTF16(kDataResources[i].name))
      return kDataResources[i].id;
  }

  return -1;
}

bool AutofillPopupView::CanDelete(int id) {
  return id > 0 ||
      id == WebAutofillClient::MenuItemIDAutocompleteEntry ||
      id == WebAutofillClient::MenuItemIDPasswordEntry;
}

bool AutofillPopupView::HasAutofillEntries() {
  return autofill_values_.size() != 0 &&
      (autofill_unique_ids_[0] > 0 ||
       autofill_unique_ids_[0] ==
         WebAutofillClient::MenuItemIDAutocompleteEntry ||
       autofill_unique_ids_[0] == WebAutofillClient::MenuItemIDPasswordEntry ||
       autofill_unique_ids_[0] == WebAutofillClient::MenuItemIDDataListEntry);
}

void AutofillPopupView::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_WEB_CONTENTS_HIDDEN
      || type == content::NOTIFICATION_NAV_ENTRY_COMMITTED)
    Hide();
}
