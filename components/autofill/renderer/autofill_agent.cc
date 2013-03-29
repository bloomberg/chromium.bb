// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/renderer/autofill_agent.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "components/autofill/common/autocheckout_status.h"
#include "components/autofill/common/autofill_constants.h"
#include "components/autofill/common/autofill_messages.h"
#include "components/autofill/common/autofill_switches.h"
#include "components/autofill/common/form_data.h"
#include "components/autofill/common/form_data_predictions.h"
#include "components/autofill/common/form_field_data.h"
#include "components/autofill/common/web_element_descriptor.h"
#include "components/autofill/renderer/form_autofill_util.h"
#include "components/autofill/renderer/password_autofill_manager.h"
#include "content/public/common/password_form.h"
#include "content/public/common/ssl_status.h"
#include "content/public/renderer/render_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebRect.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormControlElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNodeCollection.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebOptionElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"

using components::autofill::kRequiredAutofillFields;
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

namespace {

// The size above which we stop triggering autofill for an input text field
// (so to avoid sending long strings through IPC).
const size_t kMaximumTextSizeForAutofill = 1000;

// The maximum number of data list elements to send to the browser process
// via IPC (to prevent long IPC messages).
const size_t kMaximumDataListSizeForAutofill = 30;

const int kAutocheckoutClickTimeout = 3;

void AppendDataListSuggestions(const WebKit::WebInputElement& element,
                               std::vector<string16>* values,
                               std::vector<string16>* labels,
                               std::vector<string16>* icons,
                               std::vector<int>* item_ids) {
  WebNodeCollection options = element.dataListOptions();
  if (options.isNull())
    return;

  string16 prefix = element.editingValue();
  if (element.isMultiple() &&
      element.formControlType() == WebString::fromUTF8("email")) {
    std::vector<string16> parts;
    base::SplitStringDontTrim(prefix, ',', &parts);
    if (parts.size() > 0)
      TrimWhitespace(parts[parts.size() - 1], TRIM_LEADING, &prefix);
  }
  for (WebOptionElement option = options.firstItem().to<WebOptionElement>();
       !option.isNull(); option = options.nextItem().to<WebOptionElement>()) {
    if (!StartsWith(option.value(), prefix, false) ||
        option.value() == prefix ||
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

// Trim the vectors before sending them to the browser process to ensure we
// don't send too much data through the IPC.
void TrimDataListsForIPC(std::vector<string16>* values,
                         std::vector<string16>* labels,
                         std::vector<string16>* icons,
                         std::vector<int>* unique_ids) {
  // Limit the size of the vectors.
  if (values->size() > kMaximumDataListSizeForAutofill) {
    values->resize(kMaximumDataListSizeForAutofill);
    labels->resize(kMaximumDataListSizeForAutofill);
    icons->resize(kMaximumDataListSizeForAutofill);
    unique_ids->resize(kMaximumDataListSizeForAutofill);
  }

  // Limit the size of the strings in the vectors
  for (size_t i = 0; i < values->size(); ++i) {
    if ((*values)[i].length() > kMaximumTextSizeForAutofill)
      (*values)[i].resize(kMaximumTextSizeForAutofill);

    if ((*labels)[i].length() > kMaximumTextSizeForAutofill)
      (*labels)[i].resize(kMaximumTextSizeForAutofill);

    if ((*icons)[i].length() > kMaximumTextSizeForAutofill)
      (*icons)[i].resize(kMaximumTextSizeForAutofill);
  }
}

gfx::RectF GetScaledBoundingBox(float scale, WebInputElement* element) {
  gfx::Rect bounding_box(element->boundsInViewportSpace());
  return gfx::RectF(bounding_box.x() * scale,
                    bounding_box.y() * scale,
                    bounding_box.width() * scale,
                    bounding_box.height() * scale);
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
      topmost_frame_(NULL),
      web_view_(render_view->GetWebView()),
      display_warning_if_disabled_(false),
      was_query_node_autofilled_(false),
      has_shown_autofill_popup_for_current_edit_(false),
      did_set_node_text_(false),
      autocheckout_click_in_progress_(false),
      is_whitelisted_for_autocheckout_(false),
      ignore_text_changes_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  render_view->GetWebView()->setAutofillClient(this);
}

AutofillAgent::~AutofillAgent() {}

bool AutofillAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AutofillAgent, message)
    IPC_MESSAGE_HANDLER(AutofillMsg_GetAllForms, OnGetAllForms)
    IPC_MESSAGE_HANDLER(AutofillMsg_SuggestionsReturned, OnSuggestionsReturned)
    IPC_MESSAGE_HANDLER(AutofillMsg_FormDataFilled, OnFormDataFilled)
    IPC_MESSAGE_HANDLER(AutofillMsg_FieldTypePredictionsAvailable,
                        OnFieldTypePredictionsAvailable)
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
    IPC_MESSAGE_HANDLER(AutofillMsg_AcceptDataListSuggestion,
                        OnAcceptDataListSuggestion)
    IPC_MESSAGE_HANDLER(AutofillMsg_AcceptPasswordAutofillSuggestion,
                        OnAcceptPasswordAutofillSuggestion)
    IPC_MESSAGE_HANDLER(AutofillMsg_RequestAutocompleteResult,
                        OnRequestAutocompleteResult)
    IPC_MESSAGE_HANDLER(AutofillMsg_FillFormsAndClick,
                        OnFillFormsAndClick)
    IPC_MESSAGE_HANDLER(AutofillMsg_WhitelistedForAutocheckout,
                        OnWhitelistedForAutocheckout)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AutofillAgent::DidFinishDocumentLoad(WebFrame* frame) {
  // Record timestamp on document load. This is used to record overhead of
  // Autofill feature.
  forms_seen_timestamp_ = base::TimeTicks::Now();

  // The document has now been fully loaded.  Scan for forms to be sent up to
  // the browser.
  std::vector<FormData> forms;
  bool has_more_forms = false;
  if (!frame->parent()) {
    topmost_frame_ = frame;
    form_elements_.clear();
    has_more_forms = form_cache_.ExtractFormsAndFormElements(
        *frame, kRequiredAutofillFields, &forms, &form_elements_);
    is_whitelisted_for_autocheckout_ = false;
  } else {
    form_cache_.ExtractForms(*frame, &forms);
  }

  // Always communicate to browser process for topmost frame.
  if (!forms.empty() || !frame->parent()) {
    Send(new AutofillHostMsg_FormsSeen(routing_id(), forms,
                                       forms_seen_timestamp_,
                                       has_more_forms));
  }
}

void AutofillAgent::DidStartProvisionalLoad(WebFrame* frame) {
  if (!frame->parent()) {
    topmost_frame_ = NULL;
    if (click_timer_.IsRunning()) {
      click_timer_.Stop();
      autocheckout_click_in_progress_ = true;
    }
  }
}

void AutofillAgent::DidFailProvisionalLoad(WebFrame* frame,
                                           const WebKit::WebURLError& error) {
  if (!frame->parent() && autocheckout_click_in_progress_) {
    autocheckout_click_in_progress_ = false;
    ClickFailed();
  }
}

void AutofillAgent::DidCommitProvisionalLoad(WebFrame* frame,
                                             bool is_new_navigation) {
  in_flight_request_form_.reset();
  if (!frame->parent())
    autocheckout_click_in_progress_ = false;
}

void AutofillAgent::FrameDetached(WebFrame* frame) {
  form_cache_.ResetFrame(*frame);
  if (!frame->parent()) {
    // |frame| is about to be destroyed so we need to clear |top_most_frame_|.
    topmost_frame_ = NULL;
    click_timer_.Stop();
  }
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
  HideHostAutofillUi();
}

void AutofillAgent::FocusedNodeChanged(const WebKit::WebNode& node) {
  if (!is_whitelisted_for_autocheckout_)
    return;

  if (node.isNull() || !node.isElementNode())
    return;

  WebKit::WebElement web_element = node.toConst<WebKit::WebElement>();

  if (!web_element.document().frame())
      return;

  const WebInputElement* element = toWebInputElement(&web_element);

  if (!element || !element->isEnabled() || element->isReadOnly() ||
      !element->isTextField() || element->isPasswordField())
    return;

  FormData form;
  FormFieldData field;
  // This must be called to short circuit this method if it fails.
  if (!FindFormAndFieldForInputElement(*element, &form, &field, REQUIRE_NONE))
    return;

  content::SSLStatus ssl_status = render_view()->GetSSLStatusOfFrame(
      element->document().frame());

  element_ = *element;

  Send(new AutofillHostMsg_MaybeShowAutocheckoutBubble(
      routing_id(),
      form.origin,
      ssl_status,
      GetScaledBoundingBox(web_view_->pageScaleFactor(), &element_)));
}

void AutofillAgent::DidChangeScrollOffset(WebKit::WebFrame*) {
  HideAutofillUi();
}

void AutofillAgent::didRequestAutocomplete(WebKit::WebFrame* frame,
                                           const WebFormElement& form) {
  FormData form_data;
  if (!in_flight_request_form_.isNull() ||
      !WebFormElementToFormData(form,
                                WebFormControlElement(),
                                REQUIRE_AUTOCOMPLETE,
                                EXTRACT_OPTIONS,
                                &form_data,
                                NULL)) {
    WebFormElement(form).finishRequestAutocomplete(
        WebFormElement::AutocompleteResultErrorDisabled);
    return;
  }

  // Cancel any pending Autofill requests and hide any currently showing popups.
  ++autofill_query_id_;
  HideAutofillUi();

  in_flight_request_form_ = form;
  // Add SSL Status in the formdata to let browser process alert user
  // appropriately using browser UI.
  form_data.ssl_status = render_view()->GetSSLStatusOfFrame(frame);
  Send(new AutofillHostMsg_RequestAutocomplete(
      routing_id(),
      form_data,
      frame->document().url()));
}

void AutofillAgent::setIgnoreTextChanges(bool ignore) {
  ignore_text_changes_ = ignore;
}

bool AutofillAgent::InputElementClicked(const WebInputElement& element,
                                        bool was_focused,
                                        bool is_focused) {
  if (was_focused)
    ShowSuggestions(element, true, false, true);

  return false;
}

bool AutofillAgent::InputElementLostFocus() {
  HideHostAutofillUi();

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
      // User selected an Autocomplete or password entry, so we fill directly.
      SetNodeText(value, &element_);
      break;
    case WebAutofillClient::MenuItemIDDataListEntry:
      AcceptDataListSuggestion(value);
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
  if (ignore_text_changes_)
    return;

  if (did_set_node_text_) {
    did_set_node_text_ = false;
    return;
  }

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
  // If the element isn't focused then the changes don't matter. This check is
  // required to properly handle IME interactions.
  if (!element.focused())
    return;

  if (password_autofill_manager_->TextDidChangeInTextField(element)) {
    element_ = element;
    return;
  }

  ShowSuggestions(element, false, true, false);

  FormData form;
  FormFieldData field;
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

  if (v.empty()) {
    // No suggestions, any popup currently showing is obsolete.
    HideAutofillUi();
    return;
  }

  WebKit::WebView* web_view = render_view()->GetWebView();
  if (!web_view)
    return;

  // Send to WebKit for display.
  web_view->applyAutofillSuggestions(element, v, l, i, ids);

  Send(new AutofillHostMsg_DidShowAutofillSuggestions(
      routing_id(),
      has_autofill_item && !has_shown_autofill_popup_for_current_edit_));
  has_shown_autofill_popup_for_current_edit_ |= has_autofill_item;
}

void AutofillAgent::AcceptDataListSuggestion(const string16& suggested_value) {
  string16 new_value = suggested_value;
  // If this element takes multiple values then replace the last part with
  // the suggestion.
  if (element_.isMultiple() &&
      element_.formControlType() == WebString::fromUTF8("email")) {
    std::vector<string16> parts;

    base::SplitStringDontTrim(element_.editingValue(), ',', &parts);
    if (parts.size() == 0)
      parts.push_back(string16());

    string16 last_part = parts.back();
    // We want to keep just the leading whitespace.
    for (size_t i = 0; i < last_part.size(); ++i) {
      if (!IsWhitespace(last_part[i])) {
        last_part = last_part.substr(0, i);
        break;
      }
    }
    last_part.append(suggested_value);
    parts[parts.size() - 1] = last_part;

    new_value = JoinString(parts, ',');
  }
  SetNodeText(new_value, &element_);
}

void AutofillAgent::OnFormDataFilled(int query_id,
                                     const FormData& form) {
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

void AutofillAgent::OnAcceptDataListSuggestion(const string16& value) {
  AcceptDataListSuggestion(value);
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

void AutofillAgent::OnGetAllForms() {
  form_elements_.clear();

  // Force fetch all non empty forms.
  std::vector<FormData> forms;
  form_cache_.ExtractFormsAndFormElements(
      *topmost_frame_, 0, &forms, &form_elements_);

  // OnGetAllForms should only be called if AutofillAgent reported to
  // AutofillManager that there are more forms
  DCHECK(!forms.empty());

  // Report to AutofillManager that all forms are being sent.
  Send(new AutofillHostMsg_FormsSeen(routing_id(), forms,
                                     forms_seen_timestamp_, false));
}

void AutofillAgent::OnRequestAutocompleteResult(
    WebFormElement::AutocompleteResult result, const FormData& form_data) {
  if (in_flight_request_form_.isNull())
    return;

  if (result == WebFormElement::AutocompleteResultSuccess)
    FillFormIncludingNonFocusableElements(form_data, in_flight_request_form_);

  in_flight_request_form_.finishRequestAutocomplete(result);
  in_flight_request_form_.reset();
}

void AutofillAgent::OnFillFormsAndClick(
    const std::vector<FormData>& forms,
    const WebElementDescriptor& click_element_descriptor) {
  DCHECK_EQ(forms.size(), form_elements_.size());

  // Fill the form.
  for (size_t i = 0; i < forms.size(); ++i)
    FillFormIncludingNonFocusableElements(forms[i], form_elements_[i]);

  // Exit early if there is nothing to click.
  if (click_element_descriptor.retrieval_method == WebElementDescriptor::NONE)
    return;

  // It's possible that clicking the element to proceed in an Autocheckout
  // flow will not actually proceed to the next step in the flow, e.g. there
  // is a new required field that Autocheckout does not know how to fill.  In
  // order to capture this case and present the user with an error a timer is
  // set that informs the browser of the error. |click_timer_| has to be started
  // before clicking so it can start before DidStartProvisionalLoad started.
  click_timer_.Start(FROM_HERE,
                     base::TimeDelta::FromSeconds(kAutocheckoutClickTimeout),
                     this,
                     &AutofillAgent::ClickFailed);
  if (!ClickElement(topmost_frame_->document(),
                    click_element_descriptor)) {
    click_timer_.Stop();
    Send(new AutofillHostMsg_ClickFailed(routing_id(),
                                         MISSING_ADVANCE));
  }
}

void AutofillAgent::OnWhitelistedForAutocheckout() {
  is_whitelisted_for_autocheckout_ = true;
}

void AutofillAgent::ClickFailed() {
  Send(new AutofillHostMsg_ClickFailed(routing_id(),
                                       CANNOT_PROCEED));
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
  WebString value = element.editingValue();
  if (value.length() > kMaximumTextSizeForAutofill ||
      (!autofill_on_empty_values && value.isEmpty()) ||
      (requires_caret_at_end &&
       (element.selectionStart() != element.selectionEnd() ||
        element.selectionEnd() != static_cast<int>(value.length())))) {
    // Any popup currently showing is obsolete.
    HideAutofillUi();
    return;
  }

  element_ = element;

  // If autocomplete is disabled at the form level, then we might want to show a
  // warning in place of suggestions.  However, if autocomplete is disabled
  // specifically for this field, we never want to show a warning.  Otherwise,
  // we might interfere with custom popups (e.g. search suggestions) used by the
  // website.  Note that we cannot use the WebKit method element.autoComplete()
  // as it does not allow us to distinguish the case where autocomplete is
  // disabled for *both* the element and for the form.
  // Also, if the field has no name, then we won't have values.
  const string16 autocomplete_attribute = element.getAttribute("autocomplete");
  if (LowerCaseEqualsASCII(autocomplete_attribute, "off") ||
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
  if (!element.document().frame())
    return;

  static int query_counter = 0;
  autofill_query_id_ = query_counter++;
  display_warning_if_disabled_ = display_warning_if_disabled;

  // If autocomplete is disabled at the form level, we want to see if there
  // would have been any suggestions were it enabled, so that we can show a
  // warning.  Otherwise, we want to ignore fields that disable autocomplete, so
  // that the suggestions list does not include suggestions for these form
  // fields -- see comment 1 on http://crbug.com/69914
  // Rather than testing the form's autocomplete enabled state, we test the
  // element's state.  The DCHECK below ensures that this is equivalent.
  DCHECK(element.autoComplete() || !element.form().autoComplete());
  const RequirementsMask requirements =
      element.autoComplete() ? REQUIRE_AUTOCOMPLETE : REQUIRE_NONE;

  FormData form;
  FormFieldData field;
  if (!FindFormAndFieldForInputElement(element, &form, &field, requirements)) {
    // If we didn't find the cached form, at least let autocomplete have a shot
    // at providing suggestions.
    WebFormControlElementToFormField(element, EXTRACT_VALUE, &field);
  }

  gfx::RectF bounding_box_scaled =
      GetScaledBoundingBox(web_view_->pageScaleFactor(), &element_);

  // Find the datalist values and send them to the browser process.
  std::vector<string16> data_list_values;
  std::vector<string16> data_list_labels;
  std::vector<string16> data_list_icons;
  std::vector<int> data_list_unique_ids;
  AppendDataListSuggestions(element_,
                            &data_list_values,
                            &data_list_labels,
                            &data_list_icons,
                            &data_list_unique_ids);

  TrimDataListsForIPC(&data_list_values,
                      &data_list_labels,
                      &data_list_icons,
                      &data_list_unique_ids);

  Send(new AutofillHostMsg_SetDataList(routing_id(),
                                       data_list_values,
                                       data_list_labels,
                                       data_list_icons,
                                       data_list_unique_ids));

  // Add SSL Status in the formdata to let browser process alert user
  // appropriately using browser UI.
  form.ssl_status = render_view()->GetSSLStatusOfFrame(
      element.document().frame());
  Send(new AutofillHostMsg_QueryFormFieldAutofill(routing_id(),
                                                  autofill_query_id_,
                                                  form,
                                                  field,
                                                  bounding_box_scaled,
                                                  display_warning_if_disabled));
}

void AutofillAgent::FillAutofillFormData(const WebNode& node,
                                         int unique_id,
                                         AutofillAction action) {
  DCHECK_GT(unique_id, 0);

  static int query_counter = 0;
  autofill_query_id_ = query_counter++;

  FormData form;
  FormFieldData field;
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
  did_set_node_text_ = true;
  string16 substring = value;
  substring = substring.substr(0, node->maxLength());

  node->setEditingValue(substring);
}

void AutofillAgent::HideAutofillUi() {
  WebKit::WebView* web_view = render_view()->GetWebView();
  if (web_view)
    web_view->hidePopups();

  HideHostAutofillUi();
}

void AutofillAgent::HideHostAutofillUi() {
  Send(new AutofillHostMsg_HideAutofillUi(routing_id()));
}

}  // namespace autofill
