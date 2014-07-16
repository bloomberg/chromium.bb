// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_autofill_agent.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/password_form_conversion_utils.h"
#include "components/autofill/content/renderer/renderer_save_password_progress_logger.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_autofill_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/navigation_state.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebAutofillClient.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFormElement.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebNode.h"
#include "third_party/WebKit/public/web/WebNodeList.h"
#include "third_party/WebKit/public/web/WebSecurityOrigin.h"
#include "third_party/WebKit/public/web/WebUserGestureIndicator.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

namespace autofill {
namespace {

// The size above which we stop triggering autocomplete.
static const size_t kMaximumTextSizeForAutocomplete = 1000;

// Maps element names to the actual elements to simplify form filling.
typedef std::map<base::string16, blink::WebInputElement> FormInputElementMap;

// Use the shorter name when referencing SavePasswordProgressLogger::StringID
// values to spare line breaks. The code provides enough context for that
// already.
typedef SavePasswordProgressLogger Logger;

// Utility struct for form lookup and autofill. When we parse the DOM to look up
// a form, in addition to action and origin URL's we have to compare all
// necessary form elements. To avoid having to look these up again when we want
// to fill the form, the FindFormElements function stores the pointers
// in a FormElements* result, referenced to ensure they are safe to use.
struct FormElements {
  blink::WebFormElement form_element;
  FormInputElementMap input_elements;
};

typedef std::vector<FormElements*> FormElementsList;

// Helper to search the given form element for the specified input elements
// in |data|, and add results to |result|.
static bool FindFormInputElements(blink::WebFormElement* fe,
                                  const FormData& data,
                                  FormElements* result) {
  // Loop through the list of elements we need to find on the form in order to
  // autofill it. If we don't find any one of them, abort processing this
  // form; it can't be the right one.
  for (size_t j = 0; j < data.fields.size(); j++) {
    blink::WebVector<blink::WebNode> temp_elements;
    fe->getNamedElements(data.fields[j].name, temp_elements);

    // Match the first input element, if any.
    // |getNamedElements| may return non-input elements where the names match,
    // so the results are filtered for input elements.
    // If more than one match is made, then we have ambiguity (due to misuse
    // of "name" attribute) so is it considered not found.
    bool found_input = false;
    for (size_t i = 0; i < temp_elements.size(); ++i) {
      if (temp_elements[i].to<blink::WebElement>().hasHTMLTagName("input")) {
        // Check for a non-unique match.
        if (found_input) {
          found_input = false;
          break;
        }

        // Only fill saved passwords into password fields and usernames into
        // text fields.
        blink::WebInputElement input_element =
            temp_elements[i].to<blink::WebInputElement>();
        if (input_element.isPasswordField() !=
            (data.fields[j].form_control_type == "password"))
          continue;

        // This element matched, add it to our temporary result. It's possible
        // there are multiple matches, but for purposes of identifying the form
        // one suffices and if some function needs to deal with multiple
        // matching elements it can get at them through the FormElement*.
        // Note: This assignment adds a reference to the InputElement.
        result->input_elements[data.fields[j].name] = input_element;
        found_input = true;
      }
    }

    // A required element was not found. This is not the right form.
    // Make sure no input elements from a partially matched form in this
    // iteration remain in the result set.
    // Note: clear will remove a reference from each InputElement.
    if (!found_input) {
      result->input_elements.clear();
      return false;
    }
  }
  return true;
}

// Helper to locate form elements identified by |data|.
void FindFormElements(blink::WebView* view,
                      const FormData& data,
                      FormElementsList* results) {
  DCHECK(view);
  DCHECK(results);
  blink::WebFrame* main_frame = view->mainFrame();
  if (!main_frame)
    return;

  GURL::Replacements rep;
  rep.ClearQuery();
  rep.ClearRef();

  // Loop through each frame.
  for (blink::WebFrame* f = main_frame; f; f = f->traverseNext(false)) {
    blink::WebDocument doc = f->document();
    if (!doc.isHTMLDocument())
      continue;

    GURL full_origin(doc.url());
    if (data.origin != full_origin.ReplaceComponents(rep))
      continue;

    blink::WebVector<blink::WebFormElement> forms;
    doc.forms(forms);

    for (size_t i = 0; i < forms.size(); ++i) {
      blink::WebFormElement fe = forms[i];

      GURL full_action(f->document().completeURL(fe.action()));
      if (full_action.is_empty()) {
        // The default action URL is the form's origin.
        full_action = full_origin;
      }

      // Action URL must match.
      if (data.action != full_action.ReplaceComponents(rep))
        continue;

      scoped_ptr<FormElements> curr_elements(new FormElements);
      if (!FindFormInputElements(&fe, data, curr_elements.get()))
        continue;

      // We found the right element.
      // Note: this assignment adds a reference to |fe|.
      curr_elements->form_element = fe;
      results->push_back(curr_elements.release());
    }
  }
}

bool IsElementEditable(const blink::WebInputElement& element) {
  return element.isEnabled() && !element.isReadOnly();
}

bool DoUsernamesMatch(const base::string16& username1,
                      const base::string16& username2,
                      bool exact_match) {
  if (exact_match)
    return username1 == username2;
  return StartsWith(username1, username2, true);
}

// Returns |true| if the given element is both editable and has permission to be
// autocompleted. The latter can be either because there is no
// autocomplete='off' set for the element, or because the flag is set to ignore
// autocomplete='off'. Otherwise, returns |false|.
bool IsElementAutocompletable(const blink::WebInputElement& element) {
  return IsElementEditable(element) &&
         (ShouldIgnoreAutocompleteOffForPasswordFields() ||
          element.autoComplete());
}

// Returns true if the password specified in |form| is a default value.
bool PasswordValueIsDefault(const PasswordForm& form,
                            blink::WebFormElement form_element) {
  blink::WebVector<blink::WebNode> temp_elements;
  form_element.getNamedElements(form.password_element, temp_elements);

  // We are loose in our definition here and will return true if any of the
  // appropriately named elements match the element to be saved. Currently
  // we ignore filling passwords where naming is ambigious anyway.
  for (size_t i = 0; i < temp_elements.size(); ++i) {
    if (temp_elements[i].to<blink::WebElement>().getAttribute("value") ==
        form.password_value)
      return true;
  }
  return false;
}

// Log a message including the name, method and action of |form|.
void LogHTMLForm(SavePasswordProgressLogger* logger,
                 SavePasswordProgressLogger::StringID message_id,
                 const blink::WebFormElement& form) {
  logger->LogHTMLForm(message_id,
                      form.name().utf8(),
                      GURL(form.action().utf8()));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillAgent, public:

PasswordAutofillAgent::PasswordAutofillAgent(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      usernames_usage_(NOTHING_TO_AUTOFILL),
      web_view_(render_view->GetWebView()),
      logging_state_active_(false),
      was_username_autofilled_(false),
      was_password_autofilled_(false),
      username_selection_start_(0),
      did_stop_loading_(false),
      weak_ptr_factory_(this) {
  Send(new AutofillHostMsg_PasswordAutofillAgentConstructed(routing_id()));
}

PasswordAutofillAgent::~PasswordAutofillAgent() {
}

PasswordAutofillAgent::PasswordValueGatekeeper::PasswordValueGatekeeper()
    : was_user_gesture_seen_(false) {
}

PasswordAutofillAgent::PasswordValueGatekeeper::~PasswordValueGatekeeper() {
}

void PasswordAutofillAgent::PasswordValueGatekeeper::RegisterElement(
    blink::WebInputElement* element) {
  if (was_user_gesture_seen_)
    ShowValue(element);
  else
    elements_.push_back(*element);
}

void PasswordAutofillAgent::PasswordValueGatekeeper::OnUserGesture() {
  was_user_gesture_seen_ = true;

  for (std::vector<blink::WebInputElement>::iterator it = elements_.begin();
       it != elements_.end();
       ++it) {
    ShowValue(&(*it));
  }

  elements_.clear();
}

void PasswordAutofillAgent::PasswordValueGatekeeper::Reset() {
  was_user_gesture_seen_ = false;
  elements_.clear();
}

void PasswordAutofillAgent::PasswordValueGatekeeper::ShowValue(
    blink::WebInputElement* element) {
  if (!element->isNull() && !element->suggestedValue().isNull())
    element->setValue(element->suggestedValue(), true);
}

bool PasswordAutofillAgent::TextFieldDidEndEditing(
    const blink::WebInputElement& element) {
  LoginToPasswordInfoMap::const_iterator iter =
      login_to_password_info_.find(element);
  if (iter == login_to_password_info_.end())
    return false;

  const PasswordFormFillData& fill_data = iter->second.fill_data;

  // If wait_for_username is false, we should have filled when the text changed.
  if (!fill_data.wait_for_username)
    return false;

  blink::WebInputElement password = iter->second.password_field;
  if (!IsElementEditable(password))
    return false;

  blink::WebInputElement username = element;  // We need a non-const.

  // Do not set selection when ending an editing session, otherwise it can
  // mess with focus.
  FillUserNameAndPassword(&username,
                          &password,
                          fill_data,
                          true /* exact_username_match */,
                          false /* set_selection */);
  return true;
}

bool PasswordAutofillAgent::TextDidChangeInTextField(
    const blink::WebInputElement& element) {
  LoginToPasswordInfoMap::const_iterator iter =
      login_to_password_info_.find(element);
  if (iter == login_to_password_info_.end())
    return false;

  // The input text is being changed, so any autofilled password is now
  // outdated.
  blink::WebInputElement username = element;  // We need a non-const.
  username.setAutofilled(false);

  blink::WebInputElement password = iter->second.password_field;
  if (password.isAutofilled()) {
    password.setValue(base::string16(), true);
    password.setAutofilled(false);
  }

  // If wait_for_username is true we will fill when the username loses focus.
  if (iter->second.fill_data.wait_for_username)
    return false;

  if (!element.isText() || !IsElementAutocompletable(element) ||
      !IsElementAutocompletable(password)) {
    return false;
  }

  // Don't inline autocomplete if the user is deleting, that would be confusing.
  // But refresh the popup.  Note, since this is ours, return true to signal
  // no further processing is required.
  if (iter->second.backspace_pressed_last) {
    ShowSuggestionPopup(iter->second.fill_data, username, false);
    return true;
  }

  blink::WebString name = element.nameForAutofill();
  if (name.isEmpty())
    return false;  // If the field has no name, then we won't have values.

  // Don't attempt to autofill with values that are too large.
  if (element.value().length() > kMaximumTextSizeForAutocomplete)
    return false;

  // The caret position should have already been updated.
  PerformInlineAutocomplete(element, password, iter->second.fill_data);
  return true;
}

bool PasswordAutofillAgent::TextFieldHandlingKeyDown(
    const blink::WebInputElement& element,
    const blink::WebKeyboardEvent& event) {
  // If using the new Autofill UI that lives in the browser, it will handle
  // keypresses before this function. This is not currently an issue but if
  // the keys handled there or here change, this issue may appear.

  LoginToPasswordInfoMap::iterator iter = login_to_password_info_.find(element);
  if (iter == login_to_password_info_.end())
    return false;

  int win_key_code = event.windowsKeyCode;
  iter->second.backspace_pressed_last =
      (win_key_code == ui::VKEY_BACK || win_key_code == ui::VKEY_DELETE);
  return true;
}

bool PasswordAutofillAgent::FillSuggestion(
    const blink::WebNode& node,
    const blink::WebString& username,
    const blink::WebString& password) {
  blink::WebInputElement username_element;
  PasswordInfo password_info;

  if (!FindLoginInfo(node, &username_element, &password_info) ||
      !IsElementAutocompletable(username_element) ||
      !IsElementAutocompletable(password_info.password_field)) {
    return false;
  }

  base::string16 current_username = username_element.value();
  username_element.setValue(username, true);
  username_element.setAutofilled(true);
  username_element.setSelectionRange(username.length(), username.length());

  password_info.password_field.setValue(password, true);
  password_info.password_field.setAutofilled(true);

  return true;
}

bool PasswordAutofillAgent::PreviewSuggestion(
    const blink::WebNode& node,
    const blink::WebString& username,
    const blink::WebString& password) {
  blink::WebInputElement username_element;
  PasswordInfo password_info;

  if (!FindLoginInfo(node, &username_element, &password_info) ||
      !IsElementAutocompletable(username_element) ||
      !IsElementAutocompletable(password_info.password_field)) {
    return false;
  }

  was_username_autofilled_ = username_element.isAutofilled();
  username_selection_start_ = username_element.selectionStart();
  username_element.setSuggestedValue(username);
  username_element.setAutofilled(true);
  username_element.setSelectionRange(
      username_selection_start_,
      username_element.suggestedValue().length());

  was_password_autofilled_ = password_info.password_field.isAutofilled();
  password_info.password_field.setSuggestedValue(password);
  password_info.password_field.setAutofilled(true);

  return true;
}

bool PasswordAutofillAgent::DidClearAutofillSelection(
    const blink::WebNode& node) {
  blink::WebInputElement username_element;
  PasswordInfo password_info;
  if (!FindLoginInfo(node, &username_element, &password_info))
    return false;

  ClearPreview(&username_element, &password_info.password_field);
  return true;
}

bool PasswordAutofillAgent::ShowSuggestions(
    const blink::WebInputElement& element,
    bool show_all) {
  LoginToPasswordInfoMap::const_iterator iter =
      login_to_password_info_.find(element);
  if (iter == login_to_password_info_.end())
    return false;

  // If autocomplete='off' is set on the form elements, no suggestion dialog
  // should be shown. However, return |true| to indicate that this is a known
  // password form and that the request to show suggestions has been handled (as
  // a no-op).
  if (!IsElementAutocompletable(element) ||
      !IsElementAutocompletable(iter->second.password_field))
    return true;

  return ShowSuggestionPopup(iter->second.fill_data, element, show_all);
}

bool PasswordAutofillAgent::OriginCanAccessPasswordManager(
    const blink::WebSecurityOrigin& origin) {
  return origin.canAccessPasswordManager();
}

void PasswordAutofillAgent::OnDynamicFormsSeen(blink::WebFrame* frame) {
  SendPasswordForms(frame, false /* only_visible */);
}

void PasswordAutofillAgent::FirstUserGestureObserved() {
  gatekeeper_.OnUserGesture();
}

void PasswordAutofillAgent::SendPasswordForms(blink::WebFrame* frame,
                                              bool only_visible) {
  scoped_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(this, routing_id()));
    logger->LogMessage(Logger::STRING_SEND_PASSWORD_FORMS_METHOD);
    logger->LogBoolean(Logger::STRING_ONLY_VISIBLE, only_visible);
  }

  // Make sure that this security origin is allowed to use password manager.
  blink::WebSecurityOrigin origin = frame->document().securityOrigin();
  if (logger) {
    logger->LogURL(Logger::STRING_SECURITY_ORIGIN,
                   GURL(origin.toString().utf8()));
  }
  if (!OriginCanAccessPasswordManager(origin)) {
    if (logger) {
      logger->LogMessage(Logger::STRING_SECURITY_ORIGIN_FAILURE);
      logger->LogMessage(Logger::STRING_DECISION_DROP);
    }
    return;
  }

  // Checks whether the webpage is a redirect page or an empty page.
  if (IsWebpageEmpty(frame)) {
    if (logger) {
      logger->LogMessage(Logger::STRING_WEBPAGE_EMPTY);
      logger->LogMessage(Logger::STRING_DECISION_DROP);
    }
    return;
  }

  blink::WebVector<blink::WebFormElement> forms;
  frame->document().forms(forms);
  if (logger)
    logger->LogNumber(Logger::STRING_NUMBER_OF_ALL_FORMS, forms.size());

  std::vector<PasswordForm> password_forms;
  for (size_t i = 0; i < forms.size(); ++i) {
    const blink::WebFormElement& form = forms[i];
    bool is_form_visible = IsWebNodeVisible(form);
    if (logger) {
      LogHTMLForm(logger.get(), Logger::STRING_FORM_FOUND_ON_PAGE, form);
      logger->LogBoolean(Logger::STRING_FORM_IS_VISIBLE, is_form_visible);
    }

    // If requested, ignore non-rendered forms, e.g. those styled with
    // display:none.
    if (only_visible && !is_form_visible)
      continue;

    scoped_ptr<PasswordForm> password_form(CreatePasswordForm(form));
    if (password_form.get()) {
      if (logger) {
        logger->LogPasswordForm(Logger::STRING_FORM_IS_PASSWORD,
                                *password_form);
      }
      password_forms.push_back(*password_form);
    }
  }

  if (password_forms.empty() && !only_visible) {
    // We need to send the PasswordFormsRendered message regardless of whether
    // there are any forms visible, as this is also the code path that triggers
    // showing the infobar.
    return;
  }

  if (only_visible) {
    Send(new AutofillHostMsg_PasswordFormsRendered(routing_id(),
                                                   password_forms,
                                                   did_stop_loading_));
  } else {
    Send(new AutofillHostMsg_PasswordFormsParsed(routing_id(), password_forms));
  }
}

bool PasswordAutofillAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PasswordAutofillAgent, message)
    IPC_MESSAGE_HANDLER(AutofillMsg_FillPasswordForm, OnFillPasswordForm)
    IPC_MESSAGE_HANDLER(AutofillMsg_SetLoggingState, OnSetLoggingState)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PasswordAutofillAgent::DidStartLoading() {
  did_stop_loading_ = false;
  if (usernames_usage_ != NOTHING_TO_AUTOFILL) {
    UMA_HISTOGRAM_ENUMERATION("PasswordManager.OtherPossibleUsernamesUsage",
                              usernames_usage_,
                              OTHER_POSSIBLE_USERNAMES_MAX);
    usernames_usage_ = NOTHING_TO_AUTOFILL;
  }
}

void PasswordAutofillAgent::DidFinishDocumentLoad(blink::WebLocalFrame* frame) {
  // The |frame| contents have been parsed, but not yet rendered.  Let the
  // PasswordManager know that forms are loaded, even though we can't yet tell
  // whether they're visible.
  SendPasswordForms(frame, false);
}

void PasswordAutofillAgent::DidFinishLoad(blink::WebLocalFrame* frame) {
  // The |frame| contents have been rendered.  Let the PasswordManager know
  // which of the loaded frames are actually visible to the user.  This also
  // triggers the "Save password?" infobar if the user just submitted a password
  // form.
  SendPasswordForms(frame, true);
}

void PasswordAutofillAgent::DidStopLoading() {
  did_stop_loading_ = true;
}

void PasswordAutofillAgent::FrameDetached(blink::WebFrame* frame) {
  FrameClosing(frame);
}

void PasswordAutofillAgent::FrameWillClose(blink::WebFrame* frame) {
  FrameClosing(frame);
}

void PasswordAutofillAgent::WillSendSubmitEvent(
    blink::WebLocalFrame* frame,
    const blink::WebFormElement& form) {
  // Some login forms have onSubmit handlers that put a hash of the password
  // into a hidden field and then clear the password (http://crbug.com/28910).
  // This method gets called before any of those handlers run, so save away
  // a copy of the password in case it gets lost.
  scoped_ptr<PasswordForm> password_form(CreatePasswordForm(form));
  if (password_form)
    provisionally_saved_forms_[frame].reset(password_form.release());
}

void PasswordAutofillAgent::WillSubmitForm(blink::WebLocalFrame* frame,
                                           const blink::WebFormElement& form) {
  scoped_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(this, routing_id()));
    logger->LogMessage(Logger::STRING_WILL_SUBMIT_FORM_METHOD);
    LogHTMLForm(logger.get(), Logger::STRING_HTML_FORM_FOR_SUBMIT, form);
  }

  scoped_ptr<PasswordForm> submitted_form = CreatePasswordForm(form);

  // If there is a provisionally saved password, copy over the previous
  // password value so we get the user's typed password, not the value that
  // may have been transformed for submit.
  // TODO(gcasto): Do we need to have this action equality check? Is it trying
  // to prevent accidentally copying over passwords from a different form?
  if (submitted_form) {
    if (logger) {
      logger->LogPasswordForm(Logger::STRING_CREATED_PASSWORD_FORM,
                              *submitted_form);
    }
    if (provisionally_saved_forms_[frame].get() &&
        submitted_form->action == provisionally_saved_forms_[frame]->action) {
      if (logger)
        logger->LogMessage(Logger::STRING_SUBMITTED_PASSWORD_REPLACED);
      submitted_form->password_value =
          provisionally_saved_forms_[frame]->password_value;
    }

    // Some observers depend on sending this information now instead of when
    // the frame starts loading. If there are redirects that cause a new
    // RenderView to be instantiated (such as redirects to the WebStore)
    // we will never get to finish the load.
    Send(new AutofillHostMsg_PasswordFormSubmitted(routing_id(),
                                                   *submitted_form));
    // Remove reference since we have already submitted this form.
    provisionally_saved_forms_.erase(frame);
  } else if (logger) {
    logger->LogMessage(Logger::STRING_DECISION_DROP);
  }
}

