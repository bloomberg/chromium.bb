// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/renderer/password_form_conversion_utils.h"

#include <stddef.h>

#include <algorithm>
#include <string>

#include "base/i18n/case_conversion.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/content/renderer/form_autofill_util.h"
#include "components/autofill/content/renderer/html_based_username_detector.h"
#include "components/autofill/core/common/autofill_regex_constants.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/password_form.h"
#include "components/autofill/core/common/password_form_field_prediction_map.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "google_apis/gaia/gaia_urls.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFormControlElement.h"
#include "third_party/WebKit/public/web/WebInputElement.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/re2/src/re2/re2.h"

using blink::WebDocument;
using blink::WebFormControlElement;
using blink::WebFormElement;
using blink::WebInputElement;
using blink::WebLocalFrame;
using blink::WebString;

namespace autofill {
namespace {

// PasswordForms can be constructed for both WebFormElements and for collections
// of WebInputElements that are not in a WebFormElement. This intermediate
// aggregating structure is provided so GetPasswordForm() only has one
// view of the underlying data, regardless of its origin.
struct SyntheticForm {
  SyntheticForm();
  ~SyntheticForm();

  std::vector<blink::WebElement> fieldsets;
  // Contains control elements of the represented form, including not fillable
  // ones.
  std::vector<blink::WebFormControlElement> control_elements;
  blink::WebDocument document;
  blink::WebString action;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyntheticForm);
};

SyntheticForm::SyntheticForm() {}
SyntheticForm::~SyntheticForm() {}

// Layout classification of password forms
// A layout sequence of a form is the sequence of it's non-password and password
// input fields, represented by "N" and "P", respectively. A form like this
// <form>
//   <input type='text' ...>
//   <input type='hidden' ...>
//   <input type='password' ...>
//   <input type='submit' ...>
// </form>
// has the layout sequence "NP" -- "N" for the first field, and "P" for the
// third. The second and fourth fields are ignored, because they are not text
// fields.
//
// The code below classifies the layout (see PasswordForm::Layout) of a form
// based on its layout sequence. This is done by assigning layouts regular
// expressions over the alphabet {N, P}. LAYOUT_OTHER is implicitly the type
// corresponding to all layout sequences not matching any other layout.
//
// LAYOUT_LOGIN_AND_SIGNUP is classified by NPN+P.*. This corresponds to a form
// which starts with a login section (NP) and continues with a sign-up section
// (N+P.*). The aim is to distinguish such forms from change password-forms
// (N*PPP?.*) and forms which use password fields to store private but
// non-password data (could look like, e.g., PN+P.*).
const char kLoginAndSignupRegex[] =
    "NP"   // Login section.
    "N+P"  // Sign-up section.
    ".*";  // Anything beyond that.

const char kAutocompleteUsername[] = "username";
const char kAutocompleteCurrentPassword[] = "current-password";
const char kAutocompleteNewPassword[] = "new-password";
const char kAutocompleteCreditCardPrefix[] = "cc-";

struct LoginAndSignupLazyInstanceTraits
    : public base::internal::DestructorAtExitLazyInstanceTraits<re2::RE2> {
  static re2::RE2* New(void* instance) {
    return CreateMatcher(instance, kLoginAndSignupRegex);
  }
};

base::LazyInstance<re2::RE2, LoginAndSignupLazyInstanceTraits>
    g_login_and_signup_matcher = LAZY_INSTANCE_INITIALIZER;

// Given the sequence of non-password and password text input fields of a form,
// represented as a string of Ns (non-password) and Ps (password), computes the
// layout type of that form.
PasswordForm::Layout SequenceToLayout(base::StringPiece layout_sequence) {
  if (re2::RE2::FullMatch(
          re2::StringPiece(layout_sequence.data(),
                           base::checked_cast<int>(layout_sequence.size())),
          g_login_and_signup_matcher.Get())) {
    return PasswordForm::Layout::LAYOUT_LOGIN_AND_SIGNUP;
  }
  return PasswordForm::Layout::LAYOUT_OTHER;
}

void PopulateSyntheticFormFromWebForm(const WebFormElement& web_form,
                                      SyntheticForm* synthetic_form) {
  // TODO(vabr): The fact that we are actually passing all form fields, not just
  // autofillable ones (cause of http://crbug.com/537396, see also
  // http://crbug.com/543006) is not tested yet, due to difficulties to fake
  // test frame origin to match GAIA login page. Once this code gets moved to
  // browser, we need to add tests for this as well.
  blink::WebVector<blink::WebFormControlElement> web_control_elements;
  web_form.GetFormControlElements(web_control_elements);
  synthetic_form->control_elements.assign(web_control_elements.begin(),
                                          web_control_elements.end());
  synthetic_form->action = web_form.Action();
  synthetic_form->document = web_form.GetDocument();
}

// Helper to determine which password is the main (current) one, and which is
// the new password (e.g., on a sign-up or change password form), if any. If the
// new password is found and there is another password field with the same user
// input, the function also sets |confirmation_password| to this field.
void LocateSpecificPasswords(std::vector<WebInputElement> passwords,
                             WebInputElement* current_password,
                             WebInputElement* new_password,
                             WebInputElement* confirmation_password) {
  DCHECK(!passwords.empty());
  DCHECK(current_password && current_password->IsNull());
  DCHECK(new_password && new_password->IsNull());
  DCHECK(confirmation_password && confirmation_password->IsNull());

  // First, look for elements marked with either autocomplete='current-password'
  // or 'new-password' -- if we find any, take the hint, and treat the first of
  // each kind as the element we are looking for.
  for (const WebInputElement& it : passwords) {
    if (HasAutocompleteAttributeValue(it, kAutocompleteCurrentPassword) &&
        current_password->IsNull()) {
      *current_password = it;
    } else if (HasAutocompleteAttributeValue(it, kAutocompleteNewPassword) &&
               new_password->IsNull()) {
      *new_password = it;
    } else if (!new_password->IsNull() &&
               (new_password->Value() == it.Value())) {
      *confirmation_password = it;
    }
  }

  // If we have seen an element with either of autocomplete attributes above,
  // take that as a signal that the page author must have intentionally left the
  // rest of the password fields unmarked. Perhaps they are used for other
  // purposes, e.g., PINs, OTPs, and the like. So we skip all the heuristics we
  // normally do, and ignore the rest of the password fields.
  if (!current_password->IsNull() || !new_password->IsNull())
    return;

  switch (passwords.size()) {
    case 1:
      // Single password, easy.
      *current_password = passwords[0];
      break;
    case 2:
      if (!passwords[0].Value().IsEmpty() &&
          passwords[0].Value() == passwords[1].Value()) {
        // Two identical non-empty passwords: assume we are seeing a new
        // password with a confirmation. This can be either a sign-up form or a
        // password change form that does not ask for the old password.
        *new_password = passwords[0];
        *confirmation_password = passwords[1];
      } else {
        // Assume first is old password, second is new (no choice but to guess).
        // This case also includes empty passwords in order to allow filling of
        // password change forms (that also could autofill for sign up form, but
        // we can't do anything with this using only client side information).
        *current_password = passwords[0];
        *new_password = passwords[1];
      }
      break;
    default:
      if (!passwords[0].Value().IsEmpty() &&
          passwords[0].Value() == passwords[1].Value() &&
          passwords[0].Value() == passwords[2].Value()) {
        // All three passwords are the same and non-empty? It may be a change
        // password form where old and new passwords are the same. It doesn't
        // matter what field is correct, let's save the value.
        *current_password = passwords[0];
      } else if (passwords[1].Value() == passwords[2].Value()) {
        // New password is the duplicated one, and comes second; or empty form
        // with 3 password fields, in which case we will assume this layout.
        *current_password = passwords[0];
        *new_password = passwords[1];
        *confirmation_password = passwords[2];
      } else if (passwords[0].Value() == passwords[1].Value()) {
        // It is strange that the new password comes first, but trust more which
        // fields are duplicated than the ordering of fields. Assume that
        // any password fields after the new password contain sensitive
        // information that isn't actually a password (security hint, SSN, etc.)
        *new_password = passwords[0];
        *confirmation_password = passwords[1];
      } else {
        // Three different passwords, or first and last match with middle
        // different. No idea which is which. Let's save the first password.
        // Password selection in a prompt will allow to correct the choice.
        *current_password = passwords[0];
      }
  }
}

// Checks the |form_predictions| map to see if there is a key associated with
// the |prediction_type| value. Assigns the key to |prediction_field| and
// returns true if it is found.
bool MapContainsPrediction(
    const std::map<WebInputElement, PasswordFormFieldPredictionType>&
        form_predictions,
    PasswordFormFieldPredictionType prediction_type,
    WebInputElement* prediction_field) {
  for (auto it = form_predictions.begin(); it != form_predictions.end(); ++it) {
    if (it->second == prediction_type) {
      (*prediction_field) = it->first;
      return true;
    }
  }
  return false;
}

void FindPredictedElements(
    const SyntheticForm& form,
    const FormData& form_data,
    const FormsPredictionsMap& form_predictions,
    std::map<WebInputElement, PasswordFormFieldPredictionType>*
        predicted_elements) {
  // Matching only requires that action and name of the form match to allow
  // the username to be updated even if the form is changed after page load.
  // See https://crbug.com/476092 for more details.
  auto predictions_it = form_predictions.begin();
  for (; predictions_it != form_predictions.end(); ++predictions_it) {
    if (predictions_it->first.action == form_data.action &&
        predictions_it->first.name == form_data.name) {
      break;
    }
  }

  if (predictions_it == form_predictions.end())
    return;

  std::vector<blink::WebFormControlElement> autofillable_elements =
      form_util::ExtractAutofillableElementsFromSet(form.control_elements);
  const PasswordFormFieldPredictionMap& field_predictions =
      predictions_it->second;
  for (const auto& prediction : field_predictions) {
    const FormFieldData& target_field = prediction.first;
    const PasswordFormFieldPredictionType& type = prediction.second;
    for (const auto& control_element : autofillable_elements)  {
      if (control_element.NameForAutofill().Utf16() == target_field.name) {
        const WebInputElement* input_element =
            ToWebInputElement(&control_element);

        // TODO(sebsg): Investigate why this guard is necessary, see
        // https://crbug.com/517490 for more details.
        if (input_element) {
          (*predicted_elements)[*input_element] = type;
        }
        break;
      }
    }
  }
}

const char kPasswordSiteUrlRegex[] =
    "passwords(?:-[a-z-]+\\.corp)?\\.google\\.com";

struct PasswordSiteUrlLazyInstanceTraits
    : public base::internal::DestructorAtExitLazyInstanceTraits<re2::RE2> {
  static re2::RE2* New(void* instance) {
    return CreateMatcher(instance, kPasswordSiteUrlRegex);
  }
};

base::LazyInstance<re2::RE2, PasswordSiteUrlLazyInstanceTraits>
    g_password_site_matcher = LAZY_INSTANCE_INITIALIZER;

// Returns the |input_field| name if its non-empty; otherwise a |dummy_name|.
base::string16 FieldName(const WebInputElement& input_field,
                         const char dummy_name[]) {
  base::string16 field_name = input_field.NameForAutofill().Utf16();
  return field_name.empty() ? base::ASCIIToUTF16(dummy_name) : field_name;
}

// Returns true iff the properties mask of |element| intersects with |mask|.
bool FieldHasPropertiesMask(const FieldValueAndPropertiesMaskMap* field_map,
                            const blink::WebFormControlElement& element,
                            FieldPropertiesMask mask) {
  if (!field_map)
    return false;
  FieldValueAndPropertiesMaskMap::const_iterator it = field_map->find(element);
  return it != field_map->end() && (it->second.second & mask);
}

// Helper function that checks the presence of visible password and username
// fields in |form.control_elements|.
// Iff a visible password is found, then |*found_visible_password| is set to
// true. Iff a visible password is found AND there is a visible username before
// it, then |*found_visible_username_before_visible_password| is set to true.
void FindVisiblePasswordAndVisibleUsernameBeforePassword(
    const SyntheticForm& form,
    bool* found_visible_password,
    bool* found_visible_username_before_visible_password) {
  DCHECK(found_visible_password);
  DCHECK(found_visible_username_before_visible_password);
  *found_visible_password = false;
  *found_visible_username_before_visible_password = false;

  bool found_visible_username = false;
  for (auto& control_element : form.control_elements) {
    const WebInputElement* input_element = ToWebInputElement(&control_element);
    if (!input_element || !input_element->IsEnabled() ||
        !input_element->IsTextField()) {
      continue;
    }

    if (!form_util::IsWebElementVisible(*input_element))
      continue;

    if (input_element->IsPasswordField()) {
      *found_visible_password = true;
      *found_visible_username_before_visible_password = found_visible_username;
      break;
    } else {
      found_visible_username = true;
    }
  }
}

autofill::PossibleUsernamePair MakePossibleUsernamePair(
    const blink::WebInputElement& input) {
  base::string16 trimmed_input_value, trimmed_input_autofill;
  base::TrimString(input.Value().Utf16(), base::ASCIIToUTF16(" "),
                   &trimmed_input_value);
  return autofill::PossibleUsernamePair(trimmed_input_value,
                                        input.NameForAutofill().Utf16());
}

// Check if a script modified username is suitable for Password Manager to
// remember.
bool ScriptModifiedUsernameAcceptable(
    const base::string16& username_value,
    const base::string16& typed_username_value,
    const PasswordForm* password_form,
    const FieldValueAndPropertiesMaskMap* field_value_and_properties_map) {
  DCHECK(password_form);
  DCHECK(field_value_and_properties_map);
  // The minimal size of a field value that will be matched.
  const size_t kMinMatchSize = 3u;
  const auto username = base::i18n::ToLower(username_value);
  const auto typed_username = base::i18n::ToLower(typed_username_value);
  if (base::StartsWith(username, typed_username, base::CompareCase::SENSITIVE))
    return true;

  // Check if the username was generated by javascript based on user typed name.
  if (typed_username.size() >= kMinMatchSize &&
      username_value.find(typed_username) != base::string16::npos)
    return true;

  // Check if the username was generated by javascript based on user typed or
  // autofilled field values.
  for (const auto& field_prop : *field_value_and_properties_map) {
    if (!field_prop.second.first)
      continue;
    const auto& field_value = *field_prop.second.first;
    const WebInputElement* input_element = ToWebInputElement(&field_prop.first);
    if (input_element && input_element->IsTextField() &&
        !input_element->IsPasswordField() &&
        field_value.size() >= kMinMatchSize &&
        username_value.find(base::i18n::ToLower(field_value)) !=
            base::string16::npos) {
      return true;
    }
  }

  return false;
}

// Get information about a login form encapsulated in a PasswordForm struct.
// If an element of |form| has an entry in |nonscript_modified_values|, the
// associated string is used instead of the element's value to create
// the PasswordForm.
bool GetPasswordForm(
    const SyntheticForm& form,
    PasswordForm* password_form,
    const FieldValueAndPropertiesMaskMap* field_value_and_properties_map,
    const FormsPredictionsMap* form_predictions) {
  WebInputElement latest_input_element;
  WebInputElement username_element;
  password_form->username_marked_by_site = false;
  std::vector<WebInputElement> passwords;
  std::map<blink::WebInputElement, blink::WebInputElement>
      last_text_input_before_password;
  std::vector<WebInputElement> all_possible_usernames;

  std::map<WebInputElement, PasswordFormFieldPredictionType> predicted_elements;
  if (form_predictions) {
    FindPredictedElements(form, password_form->form_data, *form_predictions,
                          &predicted_elements);
  }
  // Check the presence of visible password and username fields.
  // If there is a visible password field, then ignore invisible password
  // fields. If there is a visible username before visible password, then ignore
  // invisible username fields.
  // If there is no visible password field, don't ignore any elements (i.e. use
  // the latest username field just before selected password field).
  bool ignore_invisible_passwords = false;
  bool ignore_invisible_usernames = false;
  FindVisiblePasswordAndVisibleUsernameBeforePassword(
      form, &ignore_invisible_passwords, &ignore_invisible_usernames);
  std::string layout_sequence;
  layout_sequence.reserve(form.control_elements.size());
  size_t number_of_non_empty_text_non_password_fields = 0;
  for (size_t i = 0; i < form.control_elements.size(); ++i) {
    WebFormControlElement control_element = form.control_elements[i];

    WebInputElement* input_element = ToWebInputElement(&control_element);
    if (!input_element || !input_element->IsEnabled())
      continue;

    if (HasCreditCardAutocompleteAttributes(*input_element))
      continue;
    if (IsCreditCardVerificationPasswordField(*input_element))
      continue;

    bool element_is_invisible = !form_util::IsWebElementVisible(*input_element);
    if (input_element->IsTextField()) {
      if (input_element->IsPasswordField()) {
        if (element_is_invisible && ignore_invisible_passwords)
          continue;
        layout_sequence.push_back('P');
      } else {
        if (FieldHasPropertiesMask(field_value_and_properties_map,
                                   *input_element,
                                   FieldPropertiesFlags::USER_TYPED |
                                       FieldPropertiesFlags::AUTOFILLED)) {
          ++number_of_non_empty_text_non_password_fields;
        }
        if (element_is_invisible && ignore_invisible_usernames)
          continue;
        layout_sequence.push_back('N');
      }
    }

    bool password_marked_by_autocomplete_attribute =
        HasAutocompleteAttributeValue(*input_element,
                                      kAutocompleteCurrentPassword) ||
        HasAutocompleteAttributeValue(*input_element, kAutocompleteNewPassword);

    // If the password field is readonly, the page is likely using a virtual
    // keyboard and bypassing the password field value (see
    // http://crbug.com/475488). There is nothing Chrome can do to fill
    // passwords for now. Continue processing in case when the password field
    // was made readonly by JavaScript before submission. We can do this by
    // checking whether password element was updated not from JavaScript.
    if (input_element->IsPasswordField() &&
        (!input_element->IsReadOnly() ||
         FieldHasPropertiesMask(field_value_and_properties_map, *input_element,
                                FieldPropertiesFlags::USER_TYPED |
                                    FieldPropertiesFlags::AUTOFILLED) ||
         password_marked_by_autocomplete_attribute)) {
      // We add the field to the list of password fields if it was not flagged
      // as a special NOT_PASSWORD prediction by Autofill. The NOT_PASSWORD
      // mechanism exists because some webpages use the type "password" for
      // fields which Autofill knows shouldn't be treated as passwords by the
      // Password Manager. This is ultimately bypassed if the field has
      // autocomplete attributes.
      auto possible_password_element_iterator =
          predicted_elements.find(*input_element);
      if (password_marked_by_autocomplete_attribute ||
          possible_password_element_iterator == predicted_elements.end() ||
          possible_password_element_iterator->second !=
              PREDICTION_NOT_PASSWORD) {
        passwords.push_back(*input_element);
        last_text_input_before_password[*input_element] = latest_input_element;
      }
    }

    // Various input types such as text, url, email can be a username field.
    if (input_element->IsTextField() && !input_element->IsPasswordField()) {
      if (!input_element->Value().IsEmpty()) {
        all_possible_usernames.push_back(*input_element);
      }
      if (HasAutocompleteAttributeValue(*input_element,
                                        kAutocompleteUsername)) {
        if (password_form->username_marked_by_site) {
          // A second or subsequent element marked with autocomplete='username'.
          // This makes us less confident that we have understood the form. We
          // will stick to our choice that the first such element was the real
          // username.
        } else {
          // The first element marked with autocomplete='username'. Take the
          // hint and treat it as the username (overruling the tentative choice
          // we might have made before). Furthermore, drop all other possible
          // usernames we have accrued so far: they come from fields not marked
          // with the autocomplete attribute, making them unlikely alternatives.
          username_element = *input_element;
          password_form->username_marked_by_site = true;
        }
      } else {
        if (password_form->username_marked_by_site) {
          // Having seen elements with autocomplete='username', elements without
          // this attribute are no longer interesting. No-op.
        } else {
          // No elements marked with autocomplete='username' so far whatsoever.
          // If we have not yet selected a username element even provisionally,
          // then remember this element for the case when the next field turns
          // out to be a password. Save a non-empty username as a possible
          // alternative, at least for now.
          if (username_element.IsNull())
            latest_input_element = *input_element;
        }
      }
    }
  }

  if (passwords.empty())
    return false;

  // Call HTML based username detector, only if corresponding flag is enabled.
  if (base::FeatureList::IsEnabled(
          password_manager::features::kEnableHtmlBasedUsernameDetector)) {
    if (username_element.IsNull()) {
      GetUsernameFieldBasedOnHtmlAttributes(
          all_possible_usernames, password_form->form_data, &username_element);
    }
  }

  WebInputElement password;
  WebInputElement new_password;
  WebInputElement confirmation_password;
  LocateSpecificPasswords(passwords, &password, &new_password,
                          &confirmation_password);

  if (base::FeatureList::IsEnabled(
          password_manager::features::kEnablePasswordSelection)) {
    bool form_has_autofilled_value = false;
    // Add non-empty unique possible passwords to the vector.
    std::vector<base::string16> all_possible_passwords;
    for (const WebInputElement& password_element : passwords) {
      const base::string16 value = password_element.Value().Utf16();
      if (value.empty())
        continue;
      bool element_has_autofilled_value = FieldHasPropertiesMask(
          field_value_and_properties_map, password_element,
          FieldPropertiesFlags::AUTOFILLED);
      form_has_autofilled_value |= element_has_autofilled_value;
      if (find(all_possible_passwords.begin(), all_possible_passwords.end(),
               value) == all_possible_passwords.end()) {
        all_possible_passwords.push_back(std::move(value));
      }
    }

    if (!all_possible_passwords.empty()) {
      password_form->all_possible_passwords = std::move(all_possible_passwords);
      password_form->form_has_autofilled_value = form_has_autofilled_value;
    }
  }

  // Base heuristic for username detection.
  DCHECK_EQ(passwords.size(), last_text_input_before_password.size());
  if (username_element.IsNull()) {
    if (!password.IsNull())
      username_element = last_text_input_before_password[password];
    if (username_element.IsNull() && !new_password.IsNull())
      username_element = last_text_input_before_password[new_password];
  }
  password_form->layout = SequenceToLayout(layout_sequence);

  WebInputElement predicted_username_element;
  bool map_has_username_prediction = MapContainsPrediction(
      predicted_elements, PREDICTION_USERNAME, &predicted_username_element);

  // Let server predictions override the selection of the username field. This
  // allows instant adjusting without changing Chromium code.
  auto username_element_iterator = predicted_elements.find(username_element);
  if (map_has_username_prediction &&
      (username_element_iterator == predicted_elements.end() ||
       username_element_iterator->second != PREDICTION_USERNAME)) {
    username_element = predicted_username_element;
    password_form->was_parsed_using_autofill_predictions = true;
  }

  if (!username_element.IsNull()) {
    password_form->username_element =
        FieldName(username_element, "anonymous_username");
    base::string16 username_value = username_element.Value().Utf16();
    if (FieldHasPropertiesMask(field_value_and_properties_map, username_element,
                               FieldPropertiesFlags::USER_TYPED |
                                   FieldPropertiesFlags::AUTOFILLED)) {
      base::string16 typed_username_value =
          *field_value_and_properties_map->at(username_element).first;

      if (!ScriptModifiedUsernameAcceptable(username_value,
                                            typed_username_value, password_form,
                                            field_value_and_properties_map)) {
        // We check that |username_value| was not obtained by autofilling
        // |typed_username_value|. In case when it was, |typed_username_value|
        // is incomplete, so we should leave autofilled value.
        username_value = typed_username_value;
      }
    }
    password_form->username_value = username_value;
  }

  password_form->origin =
      form_util::GetCanonicalOriginForDocument(form.document);
  password_form->signon_realm = GetSignOnRealm(password_form->origin);

  // Remove |username_element| from the vector |all_possible_usernames| if the
  // value presents in the vector.
  if (!username_element.IsNull()) {
    all_possible_usernames.erase(
        std::remove(all_possible_usernames.begin(),
                    all_possible_usernames.end(), username_element),
        all_possible_usernames.end());
  }
  // Convert |all_possible_usernames| to PossibleUsernamesVector.
  autofill::PossibleUsernamesVector other_possible_usernames;
  for (WebInputElement possible_username : all_possible_usernames) {
    other_possible_usernames.push_back(
        MakePossibleUsernamePair(possible_username));
  }
  password_form->other_possible_usernames.swap(other_possible_usernames);

  if (!password.IsNull()) {
    password_form->password_element = FieldName(password, "anonymous_password");
    blink::WebString password_value = password.Value();
    if (FieldHasPropertiesMask(field_value_and_properties_map, password,
                               FieldPropertiesFlags::USER_TYPED |
                                   FieldPropertiesFlags::AUTOFILLED)) {
      password_value = blink::WebString::FromUTF16(
          *field_value_and_properties_map->at(password).first);
    }
    password_form->password_value = password_value.Utf16();
  }
  if (!new_password.IsNull()) {
    password_form->new_password_element =
        FieldName(new_password, "anonymous_new_password");
    password_form->new_password_value = new_password.Value().Utf16();
    password_form->new_password_value_is_default =
        new_password.GetAttribute("value") == new_password.Value();
    if (HasAutocompleteAttributeValue(new_password, kAutocompleteNewPassword))
      password_form->new_password_marked_by_site = true;
    if (!confirmation_password.IsNull()) {
      password_form->confirmation_password_element =
          FieldName(confirmation_password, "anonymous_confirmation_password");
    }
  }

  if (username_element.IsNull()) {
    // To get a better idea on how password forms without a username field
    // look like, report the total number of text and password fields.
    UMA_HISTOGRAM_COUNTS_100(
        "PasswordManager.EmptyUsernames.TextAndPasswordFieldCount",
        layout_sequence.size());
    // For comparison, also report the number of password fields.
    UMA_HISTOGRAM_COUNTS_100(
        "PasswordManager.EmptyUsernames.PasswordFieldCount",
        std::count(layout_sequence.begin(), layout_sequence.end(), 'P'));
  }

  password_form->scheme = PasswordForm::SCHEME_HTML;
  password_form->preferred = false;
  password_form->blacklisted_by_user = false;
  password_form->type = PasswordForm::TYPE_MANUAL;

  // The password form is considered that it looks like SignUp form if it has
  // more than 1 text field with user input or it has a new password field and
  // no current password field.
  password_form->does_look_like_signup_form =
      number_of_non_empty_text_non_password_fields > 1 ||
      (number_of_non_empty_text_non_password_fields == 1 &&
       password_form->password_element.empty() &&
       !password_form->new_password_element.empty());
  return true;
}

}  // namespace

