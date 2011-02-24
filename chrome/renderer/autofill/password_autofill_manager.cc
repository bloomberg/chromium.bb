// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/autofill/password_autofill_manager.h"

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "webkit/glue/form_field.h"
#include "webkit/glue/password_form.h"
#include "webkit/glue/password_form_dom_manager.h"

namespace {

// The size above which we stop triggering autocomplete.
static const size_t kMaximumTextSizeForAutocomplete = 1000;

// Maps element names to the actual elements to simplify form filling.
typedef std::map<string16, WebKit::WebInputElement>
    FormInputElementMap;

// Utility struct for form lookup and autofill. When we parse the DOM to look up
// a form, in addition to action and origin URL's we have to compare all
// necessary form elements. To avoid having to look these up again when we want
// to fill the form, the FindFormElements function stores the pointers
// in a FormElements* result, referenced to ensure they are safe to use.
struct FormElements {
  WebKit::WebFormElement form_element;
  FormInputElementMap input_elements;
};

typedef std::vector<FormElements*> FormElementsList;

// Helper to search the given form element for the specified input elements
// in |data|, and add results to |result|.
static bool FindFormInputElements(WebKit::WebFormElement* fe,
                                  const webkit_glue::FormData& data,
                                  FormElements* result) {
  // Loop through the list of elements we need to find on the form in order to
  // autofill it. If we don't find any one of them, abort processing this
  // form; it can't be the right one.
  for (size_t j = 0; j < data.fields.size(); j++) {
    WebKit::WebVector<WebKit::WebNode> temp_elements;
    fe->getNamedElements(data.fields[j].name(), temp_elements);

    // Match the first input element, if any.
    // |getNamedElements| may return non-input elements where the names match,
    // so the results are filtered for input elements.
    // If more than one match is made, then we have ambiguity (due to misuse
    // of "name" attribute) so is it considered not found.
    bool found_input = false;
    for (size_t i = 0; i < temp_elements.size(); ++i) {
      if (temp_elements[i].to<WebKit::WebElement>().hasTagName("input")) {
        // Check for a non-unique match.
        if (found_input) {
          found_input = false;
          break;
        }

        // This element matched, add it to our temporary result. It's possible
        // there are multiple matches, but for purposes of identifying the form
        // one suffices and if some function needs to deal with multiple
        // matching elements it can get at them through the FormElement*.
        // Note: This assignment adds a reference to the InputElement.
        result->input_elements[data.fields[j].name()] =
            temp_elements[i].to<WebKit::WebInputElement>();
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
void FindFormElements(WebKit::WebView* view,
                      const webkit_glue::FormData& data,
                      FormElementsList* results) {
  DCHECK(view);
  DCHECK(results);
  WebKit::WebFrame* main_frame = view->mainFrame();
  if (!main_frame)
    return;

  GURL::Replacements rep;
  rep.ClearQuery();
  rep.ClearRef();

  // Loop through each frame.
  for (WebKit::WebFrame* f = main_frame; f; f = f->traverseNext(false)) {
    WebKit::WebDocument doc = f->document();
    if (!doc.isHTMLDocument())
      continue;

    GURL full_origin(f->url());
    if (data.origin != full_origin.ReplaceComponents(rep))
      continue;

    WebKit::WebVector<WebKit::WebFormElement> forms;
    f->forms(forms);

    for (size_t i = 0; i < forms.size(); ++i) {
      WebKit::WebFormElement fe = forms[i];
      // Action URL must match.
      GURL full_action(f->document().completeURL(fe.action()));
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

bool FillForm(FormElements* fe, const webkit_glue::FormData& data) {
  if (!fe->form_element.autoComplete())
    return false;

  std::map<string16, string16> data_map;
  for (size_t i = 0; i < data.fields.size(); i++)
    data_map[data.fields[i].name()] = data.fields[i].value();

  for (FormInputElementMap::iterator it = fe->input_elements.begin();
       it != fe->input_elements.end(); ++it) {
    WebKit::WebInputElement element = it->second;
    // Don't fill a form that has pre-filled values.
    if (!element.value().isEmpty())
      return false;
  }

  for (FormInputElementMap::iterator it = fe->input_elements.begin();
       it != fe->input_elements.end(); ++it) {
    WebKit::WebInputElement element = it->second;
    DCHECK(element.value().isEmpty());
    if (element.isPasswordField() &&
        (!element.isEnabledFormControl() || element.hasAttribute("readonly"))) {
      continue;  // Don't fill uneditable password fields.
    }
    // TODO(tkent): Check maxlength and pattern.
    element.setValue(data_map[it->first]);
    element.setAutofilled(true);
    element.dispatchFormControlChangeEvent();
  }

  return false;
}

bool IsElementEditable(const WebKit::WebInputElement& element) {
  return element.isEnabledFormControl() && !element.hasAttribute("readonly");
}

void SetElementAutofilled(WebKit::WebInputElement* element, bool autofilled) {
  if (element->isAutofilled() == autofilled)
    return;
  element->setAutofilled(autofilled);
  // Notify any changeEvent listeners.
  element->dispatchFormControlChangeEvent();
}

bool DoUsernamesMatch(const string16& username1,
                      const string16& username2,
                      bool exact_match) {
  if (exact_match)
    return username1 == username2;
  return StartsWith(username1, username2, true);
}

}  // namespace

namespace autofill {

////////////////////////////////////////////////////////////////////////////////
// PasswordAutoFillManager, public:

PasswordAutoFillManager::PasswordAutoFillManager(
    RenderView* render_view)
    : RenderViewObserver(render_view),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
}

PasswordAutoFillManager::~PasswordAutoFillManager() {
}

bool PasswordAutoFillManager::TextFieldDidEndEditing(
    const WebKit::WebInputElement& element) {
  LoginToPasswordInfoMap::const_iterator iter =
      login_to_password_info_.find(element);
  if (iter == login_to_password_info_.end())
    return false;

  const webkit_glue::PasswordFormFillData& fill_data =
      iter->second.fill_data;

  // If wait_for_username is false, we should have filled when the text changed.
  if (!fill_data.wait_for_username)
    return false;

  WebKit::WebInputElement password = iter->second.password_field;
  if (!IsElementEditable(password))
    return false;

  WebKit::WebInputElement username = element;  // We need a non-const.

  // Do not set selection when ending an editing session, otherwise it can
  // mess with focus.
  FillUserNameAndPassword(&username, &password, fill_data, true, false);
  return true;
}

bool PasswordAutoFillManager::TextDidChangeInTextField(
    const WebKit::WebInputElement& element) {
  LoginToPasswordInfoMap::const_iterator iter =
      login_to_password_info_.find(element);
  if (iter == login_to_password_info_.end())
    return false;

  // The input text is being changed, so any autofilled password is now
  // outdated.
  WebKit::WebInputElement username = element;  // We need a non-const.
  WebKit::WebInputElement password = iter->second.password_field;
  SetElementAutofilled(&username, false);
  if (password.isAutofilled()) {
    password.setValue(string16());
    SetElementAutofilled(&password, false);
  }

  // If wait_for_username is true we will fill when the username loses focus.
  if (iter->second.fill_data.wait_for_username)
    return false;

  if (!element.isEnabledFormControl() ||
      !element.isText() ||
      !element.autoComplete() || element.isReadOnly()) {
    return false;
  }

  // Don't inline autocomplete if the user is deleting, that would be confusing.
  // But refresh the popup.  Note, since this is ours, return true to signal
  // no further processing is required.
  if (iter->second.backspace_pressed_last) {
    ShowSuggestionPopup(iter->second.fill_data, username);
    return true;
  }

  WebKit::WebString name = element.nameForAutofill();
  if (name.isEmpty())
    return false;  // If the field has no name, then we won't have values.

  // Don't attempt to autofill with values that are too large.
  if (element.value().length() > kMaximumTextSizeForAutocomplete)
    return false;

  // We post a task for doing the autocomplete as the caret position is not set
  // properly at this point (http://bugs.webkit.org/show_bug.cgi?id=16976) and
  // we need it to determine whether or not to trigger autocomplete.
  MessageLoop::current()->PostTask(FROM_HERE, method_factory_.NewRunnableMethod(
      &PasswordAutoFillManager::PerformInlineAutocomplete,
      element, password, iter->second.fill_data));
  return true;
}

bool PasswordAutoFillManager::TextFieldHandlingKeyDown(
    const WebKit::WebInputElement& element,
    const WebKit::WebKeyboardEvent& event) {
  LoginToPasswordInfoMap::iterator iter = login_to_password_info_.find(element);
  if (iter == login_to_password_info_.end())
    return false;

  int win_key_code = event.windowsKeyCode;
  iter->second.backspace_pressed_last =
      (win_key_code == ui::VKEY_BACK || win_key_code == ui::VKEY_DELETE);
  return true;
}

bool PasswordAutoFillManager::DidSelectAutoFillSuggestion(
    const WebKit::WebNode& node,
    const WebKit::WebString& value) {
  if (!node.isElementNode())
    return false;

  WebKit::WebElement element(static_cast<const WebKit::WebElement&>(node));
  if (!element.hasTagName("input"))
    return false;

  WebKit::WebInputElement user_input = element.to<WebKit::WebInputElement>();
  LoginToPasswordInfoMap::iterator iter =
      login_to_password_info_.find(user_input);
  if (iter == login_to_password_info_.end())
    return false;

  // Set the incoming |value| in the text field and |FillUserNameAndPassword|
  // will do the rest.
  user_input.setValue(value);
  const webkit_glue::PasswordFormFillData& fill_data = iter->second.fill_data;
  WebKit::WebInputElement password = iter->second.password_field;
  return FillUserNameAndPassword(&user_input, &password, fill_data, true, true);
}

void PasswordAutoFillManager::SendPasswordForms(WebKit::WebFrame* frame,
                                                bool only_visible) {
  // Make sure that this security origin is allowed to use password manager.
  WebKit::WebSecurityOrigin security_origin = frame->securityOrigin();
  if (!security_origin.canAccessPasswordManager())
    return;

  WebKit::WebVector<WebKit::WebFormElement> forms;
  frame->forms(forms);

  std::vector<webkit_glue::PasswordForm> password_forms;
  for (size_t i = 0; i < forms.size(); ++i) {
    const WebKit::WebFormElement& form = forms[i];

    // Respect autocomplete=off.
    if (!form.autoComplete())
      continue;
    if (only_visible && !form.hasNonEmptyBoundingBox())
      continue;
    scoped_ptr<webkit_glue::PasswordForm> password_form(
        webkit_glue::PasswordFormDomManager::CreatePasswordForm(form));
    if (password_form.get())
      password_forms.push_back(*password_form);
  }

  if (password_forms.empty())
    return;

  if (only_visible) {
    Send(new AutoFillHostMsg_PasswordFormsVisible(
        routing_id(), password_forms));
  } else {
    Send(new AutoFillHostMsg_PasswordFormsFound(routing_id(), password_forms));
  }
}

bool PasswordAutoFillManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PasswordAutoFillManager, message)
    IPC_MESSAGE_HANDLER(AutoFillMsg_FillPasswordForm, OnFillPasswordForm)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PasswordAutoFillManager::DidFinishDocumentLoad(WebKit::WebFrame* frame) {
  SendPasswordForms(frame, false);
}

void PasswordAutoFillManager::DidFinishLoad(WebKit::WebFrame* frame) {
  SendPasswordForms(frame, true);
}

void PasswordAutoFillManager::FrameDetached(WebKit::WebFrame* frame) {
  FrameClosing(frame);
}

void PasswordAutoFillManager::FrameWillClose(WebKit::WebFrame* frame) {
  FrameClosing(frame);
}


////////////////////////////////////////////////////////////////////////////////
// PageClickListener implementation:

bool PasswordAutoFillManager::InputElementClicked(
    const WebKit::WebInputElement& element,
    bool was_focused,
    bool is_focused) {
  // TODO(jcivelli): http://crbug.com/51644 Implement behavior.
  return false;
}

void PasswordAutoFillManager::OnFillPasswordForm(
    const webkit_glue::PasswordFormFillData& form_data) {
  FormElementsList forms;
  // We own the FormElements* in forms.
  FindFormElements(render_view()->webview(), form_data.basic_data, &forms);
  FormElementsList::iterator iter;
  for (iter = forms.begin(); iter != forms.end(); ++iter) {
    scoped_ptr<FormElements> form_elements(*iter);

    // If wait_for_username is true, we don't want to initially fill the form
    // until the user types in a valid username.
    if (!form_data.wait_for_username)
      FillForm(form_elements.get(), form_data.basic_data);

    // Attach autocomplete listener to enable selecting alternate logins.
    // First, get pointers to username element.
    WebKit::WebInputElement username_element =
        form_elements->input_elements[form_data.basic_data.fields[0].name()];

    // Get pointer to password element. (We currently only support single
    // password forms).
    WebKit::WebInputElement password_element =
        form_elements->input_elements[form_data.basic_data.fields[1].name()];

    DCHECK(login_to_password_info_.find(username_element) ==
        login_to_password_info_.end());
    PasswordInfo password_info;
    password_info.fill_data = form_data;
    password_info.password_field = password_element;
    login_to_password_info_[username_element] = password_info;
  }
}

////////////////////////////////////////////////////////////////////////////////
// PasswordAutoFillManager, private:

void PasswordAutoFillManager::GetSuggestions(
    const webkit_glue::PasswordFormFillData& fill_data,
    const string16& input,
    std::vector<string16>* suggestions) {
  if (StartsWith(fill_data.basic_data.fields[0].value(), input, false))
    suggestions->push_back(fill_data.basic_data.fields[0].value());

  webkit_glue::PasswordFormFillData::LoginCollection::const_iterator iter;
  for (iter = fill_data.additional_logins.begin();
       iter != fill_data.additional_logins.end(); ++iter) {
    if (StartsWith(iter->first, input, false))
      suggestions->push_back(iter->first);
  }
}

bool PasswordAutoFillManager::ShowSuggestionPopup(
    const webkit_glue::PasswordFormFillData& fill_data,
    const WebKit::WebInputElement& user_input) {
  WebKit::WebView* webview = user_input.document().frame()->view();
  if (!webview)
    return false;

  std::vector<string16> suggestions;
  GetSuggestions(fill_data, user_input.value(), &suggestions);
  if (suggestions.empty()) {
    webview->hidePopups();
    return false;
  }

  std::vector<string16> labels(suggestions.size());
  std::vector<string16> icons(suggestions.size());
  std::vector<int> ids(suggestions.size(), 0);
  webview->applyAutoFillSuggestions(user_input, suggestions, labels, icons, ids,
                                    -1);
  return true;
}

bool PasswordAutoFillManager::FillUserNameAndPassword(
    WebKit::WebInputElement* username_element,
    WebKit::WebInputElement* password_element,
    const webkit_glue::PasswordFormFillData& fill_data,
    bool exact_username_match,
    bool set_selection) {
  string16 current_username = username_element->value();
  // username and password will contain the match found if any.
  string16 username;
  string16 password;

  // Look for any suitable matches to current field text.
  if (DoUsernamesMatch(fill_data.basic_data.fields[0].value(), current_username,
                       exact_username_match)) {
    username = fill_data.basic_data.fields[0].value();
    password = fill_data.basic_data.fields[1].value();
  } else {
    // Scan additional logins for a match.
    webkit_glue::PasswordFormFillData::LoginCollection::const_iterator iter;
    for (iter = fill_data.additional_logins.begin();
         iter != fill_data.additional_logins.end(); ++iter) {
      if (DoUsernamesMatch(iter->first, current_username,
                           exact_username_match)) {
        username = iter->first;
        password = iter->second;
        break;
      }
    }
  }
  if (password.empty())
    return false;  // No match was found.

  // Input matches the username, fill in required values.
  username_element->setValue(username);

  if (set_selection) {
    username_element->setSelectionRange(current_username.length(),
                                        username.length());
  }

  SetElementAutofilled(username_element, true);
  if (IsElementEditable(*password_element))
    password_element->setValue(password);
  SetElementAutofilled(password_element, true);
  return true;
}

void PasswordAutoFillManager::PerformInlineAutocomplete(
    const WebKit::WebInputElement& username_input,
    const WebKit::WebInputElement& password_input,
    const webkit_glue::PasswordFormFillData& fill_data) {
  DCHECK(!fill_data.wait_for_username);

  // We need non-const versions of the username and password inputs.
  WebKit::WebInputElement username = username_input;
  WebKit::WebInputElement password = password_input;

  // Don't inline autocomplete if the caret is not at the end.
  // TODO(jcivelli): is there a better way to test the caret location?
  if (username.selectionStart() != username.selectionEnd() ||
      username.selectionEnd() != static_cast<int>(username.value().length())) {
    return;
  }

  // Show the popup with the list of available usernames.
  ShowSuggestionPopup(fill_data, username);

  // Fill the user and password field with the most relevant match.
  FillUserNameAndPassword(&username, &password, fill_data, false, true);
}

void PasswordAutoFillManager::FrameClosing(const WebKit::WebFrame* frame) {
  for (LoginToPasswordInfoMap::iterator iter = login_to_password_info_.begin();
       iter != login_to_password_info_.end();) {
    if (iter->first.document().frame() == frame)
      login_to_password_info_.erase(iter++);
    else
      ++iter;
  }
}

}  // namespace autofill