blink::WebFrame* PasswordAutofillAgent::CurrentOrChildFrameWithSavedForms(
    const blink::WebFrame* current_frame) {
  for (FrameToPasswordFormMap::const_iterator it =
           provisionally_saved_forms_.begin();
       it != provisionally_saved_forms_.end();
       ++it) {
    blink::WebFrame* form_frame = it->first;
    // The check that the returned frame is related to |current_frame| is mainly
    // for double-checking. There should not be any unrelated frames in
    // |provisionally_saved_forms_|, because the map is cleared after
    // navigation. If there are reasons to remove this check in the future and
    // keep just the first frame found, it might be a good idea to add a UMA
    // statistic or a similar check on how many frames are here to choose from.
    if (current_frame == form_frame ||
        current_frame->findChildByName(form_frame->assignedName())) {
      return form_frame;
    }
  }
  return NULL;
}

void PasswordAutofillAgent::DidStartProvisionalLoad(
    blink::WebLocalFrame* frame) {
  scoped_ptr<RendererSavePasswordProgressLogger> logger;
  if (logging_state_active_) {
    logger.reset(new RendererSavePasswordProgressLogger(this, routing_id()));
    logger->LogMessage(Logger::STRING_DID_START_PROVISIONAL_LOAD_METHOD);
  }

  if (!frame->parent()) {
    // If the navigation is not triggered by a user gesture, e.g. by some ajax
    // callback, then inherit the submitted password form from the previous
    // state. This fixes the no password save issue for ajax login, tracked in
    // [http://crbug/43219]. Note that this still fails for sites that use
    // synchonous XHR as isProcessingUserGesture() will return true.
    blink::WebFrame* form_frame = CurrentOrChildFrameWithSavedForms(frame);
    if (logger) {
      logger->LogBoolean(Logger::STRING_FORM_FRAME_EQ_FRAME,
                         form_frame == frame);
    }
    // Bug fix for crbug.com/368690. isProcessingUserGesture() is false when
    // the user is performing actions outside the page (e.g. typed url,
    // history navigation). We don't want to trigger saving in these cases.
    content::DocumentState* document_state =
        content::DocumentState::FromDataSource(
            frame->provisionalDataSource());
    content::NavigationState* navigation_state =
        document_state->navigation_state();
    if (content::PageTransitionIsWebTriggerable(
            navigation_state->transition_type()) &&
        !blink::WebUserGestureIndicator::isProcessingUserGesture()) {
      // If onsubmit has been called, try and save that form.
      if (provisionally_saved_forms_[form_frame].get()) {
        if (logger) {
          logger->LogPasswordForm(
              Logger::STRING_PROVISIONALLY_SAVED_FORM_FOR_FRAME,
              *provisionally_saved_forms_[form_frame]);
        }
        Send(new AutofillHostMsg_PasswordFormSubmitted(
            routing_id(), *provisionally_saved_forms_[form_frame]));
        provisionally_saved_forms_.erase(form_frame);
      } else {
        // Loop through the forms on the page looking for one that has been
        // filled out. If one exists, try and save the credentials.
        blink::WebVector<blink::WebFormElement> forms;
        frame->document().forms(forms);

        bool password_forms_found = false;
        for (size_t i = 0; i < forms.size(); ++i) {
          blink::WebFormElement form_element = forms[i];
          if (logger) {
            LogHTMLForm(
                logger.get(), Logger::STRING_FORM_FOUND_ON_PAGE, form_element);
          }
          scoped_ptr<PasswordForm> password_form(
              CreatePasswordForm(form_element));
          if (password_form.get() && !password_form->username_value.empty() &&
              !password_form->password_value.empty() &&
              !PasswordValueIsDefault(*password_form, form_element)) {
            password_forms_found = true;
            if (logger) {
              logger->LogPasswordForm(
                  Logger::STRING_PASSWORD_FORM_FOUND_ON_PAGE, *password_form);
            }
            Send(new AutofillHostMsg_PasswordFormSubmitted(routing_id(),
                                                           *password_form));
          }
        }
        if (!password_forms_found && logger) {
          logger->LogMessage(Logger::STRING_DECISION_DROP);
        }
      }
    }
    // Clear the whole map during main frame navigation.
    provisionally_saved_forms_.clear();

    // This is a new navigation, so require a new user gesture before filling in
    // passwords.
    gatekeeper_.Reset();
  } else {
    if (logger)
      logger->LogMessage(Logger::STRING_DECISION_DROP);
  }
}

