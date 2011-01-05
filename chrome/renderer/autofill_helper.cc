// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/autofill_helper.h"

#include "app/keyboard_codes.h"
#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/renderer/form_manager.h"
#include "chrome/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/password_form.h"

using WebKit::WebFormControlElement;
using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebKeyboardEvent;
using WebKit::WebNode;
using WebKit::WebString;

namespace {

// The size above which we stop triggering autofill for an input text field
// (so to avoid sending long strings through IPC).
const size_t kMaximumTextSizeForAutoFill = 1000;

}  // namespace

AutoFillHelper::AutoFillHelper(RenderView* render_view)
    : render_view_(render_view),
      autofill_query_id_(0),
      autofill_action_(AUTOFILL_NONE),
      display_warning_if_disabled_(false),
      was_query_node_autofilled_(false),
      suggestions_clear_index_(-1),
      suggestions_options_index_(-1) {
}

void AutoFillHelper::RemoveAutocompleteSuggestion(const WebString& name,
                                                  const WebString& value) {
  // The index of clear & options will have shifted down.
  if (suggestions_clear_index_ != -1)
    suggestions_clear_index_--;
  if (suggestions_options_index_ != -1)
    suggestions_options_index_--;

  render_view_->Send(new ViewHostMsg_RemoveAutocompleteEntry(
      render_view_->routing_id(), name, value));
}

void AutoFillHelper::SuggestionsReceived(int query_id,
                                         const std::vector<string16>& values,
                                         const std::vector<string16>& labels,
                                         const std::vector<string16>& icons,
                                         const std::vector<int>& unique_ids) {
  WebKit::WebView* web_view = render_view_->webview();
  if (!web_view || query_id != autofill_query_id_)
    return;

  if (values.empty()) {
    // No suggestions, any popup currently showing is obsolete.
    web_view->hidePopups();
    return;
  }

  std::vector<string16> v(values);
  std::vector<string16> l(labels);
  std::vector<string16> i(icons);
  std::vector<int> ids(unique_ids);
  int separator_index = -1;

  if (ids[0] < 0 && ids.size() > 1) {
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

  // Only include "AutoFill Options" special menu item if we have AutoFill
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
      form_manager_.FormWithNodeIsAutoFilled(autofill_query_node_)) {
    v.push_back(l10n_util::GetStringUTF16(IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM));
    l.push_back(string16());
    i.push_back(string16());
    ids.push_back(0);
    suggestions_clear_index_ = v.size() - 1;
    separator_index = v.size() - 1;
  }

  if (has_autofill_item) {
    // Append the 'Chrome Autofill options' menu item;
    v.push_back(l10n_util::GetStringFUTF16(IDS_AUTOFILL_OPTIONS_POPUP,
        WideToUTF16(chrome::kBrowserAppName)));
    l.push_back(string16());
    i.push_back(string16());
    ids.push_back(0);
    suggestions_options_index_ = v.size() - 1;
    separator_index = values.size();
  }

  // Send to WebKit for display.
  if (!v.empty() && autofill_query_node_.hasNonEmptyBoundingBox()) {
    web_view->applyAutoFillSuggestions(
        autofill_query_node_, v, l, i, ids, separator_index);
  }

  render_view_->Send(new ViewHostMsg_DidShowAutoFillSuggestions(
      render_view_->routing_id()));
}

void AutoFillHelper::FormDataFilled(int query_id,
                                    const webkit_glue::FormData& form) {
  if (!render_view_->webview() || query_id != autofill_query_id_)
    return;

  switch (autofill_action_) {
    case AUTOFILL_FILL:
      form_manager_.FillForm(form, autofill_query_node_);
      break;
    case AUTOFILL_PREVIEW:
      form_manager_.PreviewForm(form, autofill_query_node_);
      break;
    default:
      NOTREACHED();
  }
  autofill_action_ = AUTOFILL_NONE;
  render_view_->Send(new ViewHostMsg_DidFillAutoFillFormData(
      render_view_->routing_id()));
}

void AutoFillHelper::DidSelectAutoFillSuggestion(const WebNode& node,
                                                 int unique_id) {
  DCHECK_GE(unique_id, 0);

  DidClearAutoFillSelection(node);
  FillAutoFillFormData(node, unique_id, AUTOFILL_PREVIEW);
}

void AutoFillHelper::DidAcceptAutoFillSuggestion(const WebNode& node,
                                                 const WebString& value,
                                                 int unique_id,
                                                 unsigned index) {
  if (suggestions_options_index_ != -1 &&
      index == static_cast<unsigned>(suggestions_options_index_)) {
    // User selected 'AutoFill Options'.
    render_view_->Send(new ViewHostMsg_ShowAutoFillDialog(
        render_view_->routing_id()));
  } else if (suggestions_clear_index_ != -1 &&
             index == static_cast<unsigned>(suggestions_clear_index_)) {
    // User selected 'Clear form'.
    form_manager_.ClearFormWithNode(node);
  } else if (!unique_id) {
    // User selected an Autocomplete entry, so we fill directly.
    WebInputElement element = node.toConst<WebInputElement>();

    string16 substring = value;
    substring = substring.substr(0, element.maxLength());
    element.setValue(substring);

    WebFrame* webframe = node.document().frame();
    if (webframe)
      webframe->notifiyPasswordListenerOfAutocomplete(element);
  } else {
    // Fill the values for the whole form.
    FillAutoFillFormData(node, unique_id, AUTOFILL_FILL);
  }

  suggestions_clear_index_ = -1;
  suggestions_options_index_ = -1;
}

