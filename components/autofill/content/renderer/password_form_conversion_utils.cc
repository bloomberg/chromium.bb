// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_form_conversion_utils.h"

#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/core/common/password_form.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"

using blink::WebDocument;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebString;
using blink::WebVector;

namespace autofill {
namespace {

// Maximum number of password fields we will observe before throwing our
// hands in the air and giving up with a given form.
static const size_t kMaxPasswords = 3;

// Helper to determine which password is the main one, and which is
// an old password (e.g on a "make new password" form), if any.
bool LocateSpecificPasswords(std::vector<WebInputElement> passwords,
                             WebInputElement* password,
                             WebInputElement* old_password) {
  switch (passwords.size()) {
    case 1:
      // Single password, easy.
      *password = passwords[0];
      break;
    case 2:
      if (passwords[0].value() == passwords[1].value()) {
        // Treat two identical passwords as a single password.
        *password = passwords[0];
      } else {
        // Assume first is old password, second is new (no choice but to guess).
        *old_password = passwords[0];
        *password = passwords[1];
      }
      break;
    case 3:
      if (passwords[0].value() == passwords[1].value() &&
          passwords[0].value() == passwords[2].value()) {
        // All three passwords the same? Just treat as one and hope.
        *password = passwords[0];
      } else if (passwords[0].value() == passwords[1].value()) {
        // Two the same and one different -> old password is duplicated one.
        *old_password = passwords[0];
        *password = passwords[2];
      } else if (passwords[1].value() == passwords[2].value()) {
        *old_password = passwords[0];
        *password = passwords[1];
      } else {
        // Three different passwords, or first and last match with middle
        // different. No idea which is which, so no luck.
        return false;
      }
      break;
    default:
      return false;
  }
  return true;
}

// Get information about a login form that encapsulated in the
// PasswordForm struct.
void GetPasswordForm(const WebFormElement& form, PasswordForm* password_form) {
  WebInputElement latest_input_element;
  std::vector<WebInputElement> passwords;
  std::vector<base::string16> other_possible_usernames;

  WebVector<WebFormControlElement> control_elements;
  form.getFormControlElements(control_elements);

  for (size_t i = 0; i < control_elements.size(); ++i) {
    WebFormControlElement control_element = control_elements[i];
    if (control_element.isActivatedSubmit())
      password_form->submit_element = control_element.formControlName();

    WebInputElement* input_element = toWebInputElement(&control_element);
    if (!input_element || !input_element->isEnabled())
      continue;

    if ((passwords.size() < kMaxPasswords) &&
        input_element->isPasswordField()) {
      // We assume that the username element is the input element before the
      // first password element.
      if (passwords.empty() && !latest_input_element.isNull()) {
        password_form->username_element =
            latest_input_element.nameForAutofill();
        password_form->username_value = latest_input_element.value();
        // Remove the selected username from other_possible_usernames.
        if (!other_possible_usernames.empty() &&
            !latest_input_element.value().isEmpty())
          other_possible_usernames.resize(other_possible_usernames.size() - 1);
      }
      passwords.push_back(*input_element);
    }

    // Various input types such as text, url, email can be a username field.
    if (input_element->isTextField() && !input_element->isPasswordField()) {
      latest_input_element = *input_element;
      // We ignore elements that have no value. Unlike username_element,
      // other_possible_usernames is used only for autofill, not for form
      // identification, and blank autofill entries are not useful.
      if (!input_element->value().isEmpty())
        other_possible_usernames.push_back(input_element->value());
    }
  }

  // Get the document URL
  GURL full_origin(form.document().url());

  // Calculate the canonical action URL
  WebString action = form.action();
  if (action.isNull())
    action = WebString(""); // missing 'action' attribute implies current URL
  GURL full_action(form.document().completeURL(action));
  if (!full_action.is_valid())
    return;

  WebInputElement password;
  WebInputElement old_password;
  if (!LocateSpecificPasswords(passwords, &password, &old_password))
    return;

  // We want to keep the path but strip any authentication data, as well as
  // query and ref portions of URL, for the form action and form origin.
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  password_form->action = full_action.ReplaceComponents(rep);
  password_form->origin = full_origin.ReplaceComponents(rep);

  rep.SetPathStr("");
  password_form->signon_realm = full_origin.ReplaceComponents(rep).spec();

  password_form->other_possible_usernames.swap(other_possible_usernames);

  if (!password.isNull()) {
    password_form->password_element = password.nameForAutofill();
    password_form->password_value = password.value();
    password_form->password_autocomplete_set = password.autoComplete();
  }
  if (!old_password.isNull()) {
    password_form->old_password_element = old_password.nameForAutofill();
    password_form->old_password_value = old_password.value();
  }

  password_form->scheme = PasswordForm::SCHEME_HTML;
  password_form->ssl_valid = false;
  password_form->preferred = false;
  password_form->blacklisted_by_user = false;
  password_form->type = PasswordForm::TYPE_MANUAL;
  password_form->use_additional_authentication = false;
}

}  // namespace

scoped_ptr<PasswordForm> CreatePasswordForm(const WebFormElement& web_form) {
  if (web_form.isNull())
    return scoped_ptr<PasswordForm>();

  scoped_ptr<PasswordForm> password_form(new PasswordForm());
  GetPasswordForm(web_form, password_form.get());

  if (!password_form->action.is_valid())
    return scoped_ptr<PasswordForm>();

  WebFormElementToFormData(web_form,
                           blink::WebFormControlElement(),
                           REQUIRE_NONE,
                           EXTRACT_NONE,
                           &password_form->form_data,
                           NULL /* FormFieldData */);

  return password_form.Pass();
}

}  // namespace autofill