void PasswordAutofillAgent::OnFillPasswordForm(
    const PasswordFormFillData& form_data) {
  if (usernames_usage_ == NOTHING_TO_AUTOFILL) {
    if (form_data.other_possible_usernames.size())
      usernames_usage_ = OTHER_POSSIBLE_USERNAMES_PRESENT;
    else if (usernames_usage_ == NOTHING_TO_AUTOFILL)
      usernames_usage_ = OTHER_POSSIBLE_USERNAMES_ABSENT;
  }

  FormElementsList forms;
  // We own the FormElements* in forms.
  FindFormElements(render_view()->GetWebView(), form_data.basic_data, &forms);
  FormElementsList::iterator iter;
  for (iter = forms.begin(); iter != forms.end(); ++iter) {
    scoped_ptr<FormElements> form_elements(*iter);

    // Attach autocomplete listener to enable selecting alternate logins.
    // First, get pointers to username element.
    blink::WebInputElement username_element =
        form_elements->input_elements[form_data.basic_data.fields[0].name];

    // Get pointer to password element. (We currently only support single
    // password forms).
    blink::WebInputElement password_element =
        form_elements->input_elements[form_data.basic_data.fields[1].name];

    // If wait_for_username is true, we don't want to initially fill the form
    // until the user types in a valid username.
    if (!form_data.wait_for_username)
      FillFormOnPasswordRecieved(form_data, username_element, password_element);

    // We might have already filled this form if there are two <form> elements
    // with identical markup.
    if (login_to_password_info_.find(username_element) !=
        login_to_password_info_.end())
      continue;

    PasswordInfo password_info;
    password_info.fill_data = form_data;
    password_info.password_field = password_element;
    login_to_password_info_[username_element] = password_info;

    FormData form;
    FormFieldData field;
    FindFormAndFieldForFormControlElement(
        username_element, &form, &field, REQUIRE_NONE);
    Send(new AutofillHostMsg_AddPasswordFormMapping(
        routing_id(), field, form_data));
  }
}