re2::RE2* CreateMatcher(void* instance, const char* pattern) {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  // Use placement new to initialize the instance in the preallocated space.
  // The "(instance)" is very important to force POD type initialization.
  re2::RE2* matcher = new (instance) re2::RE2(pattern, options);
  DCHECK(matcher->ok());
  return matcher;
}

bool IsGaiaReauthenticationForm(const blink::WebFormElement& form) {
  if (GURL(form.GetDocument().Url()).GetOrigin() !=
      GaiaUrls::GetInstance()->gaia_url().GetOrigin()) {
    return false;
  }

  bool has_rart_field = false;
  bool has_continue_field = false;

  blink::WebVector<blink::WebFormControlElement> web_control_elements;
  form.GetFormControlElements(web_control_elements);

  for (const blink::WebFormControlElement& element : web_control_elements) {
    // We're only interested in the presence
    // of <input type="hidden" /> elements.
    CR_DEFINE_STATIC_LOCAL(WebString, kHidden, ("hidden"));
    const blink::WebInputElement* input = blink::ToWebInputElement(&element);
    if (!input || input->FormControlType() != kHidden)
      continue;

    // There must be a hidden input named "rart".
    if (input->FormControlName() == "rart")
      has_rart_field = true;

    // There must be a hidden input named "continue", whose value points
    // to a password (or password testing) site.
    if (input->FormControlName() == "continue" &&
        re2::RE2::PartialMatch(input->Value().Utf8(),
                               g_password_site_matcher.Get())) {
      has_continue_field = true;
    }
  }

  return has_rart_field && has_continue_field;
}

