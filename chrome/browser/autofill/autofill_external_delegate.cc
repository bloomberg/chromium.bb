// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::RenderViewHost;

AutofillExternalDelegate::~AutofillExternalDelegate() {
}

AutofillExternalDelegate::AutofillExternalDelegate(
    TabContentsWrapper* tab_contents_wrapper,
    AutofillManager* autofill_manager)
    : tab_contents_wrapper_(tab_contents_wrapper),
      autofill_manager_(autofill_manager),
      autofill_query_id_(0),
      display_warning_if_disabled_(false),
      has_shown_autofill_popup_for_current_edit_(false),
      suggestions_clear_index_(-1),
      suggestions_options_index_(-1) {
}

void AutofillExternalDelegate::SelectAutofillSuggestionAtIndex(int unique_id,
                                                               int list_index) {
  if (list_index == suggestions_options_index_ ||
      list_index == suggestions_clear_index_ ||
      unique_id == -1)
    return;

  FillAutofillFormData(unique_id, true);
}

void AutofillExternalDelegate::OnQuery(int query_id,
                                       const webkit::forms::FormData& form,
                                       const webkit::forms::FormField& field,
                                       const gfx::Rect& bounds,
                                       bool display_warning_if_disabled) {
  autofill_query_form_ = form;
  autofill_query_field_ = field;
  display_warning_if_disabled_ = display_warning_if_disabled;
  autofill_query_id_ = query_id;

  OnQueryPlatformSpecific(query_id, form, field, bounds);
}

void AutofillExternalDelegate::OnSuggestionsReturned(
    int query_id,
    const std::vector<string16>& values,
    const std::vector<string16>& labels,
    const std::vector<string16>& icons,
    const std::vector<int>& unique_ids) {
  if (query_id != autofill_query_id_)
    return;

  if (values.empty()) {
    // No suggestions, any popup currently showing is obsolete.
    HideAutofillPopup();
    return;
  }

  std::vector<string16> v(values);
  std::vector<string16> l(labels);
  std::vector<string16> i(icons);
  std::vector<int> ids(unique_ids);
  int separator_index = -1;

  DCHECK_GT(ids.size(), 0U);
  if (!autofill_query_field_.should_autocomplete) {
    // If autofill is disabled and we had suggestions, show a warning instead.
    v.assign(1, l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_FORM_DISABLED));
    l.assign(1, string16());
    i.assign(1, string16());
    ids.assign(1, -1);
  } else if (ids[0] < 0 && ids.size() > 1) {
    // If we received a warning instead of suggestions from autofill but regular
    // suggestions from autocomplete, don't show the autofill warning.
    v.erase(v.begin());
    l.erase(l.begin());
    i.erase(i.begin());
    ids.erase(ids.begin());
  }

  // If we were about to show a warning and we shouldn't, don't.
  if (ids[0] < 0 && !display_warning_if_disabled_)
    return;

  // Only include "Autofill Options" special menu item if we have Autofill
  // items, identified by |unique_ids| having at least one valid value.
  bool has_autofill_item = false;
  for (size_t i = 0; i < ids.size(); ++i) {
    if (ids[i] > 0) {
      has_autofill_item = true;
      break;
    }
  }

  // The form has been auto-filled, so give the user the chance to clear the
  // form.  Append the 'Clear form' menu item.
  if (has_autofill_item &&
      autofill_query_field_.is_autofilled) {
    v.push_back(l10n_util::GetStringUTF16(IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM));
    l.push_back(string16());
    i.push_back(string16());
    ids.push_back(0);
    suggestions_clear_index_ = v.size() - 1;
    separator_index = v.size() - 1;
  }

  if (has_autofill_item) {
    // Append the 'Chrome Autofill options' menu item;
    v.push_back(l10n_util::GetStringUTF16(IDS_AUTOFILL_OPTIONS_POPUP));
    l.push_back(string16());
    i.push_back(string16());
    ids.push_back(0);
    suggestions_options_index_ = v.size() - 1;
    separator_index = values.size();
  }

  // Send to display.
  if (!v.empty() && autofill_query_field_.is_focusable)
    ApplyAutofillSuggestions(v, l, i, ids, separator_index);

  tab_contents_wrapper_->autofill_manager()->OnDidShowAutofillSuggestions(
      has_autofill_item && !has_shown_autofill_popup_for_current_edit_);
  has_shown_autofill_popup_for_current_edit_ |= has_autofill_item;
}

void AutofillExternalDelegate::DidEndTextFieldEditing() {
  has_shown_autofill_popup_for_current_edit_ = false;
}

bool AutofillExternalDelegate::DidAcceptAutofillSuggestions(
    const string16& value,
    int unique_id,
    unsigned index) {
  // If the selected element is a warning we don't want to do anything.
  if (unique_id < 0)
    return false;

  // TODO(csharp): Add the password autofill manager.
  // if (password_autofill_manager_->DidAcceptAutofillSuggestion(node, value))
  //   return;

  if (suggestions_options_index_ != -1 &&
      index == static_cast<unsigned>(suggestions_options_index_)) {
    // User selected 'Autofill Options'.
    autofill_manager_->OnShowAutofillDialog();
  } else if (suggestions_clear_index_ != -1 &&
             index == static_cast<unsigned>(suggestions_clear_index_)) {
    // User selected 'Clear form'.
    RenderViewHost* host =
        tab_contents_wrapper_->web_contents()->GetRenderViewHost();
    host->Send(new AutofillMsg_ClearForm(host->GetRoutingID()));
  } else if (!unique_id) {
    // User selected an Autocomplete entry, so we fill directly.
    RenderViewHost* host =
        tab_contents_wrapper_->web_contents()->GetRenderViewHost();
    host->Send(new AutofillMsg_SetNodeText(
        host->GetRoutingID(),
        value));
  } else {
    FillAutofillFormData(unique_id, false);
  }

  HideAutofillPopup();

  return true;
}

void AutofillExternalDelegate::ClearPreviewedForm() {
  RenderViewHost* host =
      tab_contents_wrapper_->web_contents()->GetRenderViewHost();
  host->Send(new AutofillMsg_ClearPreviewedForm(host->GetRoutingID()));
}

void AutofillExternalDelegate::HideAutofillPopup() {
  suggestions_clear_index_ = -1;
  suggestions_options_index_ = -1;

  HideAutofillPopupInternal();
}

void AutofillExternalDelegate::FillAutofillFormData(int unique_id,
                                                    bool is_preview) {
  RenderViewHost* host =
      tab_contents_wrapper_->web_contents()->GetRenderViewHost();

  if (is_preview) {
    host->Send(new AutofillMsg_SetAutofillActionPreview(
        host->GetRoutingID()));
  } else {
    host->Send(new AutofillMsg_SetAutofillActionFill(
        host->GetRoutingID()));
  }

  // Fill the values for the whole form.
  autofill_manager_->OnFillAutofillFormData(autofill_query_id_,
                                            autofill_query_form_,
                                            autofill_query_field_,
                                            unique_id);
}

// Add a "!defined(OS_YOUROS) for each platform that implements this
// in an autofill_external_delegate_YOUROS.cc.  Currently there are
// none, so all platforms use the default.

#if !defined(OS_ANDROID) && !defined(TOOLKIT_GTK)

AutofillExternalDelegate* AutofillExternalDelegate::Create(
    TabContentsWrapper*, AutofillManager*) {
  return NULL;
}

#endif