void PasswordAutofillAgent::OnSetLoggingState(bool active) {
  logging_state_active_ = active;
}

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillAgent, private:

void PasswordAutofillAgent::GetSuggestions(
    const PasswordFormFillData& fill_data,
    const base::string16& input,
    std::vector<base::string16>* suggestions,
    std::vector<base::string16>* realms,
    bool show_all) {
  if (show_all ||
      StartsWith(fill_data.basic_data.fields[0].value, input, false)) {
    suggestions->push_back(fill_data.basic_data.fields[0].value);
    realms->push_back(base::UTF8ToUTF16(fill_data.preferred_realm));
  }

  for (PasswordFormFillData::LoginCollection::const_iterator iter =
           fill_data.additional_logins.begin();
       iter != fill_data.additional_logins.end();
       ++iter) {
    if (show_all || StartsWith(iter->first, input, false)) {
      suggestions->push_back(iter->first);
      realms->push_back(base::UTF8ToUTF16(iter->second.realm));
    }
  }

  for (PasswordFormFillData::UsernamesCollection::const_iterator iter =
           fill_data.other_possible_usernames.begin();
       iter != fill_data.other_possible_usernames.end();
       ++iter) {
    for (size_t i = 0; i < iter->second.size(); ++i) {
      if (show_all || StartsWith(iter->second[i], input, false)) {
        usernames_usage_ = OTHER_POSSIBLE_USERNAME_SHOWN;
        suggestions->push_back(iter->second[i]);
        realms->push_back(base::UTF8ToUTF16(iter->first.realm));
      }
    }
  }
}