std::unique_ptr<PasswordForm> CreatePasswordFormFromWebForm(
    const WebFormElement& web_form,
    const FieldValueAndPropertiesMaskMap* field_value_and_properties_map,
    const FormsPredictionsMap* form_predictions) {
  if (web_form.IsNull())
    return std::unique_ptr<PasswordForm>();

  std::unique_ptr<PasswordForm> password_form(new PasswordForm());
  password_form->action = form_util::GetCanonicalActionForForm(web_form);
  if (!password_form->action.is_valid())
    return std::unique_ptr<PasswordForm>();

  SyntheticForm synthetic_form;
  PopulateSyntheticFormFromWebForm(web_form, &synthetic_form);

  if (!WebFormElementToFormData(
          web_form, blink::WebFormControlElement(),
          field_value_and_properties_map, form_util::EXTRACT_NONE,
          &password_form->form_data, NULL /* FormFieldData */)) {
    return std::unique_ptr<PasswordForm>();
  }

  if (!GetPasswordForm(synthetic_form, password_form.get(),
                       field_value_and_properties_map, form_predictions)) {
    return std::unique_ptr<PasswordForm>();
  }
  return password_form;
}

std::unique_ptr<PasswordForm> CreatePasswordFormFromUnownedInputElements(
    const WebLocalFrame& frame,
    const FieldValueAndPropertiesMaskMap* field_value_and_properties_map,
    const FormsPredictionsMap* form_predictions) {
  SyntheticForm synthetic_form;
  synthetic_form.control_elements = form_util::GetUnownedFormFieldElements(
      frame.GetDocument().All(), &synthetic_form.fieldsets);
  synthetic_form.document = frame.GetDocument();

  if (synthetic_form.control_elements.empty())
    return std::unique_ptr<PasswordForm>();

  std::unique_ptr<PasswordForm> password_form(new PasswordForm());
  if (!UnownedPasswordFormElementsAndFieldSetsToFormData(
          synthetic_form.fieldsets, synthetic_form.control_elements, nullptr,
          frame.GetDocument(), field_value_and_properties_map,
          form_util::EXTRACT_NONE, &password_form->form_data,
          nullptr /* FormFieldData */)) {
    return std::unique_ptr<PasswordForm>();
  }
  if (!GetPasswordForm(synthetic_form, password_form.get(),
                       field_value_and_properties_map, form_predictions)) {
    return std::unique_ptr<PasswordForm>();
  }

  // No actual action on the form, so use the the origin as the action.
  password_form->action = password_form->origin;
  return password_form;
}

