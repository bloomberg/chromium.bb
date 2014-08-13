// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/autofill_agent.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/page_click_tracker.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/content/renderer/password_generation_agent.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_data_validation.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/web_element_descriptor.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_view.h"
#include "grit/components_strings.h"
#include "net/cert/cert_status_flags.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElementCollection.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebOptionElement.h"
#include "third_party/WebKit/public/web/WebTextAreaElement.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes.h"

using blink::WebAutofillClient;
using blink::WebConsoleMessage;
using blink::WebElement;
using blink::WebElementCollection;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebFrame;
using blink::WebInputElement;
using blink::WebKeyboardEvent;
using blink::WebLocalFrame;
using blink::WebNode;
using blink::WebOptionElement;
using blink::WebString;
using blink::WebTextAreaElement;
using blink::WebVector;

namespace autofill {

namespace {

// Gets all the data list values (with corresponding label) for the given
// element.
void GetDataListSuggestions(const WebInputElement& element,
                            bool ignore_current_value,
                            std::vector<base::string16>* values,
                            std::vector<base::string16>* labels) {
  WebElementCollection options = element.dataListOptions();
  if (options.isNull())
    return;

  base::string16 prefix;
  if (!ignore_current_value) {
    prefix = element.editingValue();
    if (element.isMultiple() && element.isEmailField()) {
      std::vector<base::string16> parts;
      base::SplitStringDontTrim(prefix, ',', &parts);
      if (parts.size() > 0) {
        base::TrimWhitespace(parts[parts.size() - 1], base::TRIM_LEADING,
                             &prefix);
      }
    }
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
      labels->push_back(base::string16());
  }
}

// Trim the vector before sending it to the browser process to ensure we
// don't send too much data through the IPC.
void TrimStringVectorForIPC(std::vector<base::string16>* strings) {
  // Limit the size of the vector.
  if (strings->size() > kMaxListSize)
    strings->resize(kMaxListSize);

  // Limit the size of the strings in the vector.
  for (size_t i = 0; i < strings->size(); ++i) {
    if ((*strings)[i].length() > kMaxDataLength)
      (*strings)[i].resize(kMaxDataLength);
  }
}

}  // namespace

AutofillAgent::AutofillAgent(content::RenderView* render_view,
                             PasswordAutofillAgent* password_autofill_agent,
                             PasswordGenerationAgent* password_generation_agent)
    : content::RenderViewObserver(render_view),
      password_autofill_agent_(password_autofill_agent),
      password_generation_agent_(password_generation_agent),
      autofill_query_id_(0),
      web_view_(render_view->GetWebView()),
      display_warning_if_disabled_(false),
      was_query_node_autofilled_(false),
      has_shown_autofill_popup_for_current_edit_(false),
      did_set_node_text_(false),
      ignore_text_changes_(false),
      is_popup_possibly_visible_(false),
      main_frame_processed_(false),
      weak_ptr_factory_(this) {
  render_view->GetWebView()->setAutofillClient(this);

  // The PageClickTracker is a RenderViewObserver, and hence will be freed when
  // the RenderView is destroyed.
  new PageClickTracker(render_view, this);
}

AutofillAgent::~AutofillAgent() {}

bool AutofillAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(AutofillAgent, message)
    IPC_MESSAGE_HANDLER(AutofillMsg_Ping, OnPing)
    IPC_MESSAGE_HANDLER(AutofillMsg_FillForm, OnFillForm)
    IPC_MESSAGE_HANDLER(AutofillMsg_PreviewForm, OnPreviewForm)
    IPC_MESSAGE_HANDLER(AutofillMsg_FieldTypePredictionsAvailable,
                        OnFieldTypePredictionsAvailable)
    IPC_MESSAGE_HANDLER(AutofillMsg_ClearForm, OnClearForm)
    IPC_MESSAGE_HANDLER(AutofillMsg_ClearPreviewedForm, OnClearPreviewedForm)
    IPC_MESSAGE_HANDLER(AutofillMsg_FillFieldWithValue, OnFillFieldWithValue)
    IPC_MESSAGE_HANDLER(AutofillMsg_PreviewFieldWithValue,
                        OnPreviewFieldWithValue)
    IPC_MESSAGE_HANDLER(AutofillMsg_AcceptDataListSuggestion,
                        OnAcceptDataListSuggestion)
    IPC_MESSAGE_HANDLER(AutofillMsg_FillPasswordSuggestion,
                        OnFillPasswordSuggestion)
    IPC_MESSAGE_HANDLER(AutofillMsg_PreviewPasswordSuggestion,
                        OnPreviewPasswordSuggestion)
    IPC_MESSAGE_HANDLER(AutofillMsg_RequestAutocompleteResult,
                        OnRequestAutocompleteResult)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void AutofillAgent::DidFinishDocumentLoad(WebLocalFrame* frame) {
  // If the main frame just finished loading, we should process it.
  if (!frame->parent())
    main_frame_processed_ = false;

  ProcessForms(*frame);
}

void AutofillAgent::DidCommitProvisionalLoad(WebLocalFrame* frame,
                                             bool is_new_navigation) {
  form_cache_.ResetFrame(*frame);
}

void AutofillAgent::FrameDetached(WebFrame* frame) {
  form_cache_.ResetFrame(*frame);
}

void AutofillAgent::FrameWillClose(WebFrame* frame) {
  if (in_flight_request_form_.isNull())
    return;

  for (WebFrame* temp = in_flight_request_form_.document().frame();
       temp; temp = temp->parent()) {
    if (temp == frame) {
      Send(new AutofillHostMsg_CancelRequestAutocomplete(routing_id()));
      break;
    }
  }
}

void AutofillAgent::WillSubmitForm(WebLocalFrame* frame,
                                   const WebFormElement& form) {
  FormData form_data;
  if (WebFormElementToFormData(form,
                               WebFormControlElement(),
                               REQUIRE_NONE,
                               static_cast<ExtractMask>(
                                   EXTRACT_VALUE | EXTRACT_OPTION_TEXT),
                               &form_data,
                               NULL)) {
    Send(new AutofillHostMsg_FormSubmitted(routing_id(), form_data,
                                           base::TimeTicks::Now()));
  }
}

void AutofillAgent::FocusedNodeChanged(const WebNode& node) {
  HidePopup();

  if (password_generation_agent_ &&
      password_generation_agent_->FocusedNodeHasChanged(node)) {
    is_popup_possibly_visible_ = true;
    return;
  }

  if (node.isNull() || !node.isElementNode())
    return;

  WebElement web_element = node.toConst<WebElement>();

  if (!web_element.document().frame())
      return;

  const WebInputElement* element = toWebInputElement(&web_element);

  if (!element || !element->isEnabled() || element->isReadOnly() ||
      !element->isTextField() || element->isPasswordField())
    return;

  element_ = *element;
}

void AutofillAgent::OrientationChangeEvent() {
  HidePopup();
}

void AutofillAgent::DidChangeScrollOffset(WebLocalFrame*) {
  HidePopup();
}

void AutofillAgent::didRequestAutocomplete(
    const WebFormElement& form) {
  // Disallow the dialog over non-https or broken https, except when the
  // ignore SSL flag is passed. See http://crbug.com/272512.
  // TODO(palmer): this should be moved to the browser process after frames
  // get their own processes.
  GURL url(form.document().url());
  content::SSLStatus ssl_status =
      render_view()->GetSSLStatusOfFrame(form.document().frame());
  bool is_safe = url.SchemeIs(url::kHttpsScheme) &&
      !net::IsCertStatusError(ssl_status.cert_status);
  bool allow_unsafe = CommandLine::ForCurrentProcess()->HasSwitch(
      ::switches::kReduceSecurityForTesting);

  FormData form_data;
  std::string error_message;
  if (!in_flight_request_form_.isNull()) {
    error_message = "already active.";
  } else if (!is_safe && !allow_unsafe) {
    error_message =
        "must use a secure connection or --reduce-security-for-testing.";
  } else if (!WebFormElementToFormData(form,
                                       WebFormControlElement(),
                                       REQUIRE_AUTOCOMPLETE,
                                       static_cast<ExtractMask>(
                                           EXTRACT_VALUE |
                                           EXTRACT_OPTION_TEXT |
                                           EXTRACT_OPTIONS),
                                       &form_data,
                                       NULL)) {
    error_message = "failed to parse form.";
  }

  if (!error_message.empty()) {
    WebConsoleMessage console_message = WebConsoleMessage(
        WebConsoleMessage::LevelLog,
        WebString(base::ASCIIToUTF16("requestAutocomplete: ") +
                      base::ASCIIToUTF16(error_message)));
    form.document().frame()->addMessageToConsole(console_message);
    WebFormElement(form).finishRequestAutocomplete(
        WebFormElement::AutocompleteResultErrorDisabled);
    return;
  }

  // Cancel any pending Autofill requests and hide any currently showing popups.
  ++autofill_query_id_;
  HidePopup();

  in_flight_request_form_ = form;
  Send(new AutofillHostMsg_RequestAutocomplete(routing_id(), form_data, url));
}

void AutofillAgent::setIgnoreTextChanges(bool ignore) {
  ignore_text_changes_ = ignore;
}

void AutofillAgent::FormControlElementClicked(
    const WebFormControlElement& element,
    bool was_focused) {
  const WebInputElement* input_element = toWebInputElement(&element);
  if (!input_element && !IsTextAreaElement(element))
    return;

  bool show_full_suggestion_list = element.isAutofilled() || was_focused;
  bool show_password_suggestions_only = !was_focused;
  ShowSuggestions(element,
                  true,
                  false,
                  true,
                  false,
                  show_full_suggestion_list,
                  show_password_suggestions_only);
}

void AutofillAgent::textFieldDidEndEditing(const WebInputElement& element) {
  password_autofill_agent_->TextFieldDidEndEditing(element);
  has_shown_autofill_popup_for_current_edit_ = false;
  Send(new AutofillHostMsg_DidEndTextFieldEditing(routing_id()));
}

void AutofillAgent::textFieldDidChange(const WebFormControlElement& element) {
  if (ignore_text_changes_)
    return;

  DCHECK(toWebInputElement(&element) || IsTextAreaElement(element));

  if (did_set_node_text_) {
    did_set_node_text_ = false;
    return;
  }

  // We post a task for doing the Autofill as the caret position is not set
  // properly at this point (http://bugs.webkit.org/show_bug.cgi?id=16976) and
  // it is needed to trigger autofill.
  weak_ptr_factory_.InvalidateWeakPtrs();
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&AutofillAgent::TextFieldDidChangeImpl,
                 weak_ptr_factory_.GetWeakPtr(),
                 element));
}