bool PasswordAutofillAgent::ShowSuggestionPopup(
    const PasswordFormFillData& fill_data,
    const blink::WebInputElement& user_input,
    bool show_all) {
  blink::WebFrame* frame = user_input.document().frame();
  if (!frame)
    return false;

  blink::WebView* webview = frame->view();
  if (!webview)
    return false;

  std::vector<base::string16> suggestions;
  std::vector<base::string16> realms;
  GetSuggestions(
      fill_data, user_input.value(), &suggestions, &realms, show_all);
  DCHECK_EQ(suggestions.size(), realms.size());

  FormData form;
  FormFieldData field;
  FindFormAndFieldForFormControlElement(
      user_input, &form, &field, REQUIRE_NONE);

  blink::WebInputElement selected_element = user_input;
  gfx::Rect bounding_box(selected_element.boundsInViewportSpace());

  float scale = web_view_->pageScaleFactor();
  gfx::RectF bounding_box_scaled(bounding_box.x() * scale,
                                 bounding_box.y() * scale,
                                 bounding_box.width() * scale,
                                 bounding_box.height() * scale);
  Send(new AutofillHostMsg_ShowPasswordSuggestions(
      routing_id(), field, bounding_box_scaled, suggestions, realms));
  return !suggestions.empty();
}

