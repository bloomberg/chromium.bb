// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/form_parser.h"

#include <algorithm>
#include <iterator>
#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form.h"

using autofill::FieldPropertiesFlags;
using autofill::FormFieldData;
using autofill::PasswordForm;

namespace password_manager {

namespace {

constexpr char kAutocompleteUsername[] = "username";
constexpr char kAutocompleteCurrentPassword[] = "current-password";
constexpr char kAutocompleteNewPassword[] = "new-password";
constexpr char kAutocompleteCreditCardPrefix[] = "cc-";

// The susbset of autocomplete flags related to passwords.
enum class AutocompleteFlag {
  NONE,
  USERNAME,
  CURRENT_PASSWORD,
  NEW_PASSWORD,
  // Represents the whole family of cc-* flags.
  CREDIT_CARD
};

// The autocomplete attribute has one of the following structures:
//   [section-*] [shipping|billing] [type_hint] field_type
//   on | off | false
// (see
// https://html.spec.whatwg.org/multipage/form-control-infrastructure.html#autofilling-form-controls%3A-the-autocomplete-attribute).
// For password forms, only the field_type is relevant. So parsing the attribute
// amounts to just taking the last token.  If that token is one of "username",
// "current-password" or "new-password", this returns an appropriate enum value.
// If the token starts with a "cc-" prefix, this returns CREDIT_CARD.
// Otherwise, returns NONE.
AutocompleteFlag ExtractAutocompleteFlag(const std::string& attribute) {
  std::vector<base::StringPiece> tokens =
      base::SplitStringPiece(attribute, base::kWhitespaceASCII,
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.empty())
    return AutocompleteFlag::NONE;

  const base::StringPiece& field_type = tokens.back();
  if (base::LowerCaseEqualsASCII(field_type, kAutocompleteUsername))
    return AutocompleteFlag::USERNAME;
  if (base::LowerCaseEqualsASCII(field_type, kAutocompleteCurrentPassword))
    return AutocompleteFlag::CURRENT_PASSWORD;
  if (base::LowerCaseEqualsASCII(field_type, kAutocompleteNewPassword))
    return AutocompleteFlag::NEW_PASSWORD;

  if (base::StartsWith(field_type, kAutocompleteCreditCardPrefix,
                       base::CompareCase::SENSITIVE))
    return AutocompleteFlag::CREDIT_CARD;

  return AutocompleteFlag::NONE;
}

// Helper to spare map::find boilerplate when caching field's autocomplete
// attributes.
class AutocompleteCache {
 public:
  AutocompleteCache();

  ~AutocompleteCache();

  // Computes and stores the AutocompleteFlag for |field| based on its
  // autocomplete attribute. Note that this cannot be done on-demand during
  // RetrieveFor, because the cache spares space and look-up time by not storing
  // AutocompleteFlag::NONE values, hence for all elements without an
  // autocomplete attribute, every retrieval would result in a new computation.
  void Store(const FormFieldData* field);

  // Retrieves the value previously stored for |field|.
  AutocompleteFlag RetrieveFor(const FormFieldData* field) const;

 private:
  std::map<const FormFieldData*, AutocompleteFlag> cache_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteCache);
};

AutocompleteCache::AutocompleteCache() = default;

AutocompleteCache::~AutocompleteCache() = default;

void AutocompleteCache::Store(const FormFieldData* field) {
  const AutocompleteFlag flag =
      ExtractAutocompleteFlag(field->autocomplete_attribute);
  // Only store non-trivial flags. Most of the elements will have the NONE
  // value, so spare storage and lookup time by assuming anything not stored in
  // |cache_| has the NONE flag.
  if (flag != AutocompleteFlag::NONE)
    cache_[field] = flag;
}

AutocompleteFlag AutocompleteCache::RetrieveFor(
    const FormFieldData* field) const {
  auto it = cache_.find(field);
  return it == cache_.end() ? AutocompleteFlag::NONE : it->second;
}

// Helper struct that is used to return results from the parsing function.
struct ParseResult {
  const FormFieldData* username_field = nullptr;
  const FormFieldData* password_field = nullptr;
  const FormFieldData* new_password_field = nullptr;
  const FormFieldData* confirmation_password_field = nullptr;

