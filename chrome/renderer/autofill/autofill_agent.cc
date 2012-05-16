// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNodeCollection.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebOptionElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/forms/form_data.h"
#include "webkit/forms/form_data_predictions.h"
#include "webkit/forms/form_field.h"
#include "webkit/forms/password_form.h"

using WebKit::WebAutofillClient;
using WebKit::WebFormControlElement;
using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebInputElement;
using WebKit::WebKeyboardEvent;
using WebKit::WebNode;
using WebKit::WebNodeCollection;
using WebKit::WebOptionElement;
using WebKit::WebString;
using webkit::forms::FormData;
using webkit::forms::FormDataPredictions;

namespace {

// The size above which we stop triggering autofill for an input text field
// (so to avoid sending long strings through IPC).
const size_t kMaximumTextSizeForAutofill = 1000;

void AppendDataListSuggestions(const WebKit::WebInputElement& element,
                               std::vector<string16>* values,
                               std::vector<string16>* labels,
                               std::vector<string16>* icons,
                               std::vector<int>* item_ids) {
  WebNodeCollection options = element.dataListOptions();
  if (options.isNull())
    return;

  for (WebOptionElement option = options.firstItem().to<WebOptionElement>();
      !option.isNull(); option = options.nextItem().to<WebOptionElement>()) {
    if (!StartsWith(option.value(), element.value(), false) ||
        !element.isValidValue(option.value()))
      continue;

    values->push_back(option.value());
    if (option.value() != option.label())
      labels->push_back(option.label());
    else
      labels->push_back(string16());
    icons->push_back(string16());
    item_ids->push_back(WebAutofillClient::MenuItemIDDataListEntry);
  }
}

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
    IPC_MESSAGE_HANDLER(AutofillMsg_SetAutofillActionFill,
                        OnSetAutofillActionFill)
    IPC_MESSAGE_HANDLER(AutofillMsg_ClearForm,
                        OnClearForm)
    IPC_MESSAGE_HANDLER(AutofillMsg_SetAutofillActionPreview,
                        OnSetAutofillActionPreview)
    IPC_MESSAGE_HANDLER(AutofillMsg_ClearPreviewedForm,
                        OnClearPreviewedForm)
    IPC_MESSAGE_HANDLER(AutofillMsg_SetNodeText,
                        OnSetNodeText)
    IPC_MESSAGE_HANDLER(AutofillMsg_AcceptPasswordAutofillSuggestion,
                        OnAcceptPasswordAutofillSuggestion)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AutofillAgent::DidFinishDocumentLoad(WebFrame* frame) {
  // The document has now been fully loaded.  Scan for forms to be sent up to
  // the browser.
  std::vector<webkit::forms::FormData> forms;
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

void AutofillAgent::ZoomLevelChanged() {
  // Any time the zoom level changes, the page's content moves, so any Autofill
  // popups should be hidden. This is only needed for the new Autofill UI
  // because WebKit already knows to hide the old UI when this occurs.
  Send(new AutofillHostMsg_HideAutofillPopup(routing_id()));
}

void AutofillAgent::DidChangeScrollOffset(WebKit::WebFrame*) {
  // Any time the scroll offset changes, the page's content moves, so Autofill
  // popups should be hidden. This is only needed for the new Autofill UI
  // because WebKit already knows to hide the old UI when this occurs.
  Send(new AutofillHostMsg_HideAutofillPopup(routing_id()));
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
                                                int item_id,
                                                unsigned index) {
  if (password_autofill_manager_->DidAcceptAutofillSuggestion(node, value))
    return;

  DCHECK(node == element_);

  switch (item_id) {
    case WebAutofillClient::MenuItemIDWarningMessage:
    case WebAutofillClient::MenuItemIDSeparator:
      NOTREACHED();
      break;
    case WebAutofillClient::MenuItemIDAutofillOptions:
      // User selected 'Autofill Options'.
      Send(new AutofillHostMsg_ShowAutofillDialog(routing_id()));
      break;
    case WebAutofillClient::MenuItemIDClearForm:
      // User selected 'Clear form'.
      form_cache_.ClearFormWithElement(element_);
      break;
    case WebAutofillClient::MenuItemIDAutocompleteEntry:
    case WebAutofillClient::MenuItemIDPasswordEntry:
    case WebAutofillClient::MenuItemIDDataListEntry:
      // User selected an Autocomplete or password or datalist entry, so we
      // fill directly.
      SetNodeText(value, &element_);
      break;
    default:
      // A positive item_id is a unique id for an autofill (vs. autocomplete)
      // suggestion.
      DCHECK_GT(item_id, 0);
      // Fill the values for the whole form.
      FillAutofillFormData(node, item_id, AUTOFILL_FILL);
  }
}

void AutofillAgent::didSelectAutofillSuggestion(const WebNode& node,
                                                const WebString& value,
                                                const WebString& label,
                                                int item_id) {
  if (password_autofill_manager_->DidSelectAutofillSuggestion(node))
    return;

  didClearAutofillSelection(node);

  if (item_id > 0)
    FillAutofillFormData(node, item_id, AUTOFILL_PREVIEW);
}

void AutofillAgent::didClearAutofillSelection(const WebNode& node) {
  if (password_autofill_manager_->DidClearAutofillSelection(node))
    return;

  if (!element_.isNull() && node == element_) {
    ClearPreviewedFormWithElement(element_, was_query_node_autofilled_);
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
  if (password_autofill_manager_->TextDidChangeInTextField(element)) {
    element_ = element;
    return;
  }

  ShowSuggestions(element, false, true, false);

  webkit::forms::FormData form;
  webkit::forms::FormField field;
  if (FindFormAndFieldForInputElement(element, &form, &field, REQUIRE_NONE)) {
    Send(new AutofillHostMsg_TextFieldDidChange(routing_id(), form, field,
                                                base::TimeTicks::Now()));
  }
}

void AutofillAgent::textFieldDidReceiveKeyDown(const WebInputElement& element,
                                               const WebKeyboardEvent& event) {
  if (password_autofill_manager_->TextFieldHandlingKeyDown(element, event)) {
    element_ = element;
    return;
  }

  if (event.windowsKeyCode == ui::VKEY_DOWN ||
      event.windowsKeyCode == ui::VKEY_UP)
    ShowSuggestions(element, true, true, true);
}

void AutofillAgent::OnSuggestionsReturned(int query_id,
                                          const std::vector<string16>& values,
                                          const std::vector<string16>& labels,
                                          const std::vector<string16>& icons,
                                          const std::vector<int>& unique_ids) {
  if (query_id != autofill_query_id_)
    return;

  if (element_.isNull() || !element_.isFocusable())
    return;

  std::vector<string16> v(values);
  std::vector<string16> l(labels);
  std::vector<string16> i(icons);
  std::vector<int> ids(unique_ids);

  if (!element_.autoComplete() && !v.empty()) {
    // If autofill is disabled and we had suggestions, show a warning instead.
    v.assign(1, l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_FORM_DISABLED));
    l.assign(1, string16());
    i.assign(1, string16());
    ids.assign(1, WebAutofillClient::MenuItemIDWarningMessage);
  } else if (ids.size() > 1 &&
             ids[0] == WebAutofillClient::MenuItemIDWarningMessage) {
    // If we received an autofill warning plus some autocomplete suggestions,
    // remove the autofill warning.
    v.erase(v.begin());
    l.erase(l.begin());
    i.erase(i.begin());
    ids.erase(ids.begin());
  }

  // If we were about to show a warning and we shouldn't, don't.
  if (!display_warning_if_disabled_ && !v.empty() &&
      ids[0] == WebAutofillClient::MenuItemIDWarningMessage) {
    v.clear();
    l.clear();
    i.clear();
    ids.clear();
  }

  // Only include "Autofill Options" special menu item if we have Autofill
  // items, identified by |unique_ids| having at least one valid value.
  bool has_autofill_item = false;
  for (size_t i = 0; i < ids.size(); ++i) {
    if (ids[i] > 0) {
      has_autofill_item = true;
      break;
    }
  }

  if (has_autofill_item) {
    v.push_back(string16());
    l.push_back(string16());
    i.push_back(string16());
    ids.push_back(WebAutofillClient::MenuItemIDSeparator);

    if (FormWithElementIsAutofilled(element_)) {
      // The form has been auto-filled, so give the user the chance to clear the
      // form.  Append the 'Clear form' menu item.
      v.push_back(l10n_util::GetStringUTF16(IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM));
      l.push_back(string16());
      i.push_back(string16());
      ids.push_back(WebAutofillClient::MenuItemIDClearForm);
    }

    // Append the 'Chrome Autofill options' menu item;
    v.push_back(l10n_util::GetStringUTF16(IDS_AUTOFILL_OPTIONS_POPUP));
    l.push_back(string16());
    i.push_back(string16());
    ids.push_back(WebAutofillClient::MenuItemIDAutofillOptions);
  }

  CombineDataListEntriesAndShow(element_, v, l, i, ids, has_autofill_item);
}

void AutofillAgent::CombineDataListEntriesAndShow(
    const WebKit::WebInputElement& element,
    const std::vector<string16>& values,
    const std::vector<string16>& labels,
    const std::vector<string16>& icons,
    const std::vector<int>& item_ids,
    bool has_autofill_item) {
  std::vector<string16> v;
  std::vector<string16> l;
  std::vector<string16> i;
  std::vector<int> ids;

  AppendDataListSuggestions(element, &v, &l, &i, &ids);

  // If there are both <datalist> items and Autofill suggestions, add a
  // separator between them.
  if (!v.empty() && !values.empty()) {
    v.push_back(string16());
    l.push_back(string16());
    i.push_back(string16());
    ids.push_back(WebAutofillClient::MenuItemIDSeparator);
  }

  // Append the Autofill suggestions.
  v.insert(v.end(), values.begin(), values.end());
  l.insert(l.end(), labels.begin(), labels.end());
  i.insert(i.end(), icons.begin(), icons.end());
  ids.insert(ids.end(), item_ids.begin(), item_ids.end());

  WebKit::WebView* web_view = render_view()->GetWebView();
  if (!web_view)
    return;

  if (v.empty()) {
    // No suggestions, any popup currently showing is obsolete.
    web_view->hidePopups();
    return;
  }

  // Send to WebKit for display.
  web_view->applyAutofillSuggestions(element, v, l, i, ids);

  Send(new AutofillHostMsg_DidShowAutofillSuggestions(
      routing_id(),
      has_autofill_item && !has_shown_autofill_popup_for_current_edit_));
  has_shown_autofill_popup_for_current_edit_ |= has_autofill_item;
}

void AutofillAgent::OnFormDataFilled(int query_id,
                                     const webkit::forms::FormData& form) {
  if (!render_view()->GetWebView() || query_id != autofill_query_id_)
    return;

  was_query_node_autofilled_ = element_.isAutofilled();

  switch (autofill_action_) {
    case AUTOFILL_FILL:
      FillForm(form, element_);
      Send(new AutofillHostMsg_DidFillAutofillFormData(routing_id(),
                                                       base::TimeTicks::Now()));
      break;
    case AUTOFILL_PREVIEW:
      PreviewForm(form, element_);
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

void AutofillAgent::OnSetAutofillActionFill() {
  autofill_action_ = AUTOFILL_FILL;
}

void AutofillAgent::OnClearForm() {
  form_cache_.ClearFormWithElement(element_);
}

void AutofillAgent::OnSetAutofillActionPreview() {
  autofill_action_ = AUTOFILL_PREVIEW;
}

void AutofillAgent::OnClearPreviewedForm() {
  didClearAutofillSelection(element_);
}

void AutofillAgent::OnSetNodeText(const string16& value) {
  SetNodeText(value, &element_);
}

void AutofillAgent::OnAcceptPasswordAutofillSuggestion(const string16& value) {
  // We need to make sure this is handled here because the browser process
  // skipped it handling because it believed it would be handled here. If it
  // isn't handled here then the browser logic needs to be updated.
  bool handled = password_autofill_manager_->DidAcceptAutofillSuggestion(
      element_,
      value);
  DCHECK(handled);
}

void AutofillAgent::ShowSuggestions(const WebInputElement& element,
                                    bool autofill_on_empty_values,
                                    bool requires_caret_at_end,
                                    bool display_warning_if_disabled) {
  if (!element.isEnabled() || element.isReadOnly() || !element.isTextField() ||
      element.isPasswordField() || !element.suggestedValue().isEmpty())
    return;

  // Don't attempt to autofill with values that are too large or if filling
  // criteria are not met.
  WebString value = element.value();
  if (value.length() > kMaximumTextSizeForAutofill ||
      (!autofill_on_empty_values && value.isEmpty()) ||
      (requires_caret_at_end &&
       (element.selectionStart() != element.selectionEnd() ||
        element.selectionEnd() != static_cast<int>(value.length())))) {
    // Any popup currently showing is obsolete.
    WebKit::WebView* web_view = render_view()->GetWebView();
    if (web_view)
      web_view->hidePopups();

    return;
  }

  element_ = element;

  // If autocomplete is disabled at the form level, then we might want to show
  // a warning in place of suggestions. However, if autocomplete is disabled
  // specifically for this field, we never want to show a warning. Otherwise,
  // we might interfere with custom popups (e.g. search suggestions) used by
  // the website. Also, if the field has no name, then we won't have values.
  const WebFormElement form = element.form();
  if ((!element.autoComplete() && (form.isNull() || form.autoComplete())) ||
      element.nameForAutofill().isEmpty()) {
    CombineDataListEntriesAndShow(element, std::vector<string16>(),
                                  std::vector<string16>(),
                                  std::vector<string16>(),
                                  std::vector<int>(), false);
    return;
  }

  QueryAutofillSuggestions(element, display_warning_if_disabled);
}

void AutofillAgent::QueryAutofillSuggestions(const WebInputElement& element,
                                             bool display_warning_if_disabled) {
  static int query_counter = 0;
  autofill_query_id_ = query_counter++;
  display_warning_if_disabled_ = display_warning_if_disabled;

  webkit::forms::FormData form;
  webkit::forms::FormField field;
  if (!FindFormAndFieldForInputElement(element, &form, &field,
                                       REQUIRE_AUTOCOMPLETE)) {
    // If we didn't find the cached form, at least let autocomplete have a shot
    // at providing suggestions.
    WebFormControlElementToFormField(element, EXTRACT_VALUE, &field);
  }

  gfx::Rect bounding_box(element_.boundsInViewportSpace());

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
  DCHECK_GT(unique_id, 0);

  static int query_counter = 0;
  autofill_query_id_ = query_counter++;

  webkit::forms::FormData form;
  webkit::forms::FormField field;
  if (!FindFormAndFieldForInputElement(node.toConst<WebInputElement>(), &form,
                                       &field, REQUIRE_AUTOCOMPLETE)) {
    return;
  }

  autofill_action_ = action;
  Send(new AutofillHostMsg_FillAutofillFormData(
      routing_id(), autofill_query_id_, form, field, unique_id));
}

void AutofillAgent::SetNodeText(const string16& value,
                                WebKit::WebInputElement* node) {
  string16 substring = value;
  substring = substring.substr(0, node->maxLength());

  node->setValue(substring, true);
}

}  // namespace autofill