void PasswordAutofillAgent::FillFormOnPasswordRecieved(
    const PasswordFormFillData& fill_data,
    blink::WebInputElement username_element,
    blink::WebInputElement password_element) {
  // Do not fill if the password field is in an iframe.
  DCHECK(password_element.document().frame());
  if (password_element.document().frame()->parent())
    return;

  if (!ShouldIgnoreAutocompleteOffForPasswordFields() &&
      !username_element.form().autoComplete())
    return;

  // If we can't modify the password, don't try to set the username
  if (!IsElementAutocompletable(password_element))
    return;

  // Try to set the username to the preferred name, but only if the field
  // can be set and isn't prefilled.
  if (IsElementAutocompletable(username_element) &&
      username_element.value().isEmpty()) {
    // TODO(tkent): Check maxlength and pattern.
    username_element.setValue(fill_data.basic_data.fields[0].value, true);
  }

  // Fill if we have an exact match for the username. Note that this sets
  // username to autofilled.
  FillUserNameAndPassword(&username_element,
                          &password_element,
                          fill_data,
                          true /* exact_username_match */,
                          false /* set_selection */);
}

bool PasswordAutofillAgent::FillUserNameAndPassword(
    blink::WebInputElement* username_element,
    blink::WebInputElement* password_element,
    const PasswordFormFillData& fill_data,
    bool exact_username_match,
    bool set_selection) {
  base::string16 current_username = username_element->value();
  // username and password will contain the match found if any.
  base::string16 username;
  base::string16 password;

  // Look for any suitable matches to current field text.
  if (DoUsernamesMatch(fill_data.basic_data.fields[0].value,
                       current_username,
                       exact_username_match)) {
    username = fill_data.basic_data.fields[0].value;
    password = fill_data.basic_data.fields[1].value;
  } else {
    // Scan additional logins for a match.
    PasswordFormFillData::LoginCollection::const_iterator iter;
    for (iter = fill_data.additional_logins.begin();
         iter != fill_data.additional_logins.end();
         ++iter) {
      if (DoUsernamesMatch(
              iter->first, current_username, exact_username_match)) {
        username = iter->first;
        password = iter->second.password;
        break;
      }
    }

    // Check possible usernames.
    if (username.empty() && password.empty()) {
      for (PasswordFormFillData::UsernamesCollection::const_iterator iter =
               fill_data.other_possible_usernames.begin();
           iter != fill_data.other_possible_usernames.end();
           ++iter) {
        for (size_t i = 0; i < iter->second.size(); ++i) {
          if (DoUsernamesMatch(
                  iter->second[i], current_username, exact_username_match)) {
            usernames_usage_ = OTHER_POSSIBLE_USERNAME_SELECTED;
            username = iter->second[i];
            password = iter->first.password;
            break;
          }
        }
        if (!username.empty() && !password.empty())
          break;
      }
    }
  }
  if (password.empty())
    return false;  // No match was found.

  // TODO(tkent): Check maxlength and pattern for both username and password
  // fields.

  // Don't fill username if password can't be set.
  if (!IsElementAutocompletable(*password_element)) {
    return false;
  }

  // Input matches the username, fill in required values.
  if (IsElementAutocompletable(*username_element)) {
    username_element->setValue(username, true);
    username_element->setAutofilled(true);

    if (set_selection) {
      username_element->setSelectionRange(current_username.length(),
                                          username.length());
    }
  } else if (current_username != username) {
    // If the username can't be filled and it doesn't match a saved password
    // as is, don't autofill a password.
    return false;
  }

  // Wait to fill in the password until a user gesture occurs. This is to make
  // sure that we do not fill in the DOM with a password until we believe the
  // user is intentionally interacting with the page.
  password_element->setSuggestedValue(password);
  gatekeeper_.RegisterElement(password_element);

  password_element->setAutofilled(true);
  return true;
}

