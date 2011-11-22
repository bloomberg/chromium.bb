// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/autofill/autofill_agent.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/renderer/autofill/form_autofill_util.h"
#include "chrome/renderer/autofill/password_autofill_manager.h"
#include "content/public/renderer/render_view.h"
#include "grit/generated_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_data_predictions.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/password_form.h"

using WebKit::WebFormControlElement;
using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebKeyboardEvent;
using WebKit::WebNode;
using WebKit::WebString;
using webkit_glue::FormData;
using webkit_glue::FormDataPredictions;

namespace {

// The size above which we stop triggering autofill for an input text field
// (so to avoid sending long strings through IPC).
const size_t kMaximumTextSizeForAutofill = 1000;

}  // namespace

namespace autofill {

AutofillAgent::AutofillAgent(
    content::RenderView* render_view,
    PasswordAutofillManager* password_autofill_manager)
    : content::RenderViewObserver(render_view),
      password_autofill_manager_(password_autofill_manager),
      autofill_query_id_(0),
      autofill_action_(AUTOFILL_NONE),
      display_warning_if_disabled_(false),
      was_query_node_autofilled_(false),
      suggestions_clear_index_(-1),
      suggestions_options_index_(-1),
      has_shown_autofill_popup_for_current_edit_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  render_view->GetWebView()->setAutofillClient(this);
}

AutofillAgent::~AutofillAgent() {}

bool AutofillAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AutofillAgent, message)
    IPC_MESSAGE_HANDLER(AutofillMsg_SuggestionsReturned, OnSuggestionsReturned)
    IPC_MESSAGE_HANDLER(AutofillMsg_FormDataFilled, OnFormDataFilled)
    IPC_MESSAGE_HANDLER(AutofillMsg_FieldTypePredictionsAvailable,
                        OnFieldTypePredictionsAvailable)
    IPC_MESSAGE_HANDLER(AutofillMsg_SelectAutofillSuggestionAtIndex,
                        OnSelectAutofillSuggestionAtIndex)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AutofillAgent::DidFinishDocumentLoad(WebFrame* frame) {
  // The document has now been fully loaded.  Scan for forms to be sent up to
  // the browser.
  std::vector<webkit_glue::FormData> forms;
  form_cache_.ExtractForms(*frame, &forms);

  if (!forms.empty()) {
    Send(new AutofillHostMsg_FormsSeen(routing_id(), forms,
                                       base::TimeTicks::Now()));
  }
}

void AutofillAgent::FrameDetached(WebFrame* frame) {
  form_cache_.ResetFrame(*frame);
}

void AutofillAgent::FrameWillClose(WebFrame* frame) {
  form_cache_.ResetFrame(*frame);
}

void AutofillAgent::WillSubmitForm(WebFrame* frame,
                                   const WebFormElement& form) {
  FormData form_data;
  if (WebFormElementToFormData(form,
                               WebFormControlElement(),
                               REQUIRE_AUTOCOMPLETE,
                               static_cast<ExtractMask>(
                                   EXTRACT_VALUE | EXTRACT_OPTION_TEXT),
                               &form_data,
                               NULL)) {
    Send(new AutofillHostMsg_FormSubmitted(routing_id(), form_data,
                                           base::TimeTicks::Now()));
  }
}

bool AutofillAgent::InputElementClicked(const WebInputElement& element,
                                        bool was_focused,
                                        bool is_focused) {
  if (was_focused)
    ShowSuggestions(element, true, false, true);

  return false;
}

bool AutofillAgent::InputElementLostFocus() {
  Send(new AutofillHostMsg_HideAutofillPopup(routing_id()));

  return false;
}

void AutofillAgent::didAcceptAutofillSuggestion(const WebNode& node,
                                                const WebString& value,
                                                const WebString& label,
                                                int unique_id,
                                                unsigned index) {
  if (password_autofill_manager_->DidAcceptAutofillSuggestion(node, value))
    return;

  if (suggestions_options_index_ != -1 &&
      index == static_cast<unsigned>(suggestions_options_index_)) {
    // User selected 'Autofill Options'.
    Send(new AutofillHostMsg_ShowAutofillDialog(routing_id()));
  } else if (suggestions_clear_index_ != -1 &&
             index == static_cast<unsigned>(suggestions_clear_index_)) {
    // User selected 'Clear form'.
    DCHECK(node == autofill_query_element_);
    form_cache_.ClearFormWithElement(autofill_query_element_);
  } else if (!unique_id) {
    // User selected an Autocomplete entry, so we fill directly.
    WebInputElement element = node.toConst<WebInputElement>();

    string16 substring = value;
    substring = substring.substr(0, element.maxLength());
    element.setValue(substring, true);
  } else {
    // Fill the values for the whole form.
    FillAutofillFormData(node, unique_id, AUTOFILL_FILL);
  }

  suggestions_clear_index_ = -1;
  suggestions_options_index_ = -1;
}