void AutoFillHelper::DidClearAutoFillSelection(const WebNode& node) {
  form_manager_.ClearPreviewedFormWithNode(node, was_query_node_autofilled_);
}

void AutoFillHelper::FrameContentsAvailable(WebFrame* frame) {
  form_manager_.ExtractForms(frame);
  SendForms(frame);
}

void AutoFillHelper::FrameWillClose(WebFrame* frame) {
  form_manager_.ResetFrame(frame);
}

void AutoFillHelper::FrameDetached(WebFrame* frame) {
  form_manager_.ResetFrame(frame);
}

void AutoFillHelper::TextDidChangeInTextField(const WebInputElement& element) {
  ShowSuggestions(element, false, true, false);
}

void AutoFillHelper::KeyDownInTextField(const WebInputElement& element,
                                        const WebKeyboardEvent& event) {
  if (event.windowsKeyCode == app::VKEY_DOWN ||
      event.windowsKeyCode == app::VKEY_UP)
    ShowSuggestions(element, true, true, true);
}

bool AutoFillHelper::InputElementClicked(const WebInputElement& element,
                                         bool was_focused,
                                         bool is_focused) {
  if (was_focused)
    ShowSuggestions(element, true, false, true);
  return false;
}

void AutoFillHelper::ShowSuggestions(const WebInputElement& element,
                                     bool autofill_on_empty_values,
                                     bool requires_caret_at_end,
                                     bool display_warning_if_disabled) {
  if (!element.isEnabledFormControl() || !element.isTextField() ||
      element.isPasswordField() || element.isReadOnly() ||
      !element.autoComplete())
    return;

  // If the field has no name, then we won't have values.
  if (element.nameForAutofill().isEmpty())
    return;

  // Don't attempt to autofill with values that are too large.
  WebString value = element.value();
  if (value.length() > kMaximumTextSizeForAutoFill)
    return;

  if (!autofill_on_empty_values && value.isEmpty())
    return;

  if (requires_caret_at_end &&
      (element.selectionStart() != element.selectionEnd() ||
       element.selectionEnd() != static_cast<int>(value.length())))
    return;

  QueryAutoFillSuggestions(element, display_warning_if_disabled);
}

void AutoFillHelper::QueryAutoFillSuggestions(
    const WebNode& node, bool display_warning_if_disabled) {
  static int query_counter = 0;
  autofill_query_id_ = query_counter++;
  autofill_query_node_ = node;
  display_warning_if_disabled_ = display_warning_if_disabled;

  webkit_glue::FormData form;
  webkit_glue::FormField field;
  if (!FindFormAndFieldForNode(node, &form, &field))
    return;

  render_view_->Send(new ViewHostMsg_QueryFormFieldAutoFill(
      render_view_->routing_id(), autofill_query_id_, form, field));
}

void AutoFillHelper::FillAutoFillFormData(const WebNode& node,
                                           int unique_id,
                                           AutoFillAction action) {
  static int query_counter = 0;
  autofill_query_id_ = query_counter++;

  webkit_glue::FormData form;
  webkit_glue::FormField field;
  if (!FindFormAndFieldForNode(node, &form, &field))
    return;

  autofill_action_ = action;
  was_query_node_autofilled_ = field.is_autofilled();
  render_view_->Send(new ViewHostMsg_FillAutoFillFormData(
      render_view_->routing_id(), autofill_query_id_, form, field, unique_id));
}

void AutoFillHelper::SendForms(WebFrame* frame) {
  // TODO(jhawkins): Use FormManager once we have strict ordering of form
  // control elements in the cache.
  WebKit::WebVector<WebFormElement> web_forms;
  frame->forms(web_forms);

  std::vector<webkit_glue::FormData> forms;
  for (size_t i = 0; i < web_forms.size(); ++i) {
    const WebFormElement& web_form = web_forms[i];

    webkit_glue::FormData form;
    if (FormManager::WebFormElementToFormData(
            web_form, FormManager::REQUIRE_NONE,
            FormManager::EXTRACT_NONE, &form)) {
      forms.push_back(form);
    }
  }

  if (!forms.empty()) {
    render_view_->Send(new ViewHostMsg_FormsSeen(render_view_->routing_id(),
                                                 forms));
  }
}

bool AutoFillHelper::FindFormAndFieldForNode(const WebNode& node,
                                             webkit_glue::FormData* form,
                                             webkit_glue::FormField* field) {
  const WebInputElement& element = node.toConst<WebInputElement>();
  if (!form_manager_.FindFormWithFormControlElement(element,
                                                    FormManager::REQUIRE_NONE,
                                                    form))
    return false;

  FormManager::WebFormControlElementToFormField(element,
                                                FormManager::EXTRACT_VALUE,
                                                field);

  // WebFormControlElementToFormField does not scrape the DOM for the field
  // label, so find the label here.
  // TODO(jhawkins): Add form and field identities so we can use the cached form
  // data in FormManager.
  field->set_label(FormManager::LabelForElement(element));

  return true;
}
