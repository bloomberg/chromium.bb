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
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/page_click_tracker.h"
#include "components/autofill/content/renderer/password_autofill_agent.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_messages.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_predictions.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/web_element_descriptor.h"
#include "content/public/common/password_form.h"
#include "content/public/common/ssl_status.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_view.h"
#include "grit/component_strings.h"
#include "net/cert/cert_status_flags.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebDataSource.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebNodeCollection.h"
#include "third_party/WebKit/public/web/WebOptionElement.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"

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


// Gets all the data list values (with corresponding label) for the given
// element.
void GetDataListSuggestions(const WebKit::WebInputElement& element,
                            std::vector<base::string16>* values,
                            std::vector<base::string16>* labels) {
  WebNodeCollection options = element.dataListOptions();
  if (options.isNull())
    return;

  base::string16 prefix = element.editingValue();
  if (element.isMultiple() &&
      element.formControlType() == WebString::fromUTF8("email")) {
    std::vector<base::string16> parts;
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
      labels->push_back(base::string16());
  }
}

// Trim the vector before sending it to the browser process to ensure we
// don't send too much data through the IPC.
void TrimStringVectorForIPC(std::vector<base::string16>* strings) {
  // Limit the size of the vector.
  if (strings->size() > kMaximumDataListSizeForAutofill)
    strings->resize(kMaximumDataListSizeForAutofill);

  // Limit the size of the strings in the vector.
  for (size_t i = 0; i < strings->size(); ++i) {
    if ((*strings)[i].length() > kMaximumTextSizeForAutofill)
      (*strings)[i].resize(kMaximumTextSizeForAutofill);
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

AutofillAgent::AutofillAgent(content::RenderView* render_view,
                             PasswordAutofillAgent* password_autofill_agent)
    : content::RenderViewObserver(render_view),
      password_autofill_agent_(password_autofill_agent),
      autofill_query_id_(0),
      autofill_action_(AUTOFILL_NONE),
      topmost_frame_(NULL),
      web_view_(render_view->GetWebView()),
      display_warning_if_disabled_(false),
      was_query_node_autofilled_(false),
      has_shown_autofill_popup_for_current_edit_(false),
      did_set_node_text_(false),
      autocheckout_click_in_progress_(false),
      is_autocheckout_supported_(false),
      has_new_forms_for_browser_(false),
      ignore_text_changes_(false),
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
    IPC_MESSAGE_HANDLER(AutofillMsg_GetAllForms, OnGetAllForms)
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
    IPC_MESSAGE_HANDLER(AutofillMsg_AutocheckoutSupported,
                        OnAutocheckoutSupported)
    IPC_MESSAGE_HANDLER(AutofillMsg_PageShown,
                        OnPageShown)
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
  } else {
    form_cache_.ExtractForms(*frame, &forms);
  }

  autofill::FormsSeenState state = has_more_forms ?
      autofill::PARTIAL_FORMS_SEEN : autofill::NO_SPECIAL_FORMS_SEEN;

  // Always communicate to browser process for topmost frame.
  if (!forms.empty() || !frame->parent()) {
    Send(new AutofillHostMsg_FormsSeen(routing_id(), forms,
                                       forms_seen_timestamp_,
                                       state));
  }
}