void AutofillAgent::didSelectAutofillSuggestion(const WebNode& node,
                                                const WebString& value,
                                                const WebString& label,
                                                int unique_id) {
  DCHECK_GE(unique_id, 0);
  if (password_autofill_manager_->DidSelectAutofillSuggestion(node))
    return;

  didClearAutofillSelection(node);
  FillAutofillFormData(node, unique_id, AUTOFILL_PREVIEW);
}

void AutofillAgent::didClearAutofillSelection(const WebNode& node) {
  if (password_autofill_manager_->DidClearAutofillSelection(node))
    return;

  if (!autofill_query_element_.isNull() && node == autofill_query_element_) {
    ClearPreviewedFormWithElement(autofill_query_element_,
                                  was_query_node_autofilled_);
  } else {
    // TODO(isherman): There seem to be rare cases where this code *is*
    // reachable: see [ http://crbug.com/96321#c6 ].  Ideally we would
    // understand those cases and fix the code to avoid them.  However, so far I
    // have been unable to reproduce such a case locally.  If you hit this
    // NOTREACHED(), please file a bug against me.
    NOTREACHED();
  }
}

void AutofillAgent::removeAutocompleteSuggestion(const WebString& name,
                                                 const WebString& value) {
  // The index of clear & options will have shifted down.
  if (suggestions_clear_index_ != -1)
    suggestions_clear_index_--;
  if (suggestions_options_index_ != -1)
    suggestions_options_index_--;

  Send(new AutofillHostMsg_RemoveAutocompleteEntry(routing_id(), name, value));
}

void AutofillAgent::textFieldDidEndEditing(const WebInputElement& element) {
  password_autofill_manager_->TextFieldDidEndEditing(element);
  has_shown_autofill_popup_for_current_edit_ = false;
  Send(new AutofillHostMsg_DidEndTextFieldEditing(routing_id()));
}

void AutofillAgent::textFieldDidChange(const WebInputElement& element) {
  // We post a task for doing the Autofill as the caret position is not set
  // properly at this point (http://bugs.webkit.org/show_bug.cgi?id=16976) and
  // it is needed to trigger autofill.
  weak_ptr_factory_.InvalidateWeakPtrs();
  MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AutofillAgent::TextFieldDidChangeImpl,
                   weak_ptr_factory_.GetWeakPtr(), element));
}

void AutofillAgent::TextFieldDidChangeImpl(const WebInputElement& element) {
  if (password_autofill_manager_->TextDidChangeInTextField(element))
    return;

  ShowSuggestions(element, false, true, false);

  webkit_glue::FormData form;
  webkit_glue::FormField field;
  if (FindFormAndFieldForInputElement(element, &form, &field, REQUIRE_NONE)) {
    Send(new AutofillHostMsg_TextFieldDidChange(routing_id(), form, field,
                                                base::TimeTicks::Now()));
  }
}

void AutofillAgent::textFieldDidReceiveKeyDown(const WebInputElement& element,
                                               const WebKeyboardEvent& event) {
  if (password_autofill_manager_->TextFieldHandlingKeyDown(element, event))
    return;

  if (event.windowsKeyCode == ui::VKEY_DOWN ||
      event.windowsKeyCode == ui::VKEY_UP)
    ShowSuggestions(element, true, true, true);
}