void PasswordAutofillAgent::PerformInlineAutocomplete(
    const blink::WebInputElement& username_input,
    const blink::WebInputElement& password_input,
    const PasswordFormFillData& fill_data) {
  DCHECK(!fill_data.wait_for_username);

  // We need non-const versions of the username and password inputs.
  blink::WebInputElement username = username_input;
  blink::WebInputElement password = password_input;

  // Don't inline autocomplete if the caret is not at the end.
  // TODO(jcivelli): is there a better way to test the caret location?
  if (username.selectionStart() != username.selectionEnd() ||
      username.selectionEnd() != static_cast<int>(username.value().length())) {
    return;
  }

  // Show the popup with the list of available usernames.
  ShowSuggestionPopup(fill_data, username, false);

#if !defined(OS_ANDROID)
  // Fill the user and password field with the most relevant match. Android
  // only fills in the fields after the user clicks on the suggestion popup.
  FillUserNameAndPassword(&username,
                          &password,
                          fill_data,
                          false /* exact_username_match */,
                          true /* set_selection */);
#endif
}

void PasswordAutofillAgent::FrameClosing(const blink::WebFrame* frame) {
  for (LoginToPasswordInfoMap::iterator iter = login_to_password_info_.begin();
       iter != login_to_password_info_.end();) {
    if (iter->first.document().frame() == frame)
      login_to_password_info_.erase(iter++);
    else
      ++iter;
  }
  for (FrameToPasswordFormMap::iterator iter =
           provisionally_saved_forms_.begin();
       iter != provisionally_saved_forms_.end();) {
    if (iter->first == frame)
      provisionally_saved_forms_.erase(iter++);
    else
      ++iter;
  }
}

bool PasswordAutofillAgent::FindLoginInfo(const blink::WebNode& node,
                                          blink::WebInputElement* found_input,
                                          PasswordInfo* found_password) {
  if (!node.isElementNode())
    return false;

  blink::WebElement element = node.toConst<blink::WebElement>();
  if (!element.hasHTMLTagName("input"))
    return false;

  blink::WebInputElement input = element.to<blink::WebInputElement>();
  LoginToPasswordInfoMap::iterator iter = login_to_password_info_.find(input);
  if (iter == login_to_password_info_.end())
    return false;

  *found_input = input;
  *found_password = iter->second;
  return true;
}

void PasswordAutofillAgent::ClearPreview(
    blink::WebInputElement* username,
    blink::WebInputElement* password) {
  if (!username->suggestedValue().isEmpty()) {
    username->setSuggestedValue(blink::WebString());
    username->setAutofilled(was_username_autofilled_);
    username->setSelectionRange(username_selection_start_,
                                username->value().length());
  }
  if (!password->suggestedValue().isEmpty()) {
      password->setSuggestedValue(blink::WebString());
      password->setAutofilled(was_password_autofilled_);
  }
}

}  // namespace autofill