void AutofillAgent::DidStartProvisionalLoad(WebFrame* frame) {
  if (!frame->parent()) {
    is_autocheckout_supported_ = false;
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
  if (!frame->parent() && autocheckout_click_in_progress_) {
    autocheckout_click_in_progress_ = false;
    CompleteAutocheckoutPage(SUCCESS);
  }
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
  HideAutofillUI();
}

void AutofillAgent::FocusedNodeChanged(const WebKit::WebNode& node) {
  if (node.isNull() || !node.isElementNode())
    return;

  WebKit::WebElement web_element = node.toConst<WebKit::WebElement>();

  if (!web_element.document().frame())
      return;

  const WebInputElement* element = toWebInputElement(&web_element);

  if (!element || !element->isEnabled() || element->isReadOnly() ||
      !element->isTextField() || element->isPasswordField())
    return;

  element_ = *element;

  MaybeShowAutocheckoutBubble();
}

void AutofillAgent::MaybeShowAutocheckoutBubble() {
  if (element_.isNull() || !element_.focused())
    return;

  FormData form;
  FormFieldData field;
  // This must be called to short circuit this method if it fails.
  if (!FindFormAndFieldForInputElement(element_, &form, &field, REQUIRE_NONE))
    return;

  Send(new AutofillHostMsg_MaybeShowAutocheckoutBubble(
      routing_id(),
      form,
      GetScaledBoundingBox(web_view_->pageScaleFactor(), &element_)));
}

void AutofillAgent::DidChangeScrollOffset(WebKit::WebFrame*) {
  HideAutofillUI();
}

void AutofillAgent::didRequestAutocomplete(WebKit::WebFrame* frame,
                                           const WebFormElement& form) {
  GURL url(frame->document().url());
  content::SSLStatus ssl_status = render_view()->GetSSLStatusOfFrame(frame);
  FormData form_data;
  if (!in_flight_request_form_.isNull() ||
      (url.SchemeIs(chrome::kHttpsScheme) &&
       (net::IsCertStatusError(ssl_status.cert_status) ||
        net::IsCertStatusMinorError(ssl_status.cert_status))) ||
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
  HideAutofillUI();

  in_flight_request_form_ = form;
  Send(new AutofillHostMsg_RequestAutocomplete(routing_id(), form_data, url));
}

void AutofillAgent::setIgnoreTextChanges(bool ignore) {
  ignore_text_changes_ = ignore;
}

void AutofillAgent::InputElementClicked(const WebInputElement& element,
                                        bool was_focused,
                                        bool is_focused) {
  if (was_focused)
    ShowSuggestions(element, true, false, true);
}

void AutofillAgent::InputElementLostFocus() {
  HideAutofillUI();
}

void AutofillAgent::didClearAutofillSelection(const WebNode& node) {
  if (password_autofill_agent_->DidClearAutofillSelection(node))
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

void AutofillAgent::textFieldDidEndEditing(const WebInputElement& element) {
  password_autofill_agent_->TextFieldDidEndEditing(element);
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
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&AutofillAgent::TextFieldDidChangeImpl,
                 weak_ptr_factory_.GetWeakPtr(),
                 element));
}

void AutofillAgent::TextFieldDidChangeImpl(const WebInputElement& element) {
  // If the element isn't focused then the changes don't matter. This check is
  // required to properly handle IME interactions.
  if (!element.focused())
    return;

  if (password_autofill_agent_->TextDidChangeInTextField(element)) {
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
  if (password_autofill_agent_->TextFieldHandlingKeyDown(element, event)) {
    element_ = element;
    return;
  }

  if (event.windowsKeyCode == ui::VKEY_DOWN ||
      event.windowsKeyCode == ui::VKEY_UP)
    ShowSuggestions(element, true, true, true);
}

void AutofillAgent::AcceptDataListSuggestion(
    const base::string16& suggested_value) {
  base::string16 new_value = suggested_value;
  // If this element takes multiple values then replace the last part with
  // the suggestion.
  if (element_.isMultiple() &&
      element_.formControlType() == WebString::fromUTF8("email")) {
    std::vector<base::string16> parts;

    base::SplitStringDontTrim(element_.editingValue(), ',', &parts);
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

void AutofillAgent::OnSetNodeText(const base::string16& value) {
  SetNodeText(value, &element_);
}

void AutofillAgent::OnAcceptDataListSuggestion(const base::string16& value) {
  AcceptDataListSuggestion(value);
}

void AutofillAgent::OnAcceptPasswordAutofillSuggestion(
    const base::string16& value) {
  // We need to make sure this is handled here because the browser process
  // skipped it handling because it believed it would be handled here. If it
  // isn't handled here then the browser logic needs to be updated.
  bool handled = password_autofill_agent_->DidAcceptAutofillSuggestion(
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
                                     forms_seen_timestamp_,
                                     NO_SPECIAL_FORMS_SEEN));
}

void AutofillAgent::OnRequestAutocompleteResult(
    WebFormElement::AutocompleteResult result, const FormData& form_data) {
  if (in_flight_request_form_.isNull())
    return;

  if (result == WebFormElement::AutocompleteResultSuccess) {
    FillFormIncludingNonFocusableElements(form_data, in_flight_request_form_);
    if (!in_flight_request_form_.checkValidityWithoutDispatchingEvents())
      result = WebFormElement::AutocompleteResultErrorInvalid;
  }

  in_flight_request_form_.finishRequestAutocomplete(result);
  in_flight_request_form_.reset();
}

void AutofillAgent::OnFillFormsAndClick(
    const std::vector<FormData>& forms,
    const std::vector<WebElementDescriptor>& click_elements_before_form_fill,
    const std::vector<WebElementDescriptor>& click_elements_after_form_fill,
    const WebElementDescriptor& click_element_descriptor) {
  DCHECK_EQ(forms.size(), form_elements_.size());

  // Click elements in click_elements_before_form_fill.
  for (size_t i = 0; i < click_elements_before_form_fill.size(); ++i) {
    if (!ClickElement(topmost_frame_->document(),
                      click_elements_before_form_fill[i])) {
      CompleteAutocheckoutPage(MISSING_CLICK_ELEMENT_BEFORE_FORM_FILLING);
      return;
    }
  }

  // Fill the form.
  for (size_t i = 0; i < forms.size(); ++i)
    FillFormForAllElements(forms[i], form_elements_[i]);

  // Click elements in click_elements_after_form_fill.
  for (size_t i = 0; i < click_elements_after_form_fill.size(); ++i) {
    if (!ClickElement(topmost_frame_->document(),
                      click_elements_after_form_fill[i])) {
      CompleteAutocheckoutPage(MISSING_CLICK_ELEMENT_AFTER_FORM_FILLING);
      return;
    }
  }

  // Exit early if there is nothing to click.
  if (click_element_descriptor.retrieval_method == WebElementDescriptor::NONE) {
    CompleteAutocheckoutPage(SUCCESS);
    return;
  }

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
    CompleteAutocheckoutPage(MISSING_ADVANCE);
  }
}

void AutofillAgent::OnAutocheckoutSupported() {
  is_autocheckout_supported_ = true;
  if (has_new_forms_for_browser_)
    MaybeSendDynamicFormsSeen();
  MaybeShowAutocheckoutBubble();
}

void AutofillAgent::OnPageShown() {
  if (is_autocheckout_supported_)
    MaybeShowAutocheckoutBubble();
}

void AutofillAgent::CompleteAutocheckoutPage(
    autofill::AutocheckoutStatus status) {
  click_timer_.Stop();
  Send(new AutofillHostMsg_AutocheckoutPageCompleted(routing_id(), status));
}

void AutofillAgent::ClickFailed() {
  CompleteAutocheckoutPage(CANNOT_PROCEED);
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
    HideAutofillUI();
    return;
  }

  element_ = element;
  if (password_autofill_agent_->ShowSuggestions(element))
    return;

  // If autocomplete is disabled at the field level, ensure that the native
  // UI won't try to show a warning, since that may conflict with a custom
  // popup. Note that we cannot use the WebKit method element.autoComplete()
  // as it does not allow us to distinguish the case where autocomplete is
  // disabled for *both* the element and for the form.
  const base::string16 autocomplete_attribute =
      element.getAttribute("autocomplete");
  if (LowerCaseEqualsASCII(autocomplete_attribute, "off"))
    display_warning_if_disabled = false;

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
  std::vector<base::string16> data_list_values;
  std::vector<base::string16> data_list_labels;
  GetDataListSuggestions(element_, &data_list_values, &data_list_labels);
  TrimStringVectorForIPC(&data_list_values);
  TrimStringVectorForIPC(&data_list_labels);

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

void AutofillAgent::SetNodeText(const base::string16& value,
                                WebKit::WebInputElement* node) {
  did_set_node_text_ = true;
  base::string16 substring = value;
  substring = substring.substr(0, node->maxLength());

  node->setEditingValue(substring);
}

void AutofillAgent::HideAutofillUI() {
  Send(new AutofillHostMsg_HideAutofillUI(routing_id()));
}

void AutofillAgent::didAssociateFormControls(
    const WebKit::WebVector<WebKit::WebNode>& nodes) {
  for (size_t i = 0; i < nodes.size(); ++i) {
    WebKit::WebNode node = nodes[i];
    if (node.document().frame() == topmost_frame_) {
      forms_seen_timestamp_ = base::TimeTicks::Now();
      has_new_forms_for_browser_ = true;
      break;
    }
  }

  if (has_new_forms_for_browser_ && is_autocheckout_supported_)
    MaybeSendDynamicFormsSeen();
}

void AutofillAgent::MaybeSendDynamicFormsSeen() {
  has_new_forms_for_browser_ = false;
  form_elements_.clear();
  std::vector<FormData> forms;
  // This will only be called for Autocheckout flows, so send all forms to
  // save an IPC.
  form_cache_.ExtractFormsAndFormElements(
      *topmost_frame_, 0, &forms, &form_elements_);
  autofill::FormsSeenState state = autofill::DYNAMIC_FORMS_SEEN;

  if (!forms.empty()) {
    if (click_timer_.IsRunning())
      click_timer_.Stop();
    Send(new AutofillHostMsg_FormsSeen(routing_id(), forms,
                                       forms_seen_timestamp_,
                                       state));
  }
}

}  // namespace autofill