bool HasAutocompleteAttributeValue(const blink::WebInputElement& element,
                                   base::StringPiece value_in_lowercase) {
  CR_DEFINE_STATIC_LOCAL(WebString, kAutocomplete, ("autocomplete"));
  const std::string autocomplete_value =
      element.GetAttribute(kAutocomplete)
          .Utf8(WebString::UTF8ConversionMode::kStrictReplacingErrorsWithFFFD);

  std::vector<base::StringPiece> tokens =
      base::SplitStringPiece(autocomplete_value, base::kWhitespaceASCII,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return std::find_if(tokens.begin(), tokens.end(),
                      [value_in_lowercase](base::StringPiece token) {
                        return base::LowerCaseEqualsASCII(token,
                                                          value_in_lowercase);
                      }) != tokens.end();
}

bool HasCreditCardAutocompleteAttributes(
    const blink::WebInputElement& element) {
  CR_DEFINE_STATIC_LOCAL(WebString, kAutocomplete, ("autocomplete"));
  const std::string autocomplete_value =
      element.GetAttribute(kAutocomplete)
          .Utf8(WebString::UTF8ConversionMode::kStrictReplacingErrorsWithFFFD);

  std::vector<base::StringPiece> tokens =
      base::SplitStringPiece(autocomplete_value, base::kWhitespaceASCII,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return std::find_if(
             tokens.begin(), tokens.end(), [](base::StringPiece token) {
               return base::StartsWith(token, kAutocompleteCreditCardPrefix,
                                       base::CompareCase::INSENSITIVE_ASCII);
             }) != tokens.end();
}

bool IsCreditCardVerificationPasswordField(
    const blink::WebInputElement& field) {
  if (!field.IsPasswordField())
    return false;

  static const base::string16 kCardCvcReCached = base::UTF8ToUTF16(kCardCvcRe);

  return MatchesPattern(field.GetAttribute("id").Utf16(), kCardCvcReCached) ||
         MatchesPattern(field.GetAttribute("name").Utf16(), kCardCvcReCached);
}

std::string GetSignOnRealm(const GURL& origin) {
  GURL::Replacements rep;
  rep.SetPathStr("");
  return origin.ReplaceComponents(rep).spec();
}

}  // namespace autofill