void AutofillAgent::TextFieldDidChangeImpl(
    const WebFormControlElement& element) {
  // If the element isn't focused then the changes don't matter. This check is
  // required to properly handle IME interactions.
  if (!element.focused())
    return;

  const WebInputElement* input_element = toWebInputElement(&element);
  if (input_element) {
    if (password_generation_agent_ &&
        password_generation_agent_->TextDidChangeInTextField(*input_element)) {
      is_popup_possibly_visible_ = true;
      return;
    }

    if (password_autofill_agent_->TextDidChangeInTextField(*input_element)) {
      element_ = element;
      return;
    }
  }

  ShowSuggestions(element, false, true, false, false, false, false);

  FormData form;
  FormFieldData field;
  if (FindFormAndFieldForFormControlElement(element,
                                            &form,
                                            &field,
                                            REQUIRE_NONE)) {
    Send(new AutofillHostMsg_TextFieldDidChange(routing_id(), form, field,
                                                base::TimeTicks::Now()));
  }
}

void AutofillAgent::textFieldDidReceiveKeyDown(const WebInputElement& element,
                                               const WebKeyboardEvent& event) {
  if (password_autofill_agent_->TextFieldHandlingKeyDown(element, event)) {
    element_ = element;
    return;
  }

  if (event.windowsKeyCode == ui::VKEY_DOWN ||
      event.windowsKeyCode == ui::VKEY_UP)
    ShowSuggestions(element, true, true, true, false, false, false);
}

