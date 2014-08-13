// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_form_conversion_utils.h"

#include "base/strings/string_util.h"
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

// Checks in a case-insensitive way if the autocomplete attribute for the given
// |element| is present and has the specified |value_in_lowercase|.
bool HasAutocompleteAttributeValue(const WebInputElement& element,
                                   const char* value_in_lowercase) {
  return base::LowerCaseEqualsASCII(
      base::string16(element.getAttribute("autocomplete")),
      value_in_lowercase);
}

// Helper to determine which password is the main (current) one, and which is
// the new password (e.g., on a sign-up or change password form), if any.
bool LocateSpecificPasswords(std::vector<WebInputElement> passwords,
                             WebInputElement* current_password,
                             WebInputElement* new_password) {
  DCHECK(current_password && current_password->isNull());
  DCHECK(new_password && new_password->isNull());

  // First, look for elements marked with either autocomplete='current-password'
  // or 'new-password' -- if we find any, take the hint, and treat the first of
  // each kind as the element we are looking for.
  for (std::vector<WebInputElement>::const_iterator it = passwords.begin();
       it != passwords.end(); it++) {
    if (HasAutocompleteAttributeValue(*it, "current-password") &&
        current_password->isNull()) {
      *current_password = *it;
    } else if (HasAutocompleteAttributeValue(*it, "new-password") &&
        new_password->isNull()) {
      *new_password = *it;
    }
  }

  // If we have seen an element with either of autocomplete attributes above,
  // take that as a signal that the page author must have intentionally left the
  // rest of the password fields unmarked. Perhaps they are used for other
  // purposes, e.g., PINs, OTPs, and the like. So we skip all the heuristics we
  // normally do, and ignore the rest of the password fields.
  if (!current_password->isNull() || !new_password->isNull())
    return true;

  switch (passwords.size()) {
    case 1:
      // Single password, easy.
      *current_password = passwords[0];
      break;
    case 2:
      if (passwords[0].value() == passwords[1].value()) {
        // Two identical passwords: assume we are seeing a new password with a
        // confirmation. This can be either a sign-up form or a password change
        // form that does not ask for the old password.
        *new_password = passwords[0];
      } else {
        // Assume first is old password, second is new (no choice but to guess).
        *current_password = passwords[0];
        *new_password = passwords[1];
      }
      break;
    case 3:
      if (!passwords[0].value().isEmpty() &&
          passwords[0].value() == passwords[1].value() &&
          passwords[0].value() == passwords[2].value()) {
        // All three passwords are the same and non-empty? This does not make
        // any sense, give up.
        return false;
      } else if (passwords[1].value() == passwords[2].value()) {
        // New password is the duplicated one, and comes second; or empty form
        // with 3 password fields, in which case we will assume this layout.
        *current_password = passwords[0];
        *new_password = passwords[1];
      } else if (passwords[0].value() == passwords[1].value()) {
        // It is strange that the new password comes first, but trust more which
        // fields are duplicated than the ordering of fields.
        *current_password = passwords[2];
        *new_password = passwords[0];
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

// Get information about a login form encapsulated in a PasswordForm struct.
void GetPasswordForm(const WebFormElement& form, PasswordForm* password_form) {
  WebInputElement latest_input_element;
  WebInputElement username_element;
  // Caches whether |username_element| is marked with autocomplete='username'.
  // Needed for performance reasons to avoid recalculating this multiple times.
  bool has_seen_element_with_autocomplete_username_before = false;
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

    if (input_element->isPasswordField()) {
      passwords.push_back(*input_element);
      // If we have not yet considered any element to be the username so far,
      // provisionally select the input element just before the first password
      // element to be the username. This choice will be overruled if we later
      // find an element with autocomplete='username'.
      if (username_element.isNull() && !latest_input_element.isNull()) {
        username_element = latest_input_element;
        // Remove the selected username from other_possible_usernames.
        if (!latest_input_element.value().isEmpty()) {
          DCHECK(!other_possible_usernames.empty());
          DCHECK_EQ(base::string16(latest_input_element.value()),
                    other_possible_usernames.back());
          other_possible_usernames.pop_back();
        }
      }
    }

    // Various input types such as text, url, email can be a username field.
    if (input_element->isTextField() && !input_element->isPasswordField()) {
      if (HasAutocompleteAttributeValue(*input_element, "username")) {
        if (has_seen_element_with_autocomplete_username_before) {
          // A second or subsequent element marked with autocomplete='username'.
          // This makes us less confident that we have understood the form. We
          // will stick to our choice that the first such element was the real
          // username, but will start collecting other_possible_usernames from
          // the extra elements marked with autocomplete='username'. Note that
          // unlike username_element, other_possible_usernames is used only for
          // autofill, not for form identification, and blank autofill entries
          // are not useful, so we do not collect empty strings.
          if (!input_element->value().isEmpty())
            other_possible_usernames.push_back(input_element->value());
        } else {
          // The first element marked with autocomplete='username'. Take the
          // hint and treat it as the username (overruling the tentative choice
          // we might have made before). Furthermore, drop all other possible
          // usernames we have accrued so far: they come from fields not marked
          // with the autocomplete attribute, making them unlikely alternatives.
          username_element = *input_element;
          has_seen_element_with_autocomplete_username_before = true;
          other_possible_usernames.clear();
        }
      } else {
        if (has_seen_element_with_autocomplete_username_before) {
          // Having seen elements with autocomplete='username', elements without
          // this attribute are no longer interesting. No-op.
        } else {
          // No elements marked with autocomplete='username' so far whatsoever.
          // If we have not yet selected a username element even provisionally,
          // then remember this element for the case when the next field turns
          // out to be a password. Save a non-empty username as a possible
          // alternative, at least for now.
          if (username_element.isNull())
            latest_input_element = *input_element;
          if (!input_element->value().isEmpty())
            other_possible_usernames.push_back(input_element->value());
        }
      }
    }
  }

  if (!username_element.isNull()) {
    password_form->username_element = username_element.nameForAutofill();
    password_form->username_value = username_element.value();
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
  WebInputElement new_password;
  if (!LocateSpecificPasswords(passwords, &password, &new_password))
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
  if (!new_password.isNull()) {
    password_form->new_password_element = new_password.nameForAutofill();
    password_form->new_password_value = new_password.value();
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