  bool IsEmpty() {
    return password_field == nullptr && new_password_field == nullptr;
  }
};

// Returns text fields from |fields|.
std::vector<const FormFieldData*> GetTextFields(
    const std::vector<FormFieldData>& fields) {
  std::vector<const FormFieldData*> result;
  result.reserve(fields.size());
  for (const auto& field : fields) {
    if (field.IsTextInputElement())
      result.push_back(&field);
  }
  return result;
}

// Filters fields that have credit card related autocomplete attributes out of
// |fields|. Uses |autocomplete_cache| to get the autocomplete attributes.
void FilterCreditCardFields(std::vector<const FormFieldData*>* fields,
                            const AutocompleteCache& autocomplete_cache) {
  base::EraseIf(*fields, [&autocomplete_cache](const FormFieldData* field) {
    return autocomplete_cache.RetrieveFor(field) ==
           AutocompleteFlag::CREDIT_CARD;
  });
}

// Returns true iff there is a password field.
bool HasPasswordField(const std::vector<const FormFieldData*>& fields) {
  for (const FormFieldData* field : fields)
    if (field->form_control_type == "password")
      return true;
  return false;
}

// Returns the first element of |fields| which has the specified
// |unique_renderer_id|, or null if there is no such element.
const FormFieldData* FindFieldWithUniqueRendererId(
    const std::vector<const FormFieldData*>& fields,
    uint32_t unique_renderer_id) {
  for (const FormFieldData* field : fields) {
    if (field->unique_renderer_id == unique_renderer_id)
      return field;
  }
  return nullptr;
}

// Tries to parse |fields| based on server |predictions|.
std::unique_ptr<ParseResult> ParseUsingPredictions(
    const std::vector<const FormFieldData*>& fields,
    const FormPredictions& predictions) {
  auto result = std::make_unique<ParseResult>();
  // Note: The code does not check whether there is at most 1 username, 1
  // current password and at most 2 new passwords. It is assumed that server
  // side predictions are sane.
  for (const auto& prediction : predictions) {
    switch (DeriveFromServerFieldType(prediction.second.type)) {
      case CredentialFieldType::kUsername:
        result->username_field =
            FindFieldWithUniqueRendererId(fields, prediction.first);
        break;
      case CredentialFieldType::kCurrentPassword:
        result->password_field =
            FindFieldWithUniqueRendererId(fields, prediction.first);
        break;
      case CredentialFieldType::kNewPassword:
        result->new_password_field =
            FindFieldWithUniqueRendererId(fields, prediction.first);
        break;
      case CredentialFieldType::kConfirmationPassword:
        result->confirmation_password_field =
            FindFieldWithUniqueRendererId(fields, prediction.first);
        break;
      case CredentialFieldType::kNone:
        break;
    }
  }
  return result->IsEmpty() ? nullptr : std::move(result);
}

// Tries to parse |fields| based on autocomplete attributes from
// |autocomplete_cache|. Assumption on the usage autocomplete attributes:
// 1. Not more than 1 field with autocomplete=username.
// 2. Not more than 1 field with autocomplete=current-password.
// 3. Not more than 2 fields with autocomplete=new-password.
// 4. Only password fields have "*-password" attribute and only non-password
//    fields have the "username" attribute.
// Are these assumptions violated, or is there no password with an autocomplete
// attribute, parsing is unsuccessful. Returns nullptr if parsing is
// unsuccessful.
std::unique_ptr<ParseResult> ParseUsingAutocomplete(
    const std::vector<const FormFieldData*>& fields,
    const AutocompleteCache& autocomplete_cache) {
  auto result = std::make_unique<ParseResult>();
  for (const FormFieldData* field : fields) {
    AutocompleteFlag flag = autocomplete_cache.RetrieveFor(field);
    const bool is_password = field->form_control_type == "password";
    switch (flag) {
      case AutocompleteFlag::USERNAME:
        if (is_password || result->username_field)
          return nullptr;
        result->username_field = field;
        break;
      case AutocompleteFlag::CURRENT_PASSWORD:
        if (!is_password || result->password_field)
          return nullptr;
        result->password_field = field;
        break;
      case AutocompleteFlag::NEW_PASSWORD:
        if (!is_password)
          return nullptr;
        // The first field with autocomplete=new-password is considered to be
        // new_password_field and the second is confirmation_password_field.
        if (!result->new_password_field)
          result->new_password_field = field;
        else if (!result->confirmation_password_field)
          result->confirmation_password_field = field;
        else
          return nullptr;
        break;
      case AutocompleteFlag::CREDIT_CARD:
      case AutocompleteFlag::NONE:
        break;
    }
  }

  return result->IsEmpty() ? nullptr : std::move(result);
}

// Returns only relevant password fields from |fields|. Namely
// 1. If there is a focusable password field, return only focusable.
// 2. If mode == SAVING return only non-empty fields (for saving empty fields
// are useless).
// Note that focusability is the proxy for visibility.
std::vector<const FormFieldData*> GetRelevantPasswords(
    const std::vector<const FormFieldData*>& fields,
    FormParsingMode mode) {
  std::vector<const FormFieldData*> result;
  result.reserve(fields.size());

  const bool consider_only_non_empty = mode == FormParsingMode::SAVING;
  bool found_focusable = false;

  for (const FormFieldData* field : fields) {
    if (field->form_control_type != "password")
      continue;
    if (consider_only_non_empty && field->value.empty())
      continue;
    // Readonly fields can be an indication that filling is useless (e.g., the
    // page might use a virtual keyboard). However, if the field was readonly
    // only temporarily, that makes it still interesting for saving. The fact
    // that a user typed or Chrome filled into that field in tha past is an
    // indicator that the radonly was only temporary.
    if (field->is_readonly &&
        !(field->properties_mask & (FieldPropertiesFlags::USER_TYPED |
                                    FieldPropertiesFlags::AUTOFILLED))) {
      continue;
    }
    result.push_back(field);
    found_focusable |= field->is_focusable;
  }

  if (found_focusable) {
    base::EraseIf(result, [](const FormFieldData* field) {
      return !field->is_focusable;
    });
  }

  return result;
}

// Detects different password fields from |passwords|.
void LocateSpecificPasswords(const std::vector<const FormFieldData*>& passwords,
                             const FormFieldData** current_password,
                             const FormFieldData** new_password,
                             const FormFieldData** confirmation_password) {
  DCHECK(current_password);
  DCHECK(!*current_password);
  DCHECK(new_password);
  DCHECK(!*new_password);
  DCHECK(confirmation_password);
  DCHECK(!*confirmation_password);

  switch (passwords.size()) {
    case 1:
      *current_password = passwords[0];
      break;
    case 2:
      if (!passwords[0]->value.empty() &&
          passwords[0]->value == passwords[1]->value) {
        // Two identical non-empty passwords: assume we are seeing a new
        // password with a confirmation. This can be either a sign-up form or a
        // password change form that does not ask for the old password.
        *new_password = passwords[0];
        *confirmation_password = passwords[1];
      } else {
        // Assume first is old password, second is new (no choice but to guess).
        // If the passwords are both empty, it is impossible to tell if they
        // are the old and the new one, or the new one and its confirmation. In
        // that case Chrome errs on the side of filling and classifies them as
        // old & new to allow filling of change password forms.
        *current_password = passwords[0];
        *new_password = passwords[1];
      }
      break;
    default:
      // If there are more than 3 passwords it is not very clear what this form
      // it is. Consider only the first 3 passwords in such case as a
      // best-effort solution.
      if (!passwords[0]->value.empty() &&
          passwords[0]->value == passwords[1]->value &&
          passwords[0]->value == passwords[2]->value) {
        // All passwords are the same. Assume that the first field is the
        // current password.
        *current_password = passwords[0];
      } else if (passwords[1]->value == passwords[2]->value) {
        // New password is the duplicated one, and comes second; or empty form
        // with at least 3 password fields.
        *current_password = passwords[0];
        *new_password = passwords[1];
        *confirmation_password = passwords[2];
      } else if (passwords[0]->value == passwords[1]->value) {
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

// Tries to find username field among text fields from |fields| occurring before
// |first_relevant_password|. Returns nullptr if the username is not found.
const FormFieldData* FindUsernameFieldBaseHeuristics(
    const std::vector<const FormFieldData*>& fields,
    const FormFieldData* first_relevant_password,
    FormParsingMode mode) {
  DCHECK(first_relevant_password);

  // Let username_candidates be all non-password fields before
  // |first_relevant_password|.
  auto first_relevant_password_it =
      std::find(fields.begin(), fields.end(), first_relevant_password);
  DCHECK(first_relevant_password_it != fields.end());

  // For saving filter out empty fields.
  const bool consider_only_non_empty = mode == FormParsingMode::SAVING;

  // Search through the text input fields preceding |first_relevant_password|
  // and find the closest one focusable and the closest one in general.

  const FormFieldData* focusable_username = nullptr;
  const FormFieldData* username = nullptr;
  // Do reverse search to find the closest candidates preceding the password.
  for (auto it = std::make_reverse_iterator(first_relevant_password_it);
       it != fields.rend(); ++it) {
    if ((*it)->form_control_type == "password")
      continue;
    if (consider_only_non_empty && (*it)->value.empty())
      continue;
    if (!username)
      username = *it;
    if ((*it)->is_focusable) {
      focusable_username = *it;
      break;
    }
  }

  return focusable_username ? focusable_username : username;
}

std::unique_ptr<ParseResult> ParseUsingBaseHeuristics(
    const std::vector<const FormFieldData*>& fields,
    FormParsingMode mode) {
  // Try to find password elements (current, new, confirmation).
  std::vector<const FormFieldData*> passwords =
      GetRelevantPasswords(fields, mode);
  if (passwords.empty())
    return nullptr;

  auto result = std::make_unique<ParseResult>();
  LocateSpecificPasswords(passwords, &result->password_field,
                          &result->new_password_field,
                          &result->confirmation_password_field);
  if (result->IsEmpty())
    return nullptr;

  // If password elements are found then try to find a username.
  result->username_field =
      FindUsernameFieldBaseHeuristics(fields, passwords[0], mode);
  return result;
}

// Set username and password fields from |parse_result| in |password_form|.
void SetFields(const ParseResult& parse_result, PasswordForm* password_form) {
  if (parse_result.username_field) {
    password_form->username_element = parse_result.username_field->name;
    password_form->username_value = parse_result.username_field->value;
  }

  if (parse_result.password_field) {
    password_form->password_element = parse_result.password_field->name;
    password_form->password_value = parse_result.password_field->value;
  }

  if (parse_result.new_password_field) {
    password_form->new_password_element = parse_result.new_password_field->name;
    password_form->new_password_value = parse_result.new_password_field->value;
  }

  if (parse_result.confirmation_password_field) {
    password_form->confirmation_password_element =
        parse_result.confirmation_password_field->name;
  }
}

}  // namespace

std::unique_ptr<PasswordForm> ParseFormData(
    const autofill::FormData& form_data,
    const FormPredictions* form_predictions,
    FormParsingMode mode) {

  std::vector<const FormFieldData*> fields = GetTextFields(form_data.fields);

  // Fill the cache with autocomplete flags.
  AutocompleteCache autocomplete_cache;
  for (const FormFieldData* input : fields)
    autocomplete_cache.Store(input);

  FilterCreditCardFields(&fields, autocomplete_cache);

  // Skip forms without password fields.
  if (!HasPasswordField(fields))
    return nullptr;

  // Create parse result and set non-field related information.
  auto result = std::make_unique<PasswordForm>();
  result->origin = form_data.origin;
  result->signon_realm = form_data.origin.GetOrigin().spec();
  result->action = form_data.action;
  result->form_data = form_data;

  if (form_predictions) {
    // Try to parse with server predictions.
    auto predictions_parse_result =
        ParseUsingPredictions(fields, *form_predictions);
    if (predictions_parse_result) {
      SetFields(*predictions_parse_result, result.get());
      return result;
    }
  }

  // Try to parse with autocomplete attributes.
  auto autocomplete_parse_result =
      ParseUsingAutocomplete(fields, autocomplete_cache);
  if (autocomplete_parse_result) {
    SetFields(*autocomplete_parse_result, result.get());
    return result;
  }

  // Try to parse with base heuristic.
  auto base_heuristics_parse_result = ParseUsingBaseHeuristics(fields, mode);
  if (base_heuristics_parse_result) {
    SetFields(*base_heuristics_parse_result, result.get());
    return result;
  }

  return nullptr;
}

}  // namespace password_manager