void AutofillAgent::openTextDataListChooser(const WebInputElement& element) {
  ShowSuggestions(element, true, false, false, true, false, false);
}

void AutofillAgent::firstUserGestureObserved() {
  password_autofill_agent_->FirstUserGestureObserved();
}

void AutofillAgent::AcceptDataListSuggestion(
    const base::string16& suggested_value) {
  WebInputElement* input_element = toWebInputElement(&element_);
  DCHECK(input_element);
  base::string16 new_value = suggested_value;
  // If this element takes multiple values then replace the last part with
  // the suggestion.
  if (input_element->isMultiple() && input_element->isEmailField()) {
    std::vector<base::string16> parts;

    base::SplitStringDontTrim(input_element->editingValue(), ',', &parts);
    if (parts.size() == 0)
      parts.push_back(base::string16());

    base::string16 last_part = parts.back();
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
  FillFieldWithValue(new_value, input_element);
}

void AutofillAgent::OnFieldTypePredictionsAvailable(
    const std::vector<FormDataPredictions>& forms) {
  for (size_t i = 0; i < forms.size(); ++i) {
    form_cache_.ShowPredictions(forms[i]);
  }
}

void AutofillAgent::OnFillForm(int query_id, const FormData& form) {
  if (!render_view()->GetWebView() || query_id != autofill_query_id_)
    return;

  was_query_node_autofilled_ = element_.isAutofilled();
  FillForm(form, element_);
  Send(new AutofillHostMsg_DidFillAutofillFormData(routing_id(),
                                                   base::TimeTicks::Now()));
}

void AutofillAgent::OnPing() {
  Send(new AutofillHostMsg_PingAck(routing_id()));
}

void AutofillAgent::OnPreviewForm(int query_id, const FormData& form) {
  if (!render_view()->GetWebView() || query_id != autofill_query_id_)
    return;

  was_query_node_autofilled_ = element_.isAutofilled();
  PreviewForm(form, element_);
  Send(new AutofillHostMsg_DidPreviewAutofillFormData(routing_id()));
}

void AutofillAgent::OnClearForm() {
  form_cache_.ClearFormWithElement(element_);
}

void AutofillAgent::OnClearPreviewedForm() {
  if (!element_.isNull()) {
    if (password_autofill_agent_->DidClearAutofillSelection(element_))
      return;

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

void AutofillAgent::OnFillFieldWithValue(const base::string16& value) {
  WebInputElement* input_element = toWebInputElement(&element_);
  if (input_element)
    FillFieldWithValue(value, input_element);
}

void AutofillAgent::OnPreviewFieldWithValue(const base::string16& value) {
  WebInputElement* input_element = toWebInputElement(&element_);
  if (input_element)
    PreviewFieldWithValue(value, input_element);
}

void AutofillAgent::OnAcceptDataListSuggestion(const base::string16& value) {
  AcceptDataListSuggestion(value);
}

void AutofillAgent::OnFillPasswordSuggestion(const base::string16& username,
                                             const base::string16& password) {
  bool handled = password_autofill_agent_->FillSuggestion(
      element_,
      username,
      password);
  DCHECK(handled);
}

void AutofillAgent::OnPreviewPasswordSuggestion(
    const base::string16& username,
    const base::string16& password) {
  bool handled = password_autofill_agent_->PreviewSuggestion(
      element_,
      username,
      password);
  DCHECK(handled);
}

void AutofillAgent::OnRequestAutocompleteResult(
    WebFormElement::AutocompleteResult result,
    const base::string16& message,
    const FormData& form_data) {
  if (in_flight_request_form_.isNull())
    return;

  if (result == WebFormElement::AutocompleteResultSuccess) {
    FillFormIncludingNonFocusableElements(form_data, in_flight_request_form_);
    if (!in_flight_request_form_.checkValidity())
      result = WebFormElement::AutocompleteResultErrorInvalid;
  }

  in_flight_request_form_.finishRequestAutocomplete(result);

  if (!message.empty()) {
    const base::string16 prefix(base::ASCIIToUTF16("requestAutocomplete: "));
    WebConsoleMessage console_message = WebConsoleMessage(
        WebConsoleMessage::LevelLog, WebString(prefix + message));
    in_flight_request_form_.document().frame()->addMessageToConsole(
        console_message);
  }

  in_flight_request_form_.reset();
}

void AutofillAgent::ShowSuggestions(const WebFormControlElement& element,
                                    bool autofill_on_empty_values,
                                    bool requires_caret_at_end,
                                    bool display_warning_if_disabled,
                                    bool datalist_only,
                                    bool show_full_suggestion_list,
                                    bool show_password_suggestions_only) {
  if (!element.isEnabled() || element.isReadOnly())
    return;
  if (!datalist_only && !element.suggestedValue().isEmpty())
    return;

  const WebInputElement* input_element = toWebInputElement(&element);
  if (input_element) {
    if (!input_element->isTextField() || input_element->isPasswordField())
      return;
    if (!datalist_only && !input_element->suggestedValue().isEmpty())
      return;
  } else {
    DCHECK(IsTextAreaElement(element));
    if (!element.toConst<WebTextAreaElement>().suggestedValue().isEmpty())
      return;
  }

  // Don't attempt to autofill with values that are too large or if filling
  // criteria are not met.
  WebString value = element.editingValue();
  if (!datalist_only &&
      (value.length() > kMaxDataLength ||
       (!autofill_on_empty_values && value.isEmpty()) ||
       (requires_caret_at_end &&
        (element.selectionStart() != element.selectionEnd() ||
         element.selectionEnd() != static_cast<int>(value.length()))))) {
    // Any popup currently showing is obsolete.
    HidePopup();
    return;
  }

  element_ = element;
  if (IsAutofillableInputElement(input_element) &&
      (password_autofill_agent_->ShowSuggestions(*input_element,
                                                 show_full_suggestion_list) ||
       show_password_suggestions_only)) {
    is_popup_possibly_visible_ = true;
    return;
  }

  // If autocomplete is disabled at the field level, ensure that the native
  // UI won't try to show a warning, since that may conflict with a custom
  // popup. Note that we cannot use the WebKit method element.autoComplete()
  // as it does not allow us to distinguish the case where autocomplete is
  // disabled for *both* the element and for the form.
  const base::string16 autocomplete_attribute =
      element.getAttribute("autocomplete");
  if (base::LowerCaseEqualsASCII(autocomplete_attribute, "off"))
    display_warning_if_disabled = false;

  QueryAutofillSuggestions(element,
                           display_warning_if_disabled,
                           datalist_only);
}

void AutofillAgent::QueryAutofillSuggestions(
    const WebFormControlElement& element,
    bool display_warning_if_disabled,
    bool datalist_only) {
  if (!element.document().frame())
    return;

  DCHECK(toWebInputElement(&element) || IsTextAreaElement(element));

  static int query_counter = 0;
  autofill_query_id_ = query_counter++;
  display_warning_if_disabled_ = display_warning_if_disabled;

  // If autocomplete is disabled at the form level, we want to see if there
  // would have been any suggestions were it enabled, so that we can show a
  // warning.  Otherwise, we want to ignore fields that disable autocomplete, so
  // that the suggestions list does not include suggestions for these form
  // fields -- see comment 1 on http://crbug.com/69914
  const RequirementsMask requirements =
      element.autoComplete() ? REQUIRE_AUTOCOMPLETE : REQUIRE_NONE;

  FormData form;
  FormFieldData field;
  if (!FindFormAndFieldForFormControlElement(element, &form, &field,
                                             requirements)) {
    // If we didn't find the cached form, at least let autocomplete have a shot
    // at providing suggestions.
    WebFormControlElementToFormField(element, EXTRACT_VALUE, &field);
  }
  if (datalist_only)
    field.should_autocomplete = false;

  gfx::RectF bounding_box_scaled =
      GetScaledBoundingBox(web_view_->pageScaleFactor(), &element_);

  std::vector<base::string16> data_list_values;
  std::vector<base::string16> data_list_labels;
  const WebInputElement* input_element = toWebInputElement(&element);
  if (input_element) {
    // Find the datalist values and send them to the browser process.
    GetDataListSuggestions(*input_element,
                           datalist_only,
                           &data_list_values,
                           &data_list_labels);
    TrimStringVectorForIPC(&data_list_values);
    TrimStringVectorForIPC(&data_list_labels);
  }

  is_popup_possibly_visible_ = true;
  Send(new AutofillHostMsg_SetDataList(routing_id(),
                                       data_list_values,
                                       data_list_labels));

  Send(new AutofillHostMsg_QueryFormFieldAutofill(routing_id(),
                                                  autofill_query_id_,
                                                  form,
                                                  field,
                                                  bounding_box_scaled,
                                                  display_warning_if_disabled));
}

void AutofillAgent::FillFieldWithValue(const base::string16& value,
                                       WebInputElement* node) {
  did_set_node_text_ = true;
  node->setEditingValue(value.substr(0, node->maxLength()));
  node->setAutofilled(true);
}

void AutofillAgent::PreviewFieldWithValue(const base::string16& value,
                                          WebInputElement* node) {
  was_query_node_autofilled_ = element_.isAutofilled();
  node->setSuggestedValue(value.substr(0, node->maxLength()));
  node->setAutofilled(true);
  node->setSelectionRange(node->value().length(),
                          node->suggestedValue().length());
}

void AutofillAgent::ProcessForms(const WebLocalFrame& frame) {
  // Record timestamp of when the forms are first seen. This is used to
  // measure the overhead of the Autofill feature.
  base::TimeTicks forms_seen_timestamp = base::TimeTicks::Now();

  std::vector<FormData> forms;
  form_cache_.ExtractNewForms(frame, &forms);

  // Always communicate to browser process for topmost frame.
  if (!forms.empty() ||
      (!frame.parent() && !main_frame_processed_)) {
    Send(new AutofillHostMsg_FormsSeen(routing_id(), forms,
                                       forms_seen_timestamp));
  }

  if (!frame.parent())
    main_frame_processed_ = true;
}

void AutofillAgent::HidePopup() {
  if (!is_popup_possibly_visible_)
    return;

  if (!element_.isNull())
    OnClearPreviewedForm();

  is_popup_possibly_visible_ = false;
  Send(new AutofillHostMsg_HidePopup(routing_id()));
}

void AutofillAgent::didAssociateFormControls(const WebVector<WebNode>& nodes) {
  for (size_t i = 0; i < nodes.size(); ++i) {
    WebLocalFrame* frame = nodes[i].document().frame();
    // Only monitors dynamic forms created in the top frame. Dynamic forms
    // inserted in iframes are not captured yet. Frame is only processed
    // if it has finished loading, otherwise you can end up with a partially
    // parsed form.
    if (frame && !frame->parent() && !frame->isLoading()) {
      ProcessForms(*frame);
      password_autofill_agent_->OnDynamicFormsSeen(frame);
      return;
    }
  }
}

}  // namespace autofill
