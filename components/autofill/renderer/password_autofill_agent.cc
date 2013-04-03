// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/renderer/password_autofill_agent.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "components/autofill/common/autofill_messages.h"
#include "components/autofill/common/form_field_data.h"
#include "components/autofill/common/password_form_fill_data.h"
#include "components/autofill/renderer/form_autofill_util.h"
#include "content/public/common/password_form.h"
#include "content/public/renderer/password_form_conversion_utils.h"
#include "content/public/renderer/render_view.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAutofillClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/keycodes/keyboard_codes.h"

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
                                  const FormData& data,
                                  FormElements* result) {
  // Loop through the list of elements we need to find on the form in order to
  // autofill it. If we don't find any one of them, abort processing this
  // form; it can't be the right one.
  for (size_t j = 0; j < data.fields.size(); j++) {
    WebKit::WebVector<WebKit::WebNode> temp_elements;
    fe->getNamedElements(data.fields[j].name, temp_elements);

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
        result->input_elements[data.fields[j].name] =
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
                      const FormData& data,
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

    GURL full_origin(doc.url());
    if (data.origin != full_origin.ReplaceComponents(rep))
      continue;

    WebKit::WebVector<WebKit::WebFormElement> forms;
    doc.forms(forms);

    for (size_t i = 0; i < forms.size(); ++i) {
      WebKit::WebFormElement fe = forms[i];

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

bool IsElementEditable(const WebKit::WebInputElement& element) {
  return element.isEnabled() && !element.isReadOnly();
}

void FillForm(FormElements* fe, const FormData& data) {
  if (!fe->form_element.autoComplete())
    return;

  std::map<string16, string16> data_map;
  for (size_t i = 0; i < data.fields.size(); i++)
    data_map[data.fields[i].name] = data.fields[i].value;

  for (FormInputElementMap::iterator it = fe->input_elements.begin();
       it != fe->input_elements.end(); ++it) {
    WebKit::WebInputElement element = it->second;
    // Don't fill a form that has pre-filled values distinct from the ones we
    // want to fill with.
    if (!element.value().isEmpty() && element.value() != data_map[it->first])
      return;
  }

  for (FormInputElementMap::iterator it = fe->input_elements.begin();
       it != fe->input_elements.end(); ++it) {
    WebKit::WebInputElement element = it->second;
    if (!IsElementEditable(element))
      continue;  // Don't fill uneditable fields.

    // TODO(tkent): Check maxlength and pattern.
    element.setValue(data_map[it->first]);
    element.setAutofilled(true);
    element.dispatchFormControlChangeEvent();
  }
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
// PasswordAutofillAgent, public:

PasswordAutofillAgent::PasswordAutofillAgent(content::RenderView* render_view)
    : content::RenderViewObserver(render_view),
      disable_popup_(false),
      web_view_(render_view->GetWebView()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

PasswordAutofillAgent::~PasswordAutofillAgent() {
}

bool PasswordAutofillAgent::TextFieldDidEndEditing(
    const WebKit::WebInputElement& element) {
  LoginToPasswordInfoMap::const_iterator iter =
      login_to_password_info_.find(element);
  if (iter == login_to_password_info_.end())
    return false;

  const PasswordFormFillData& fill_data =
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

bool PasswordAutofillAgent::TextDidChangeInTextField(
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

  if (!IsElementEditable(element) ||
      !element.isText() ||
      !element.autoComplete()) {
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

  // The caret position should have already been updated.
  PerformInlineAutocomplete(element, password, iter->second.fill_data);
  return true;
}

bool PasswordAutofillAgent::TextFieldHandlingKeyDown(
    const WebKit::WebInputElement& element,
    const WebKit::WebKeyboardEvent& event) {
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

bool PasswordAutofillAgent::DidAcceptAutofillSuggestion(
    const WebKit::WebNode& node,
    const WebKit::WebString& value) {
  WebKit::WebInputElement input;
  PasswordInfo password;
  if (!FindLoginInfo(node, &input, &password))
    return false;

  // Set the incoming |value| in the text field and |FillUserNameAndPassword|
  // will do the rest.
  input.setValue(value, true);
  return FillUserNameAndPassword(&input, &password.password_field,
                                 password.fill_data, true, true);
}

bool PasswordAutofillAgent::DidSelectAutofillSuggestion(
    const WebKit::WebNode& node) {
  WebKit::WebInputElement input;
  PasswordInfo password;
  return FindLoginInfo(node, &input, &password);
}

bool PasswordAutofillAgent::DidClearAutofillSelection(
    const WebKit::WebNode& node) {
  WebKit::WebInputElement input;
  PasswordInfo password;
  return FindLoginInfo(node, &input, &password);
}

void PasswordAutofillAgent::SendPasswordForms(WebKit::WebFrame* frame,
                                              bool only_visible) {
  // Make sure that this security origin is allowed to use password manager.
  WebKit::WebSecurityOrigin origin = frame->document().securityOrigin();
  if (!origin.canAccessPasswordManager())
    return;

  WebKit::WebVector<WebKit::WebFormElement> forms;
  frame->document().forms(forms);

  std::vector<content::PasswordForm> password_forms;
  for (size_t i = 0; i < forms.size(); ++i) {
    const WebKit::WebFormElement& form = forms[i];

    // If requested, ignore non-rendered forms, e.g. those styled with
    // display:none.
    if (only_visible && !form.hasNonEmptyBoundingBox())
      continue;

    scoped_ptr<content::PasswordForm> password_form(
        content::CreatePasswordForm(form));
    if (password_form.get())
      password_forms.push_back(*password_form);
  }

  if (password_forms.empty() && !only_visible) {
    // We need to send the PasswordFormsRendered message regardless of whether
    // there are any forms visible, as this is also the code path that triggers
    // showing the infobar.
    return;
  }

  if (only_visible) {
    Send(new AutofillHostMsg_PasswordFormsRendered(
        routing_id(), password_forms));
  } else {
    Send(new AutofillHostMsg_PasswordFormsParsed(routing_id(), password_forms));
  }
}

bool PasswordAutofillAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PasswordAutofillAgent, message)
    IPC_MESSAGE_HANDLER(AutofillMsg_FillPasswordForm, OnFillPasswordForm)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PasswordAutofillAgent::DidFinishDocumentLoad(WebKit::WebFrame* frame) {
  // The |frame| contents have been parsed, but not yet rendered.  Let the
  // PasswordManager know that forms are loaded, even though we can't yet tell
  // whether they're visible.
  SendPasswordForms(frame, false);
}

void PasswordAutofillAgent::DidFinishLoad(WebKit::WebFrame* frame) {
  // The |frame| contents have been rendered.  Let the PasswordManager know
  // which of the loaded frames are actually visible to the user.  This also
  // triggers the "Save password?" infobar if the user just submitted a password
  // form.
  SendPasswordForms(frame, true);
}

void PasswordAutofillAgent::FrameDetached(WebKit::WebFrame* frame) {
  FrameClosing(frame);
}

void PasswordAutofillAgent::FrameWillClose(WebKit::WebFrame* frame) {
  FrameClosing(frame);
}


////////////////////////////////////////////////////////////////////////////////
// PageClickListener implementation:

bool PasswordAutofillAgent::InputElementClicked(
    const WebKit::WebInputElement& element,
    bool was_focused,
    bool is_focused) {
  // TODO(jcivelli): http://crbug.com/51644 Implement behavior.
  return false;
}

bool PasswordAutofillAgent::InputElementLostFocus() {
  return false;
}

void PasswordAutofillAgent::OnFillPasswordForm(
    const PasswordFormFillData& form_data,
    bool disable_popup) {
  disable_popup_ = disable_popup;

  FormElementsList forms;
  // We own the FormElements* in forms.
  FindFormElements(render_view()->GetWebView(), form_data.basic_data, &forms);
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
        form_elements->input_elements[form_data.basic_data.fields[0].name];

    // Get pointer to password element. (We currently only support single
    // password forms).
    WebKit::WebInputElement password_element =
        form_elements->input_elements[form_data.basic_data.fields[1].name];

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
    FindFormAndFieldForInputElement(
        username_element, &form, &field, REQUIRE_NONE);
    Send(new AutofillHostMsg_AddPasswordFormMapping(
        routing_id(),
        field,
        form_data));
  }
}

////////////////////////////////////////////////////////////////////////////////
// PasswordAutofillAgent, private:

void PasswordAutofillAgent::GetSuggestions(
    const PasswordFormFillData& fill_data,
    const string16& input,
    std::vector<string16>* suggestions) {
  if (StartsWith(fill_data.basic_data.fields[0].value, input, false))
    suggestions->push_back(fill_data.basic_data.fields[0].value);

  PasswordFormFillData::LoginCollection::const_iterator iter;
  for (iter = fill_data.additional_logins.begin();
       iter != fill_data.additional_logins.end(); ++iter) {
    if (StartsWith(iter->first, input, false))
      suggestions->push_back(iter->first);
  }
}

bool PasswordAutofillAgent::ShowSuggestionPopup(
    const PasswordFormFillData& fill_data,
    const WebKit::WebInputElement& user_input) {
  WebKit::WebFrame* frame = user_input.document().frame();
  if (!frame)
    return false;

  WebKit::WebView* webview = frame->view();
  if (!webview)
    return false;

  std::vector<string16> suggestions;
  GetSuggestions(fill_data, user_input.value(), &suggestions);

  if (disable_popup_) {
    FormData form;
    FormFieldData field;
    FindFormAndFieldForInputElement(
        user_input, &form, &field, REQUIRE_NONE);

    WebKit::WebInputElement selected_element = user_input;
    gfx::Rect bounding_box(selected_element.boundsInViewportSpace());

    float scale = web_view_->pageScaleFactor();
    gfx::RectF bounding_box_scaled(bounding_box.x() * scale,
                                   bounding_box.y() * scale,
                                   bounding_box.width() * scale,
                                   bounding_box.height() * scale);
    Send(new AutofillHostMsg_ShowPasswordSuggestions(routing_id(),
                                                     field,
                                                     bounding_box_scaled,
                                                     suggestions));
    return !suggestions.empty();
  }


  if (suggestions.empty()) {
    webview->hidePopups();
    return false;
  }

  std::vector<string16> labels(suggestions.size());
  std::vector<string16> icons(suggestions.size());
  std::vector<int> ids(suggestions.size(),
                       WebKit::WebAutofillClient::MenuItemIDPasswordEntry);
  webview->applyAutofillSuggestions(
      user_input, suggestions, labels, icons, ids);
  return true;
}

bool PasswordAutofillAgent::FillUserNameAndPassword(
    WebKit::WebInputElement* username_element,
    WebKit::WebInputElement* password_element,
    const PasswordFormFillData& fill_data,
    bool exact_username_match,
    bool set_selection) {
  string16 current_username = username_element->value();
  // username and password will contain the match found if any.
  string16 username;
  string16 password;

  // Look for any suitable matches to current field text.
  if (DoUsernamesMatch(fill_data.basic_data.fields[0].value, current_username,
                       exact_username_match)) {
    username = fill_data.basic_data.fields[0].value;
    password = fill_data.basic_data.fields[1].value;
  } else {
    // Scan additional logins for a match.
    PasswordFormFillData::LoginCollection::const_iterator iter;
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

void PasswordAutofillAgent::PerformInlineAutocomplete(
    const WebKit::WebInputElement& username_input,
    const WebKit::WebInputElement& password_input,
    const PasswordFormFillData& fill_data) {
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


#if !defined(OS_ANDROID)
  // Fill the user and password field with the most relevant match. Android
  // only fills in the fields after the user clicks on the suggestion popup.
  FillUserNameAndPassword(&username, &password, fill_data, false, true);
#endif
}

void PasswordAutofillAgent::FrameClosing(const WebKit::WebFrame* frame) {
  for (LoginToPasswordInfoMap::iterator iter = login_to_password_info_.begin();
       iter != login_to_password_info_.end();) {
    if (iter->first.document().frame() == frame)
      login_to_password_info_.erase(iter++);
    else
      ++iter;
  }
}

bool PasswordAutofillAgent::FindLoginInfo(const WebKit::WebNode& node,
                                          WebKit::WebInputElement* found_input,
                                          PasswordInfo* found_password) {
  if (!node.isElementNode())
    return false;

  WebKit::WebElement element = node.toConst<WebKit::WebElement>();
  if (!element.hasTagName("input"))
    return false;

  WebKit::WebInputElement input = element.to<WebKit::WebInputElement>();
  LoginToPasswordInfoMap::iterator iter = login_to_password_info_.find(input);
  if (iter == login_to_password_info_.end())
    return false;

  *found_input = input;
  *found_password = iter->second;
  return true;
}

}  // namespace autofill