void AutofillAgent::OnSuggestionsReturned(int query_id,
                                          const std::vector<string16>& values,
                                          const std::vector<string16>& labels,
                                          const std::vector<string16>& icons,
                                          const std::vector<int>& unique_ids) {
  WebKit::WebView* web_view = render_view()->GetWebView();
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

  DCHECK_GT(ids.size(), 0U);
  if (!autofill_query_element_.isNull() &&
      !autofill_query_element_.autoComplete()) {
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
      FormWithElementIsAutofilled(autofill_query_element_)) {
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
  if (!v.empty() && !autofill_query_element_.isNull() &&
      autofill_query_element_.isFocusable()) {
    web_view->applyAutofillSuggestions(
        autofill_query_element_, v, l, i, ids, separator_index);
  }

  Send(new AutofillHostMsg_DidShowAutofillSuggestions(
      routing_id(),
      has_autofill_item && !has_shown_autofill_popup_for_current_edit_));
  has_shown_autofill_popup_for_current_edit_ |= has_autofill_item;
}

void AutofillAgent::OnFormDataFilled(int query_id,
                                     const webkit_glue::FormData& form) {
  if (!render_view()->GetWebView() || query_id != autofill_query_id_)
    return;

  switch (autofill_action_) {
    case AUTOFILL_FILL:
      FillForm(form, autofill_query_element_);
      Send(new AutofillHostMsg_DidFillAutofillFormData(routing_id(),
                                                       base::TimeTicks::Now()));
      break;
    case AUTOFILL_PREVIEW:
      PreviewForm(form, autofill_query_element_);
      Send(new AutofillHostMsg_DidPreviewAutofillFormData(routing_id()));
      break;
    default:
      NOTREACHED();
  }
  autofill_action_ = AUTOFILL_NONE;
}

void AutofillAgent::OnFieldTypePredictionsAvailable(
    const std::vector<FormDataPredictions>& forms) {
  for (size_t i = 0; i < forms.size(); ++i) {
    form_cache_.ShowPredictions(forms[i]);
  }
}

void AutofillAgent::OnSelectAutofillSuggestionAtIndex(int listIndex) {
  NOTIMPLEMENTED();
  // TODO(jrg): enable once changes land in WebKit
  // render_view()->webview()->selectAutofillSuggestionAtIndex(listIndex);
}

void AutofillAgent::ShowSuggestions(const WebInputElement& element,
                                    bool autofill_on_empty_values,
                                    bool requires_caret_at_end,
                                    bool display_warning_if_disabled) {
  // If autocomplete is disabled at the form level, then we might want to show
  // a warning in place of suggestions. However, if autocomplete is disabled
  // specifically for this field, we never want to show a warning. Otherwise,
  // we might interfere with custom popups (e.g. search suggestions) used by
  // the website.
  const WebFormElement form = element.form();
  if (!element.isEnabled() || element.isReadOnly() ||
      (!element.autoComplete() && (form.isNull() || form.autoComplete())) ||
      !element.isTextField() || element.isPasswordField() ||
      !element.suggestedValue().isEmpty())
    return;

  // If the field has no name, then we won't have values.
  if (element.nameForAutofill().isEmpty())
    return;

  // Don't attempt to autofill with values that are too large.
  WebString value = element.value();
  if (value.length() > kMaximumTextSizeForAutofill)
    return;

  if (!autofill_on_empty_values && value.isEmpty())
    return;

  if (requires_caret_at_end &&
      (element.selectionStart() != element.selectionEnd() ||
       element.selectionEnd() != static_cast<int>(value.length())))
    return;

  QueryAutofillSuggestions(element, display_warning_if_disabled);
}

void AutofillAgent::QueryAutofillSuggestions(const WebInputElement& element,
                                             bool display_warning_if_disabled) {
  static int query_counter = 0;
  autofill_query_id_ = query_counter++;
  autofill_query_element_ = element;
  display_warning_if_disabled_ = display_warning_if_disabled;

  webkit_glue::FormData form;
  webkit_glue::FormField field;
  if (!FindFormAndFieldForInputElement(element, &form, &field,
                                       REQUIRE_AUTOCOMPLETE)) {
    // If we didn't find the cached form, at least let autocomplete have a shot
    // at providing suggestions.
    WebFormControlElementToFormField(element, EXTRACT_VALUE, &field);
  }

  // TODO(csharp): Stop using the hardcoded value once the WebKit change to
  // expose the position lands.
  // gfx::Rect bounding_box(autofill_query_element_.boundsInRootViewSpace());
  gfx::Rect bounding_box(26, 51, 155, 22);

  Send(new AutofillHostMsg_QueryFormFieldAutofill(routing_id(),
                                                  autofill_query_id_,
                                                  form,
                                                  field,
                                                  bounding_box,
                                                  display_warning_if_disabled));
}

void AutofillAgent::FillAutofillFormData(const WebNode& node,
                                         int unique_id,
                                         AutofillAction action) {
  static int query_counter = 0;
  autofill_query_id_ = query_counter++;

  webkit_glue::FormData form;
  webkit_glue::FormField field;
  if (!FindFormAndFieldForInputElement(node.toConst<WebInputElement>(), &form,
                                       &field, REQUIRE_AUTOCOMPLETE)) {
    return;
  }

  autofill_action_ = action;
  was_query_node_autofilled_ = field.is_autofilled;
  Send(new AutofillHostMsg_FillAutofillFormData(
      routing_id(), autofill_query_id_, form, field, unique_id));
}

}  // namespace autofill
