// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/form_parsing/form_parser.h"

#include <stddef.h>

#include <algorithm>

#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using autofill::FieldPropertiesFlags;
using autofill::FormData;
using autofill::FormFieldData;
using autofill::PasswordForm;
using base::ASCIIToUTF16;

namespace password_manager {

namespace {

// Use this value in FieldDataDescription.value to get an arbitrary unique value
// generated in GetFormDataAndExpectation().
constexpr char kNonimportantValue[] = "non-important unique";

// Use this in FieldDataDescription below to mark the expected username and
// password fields. The *_FILLING variants apply to FormParsingMode::FILLING
// only, the *_SAVING variants to FormParsingMode::SAVING only, the suffix-less
// variants to both.
enum class ElementRole {
  NONE,
  USERNAME_FILLING,
  USERNAME_SAVING,
  USERNAME,
  CURRENT_PASSWORD_FILLING,
  CURRENT_PASSWORD_SAVING,
  CURRENT_PASSWORD,
  NEW_PASSWORD_FILLING,
  NEW_PASSWORD_SAVING,
  NEW_PASSWORD,
  CONFIRMATION_PASSWORD_FILLING,
  CONFIRMATION_PASSWORD_SAVING,
  CONFIRMATION_PASSWORD
};

// Expected FormFieldData are constructed based on these descriptions.
struct FieldDataDescription {
  ElementRole role = ElementRole::NONE;
  bool is_focusable = true;
  bool is_enabled = true;
  bool is_readonly = false;
  autofill::FieldPropertiesMask properties_mask =
      FieldPropertiesFlags::NO_FLAGS;
  const char* autocomplete_attribute = nullptr;
  const char* value = kNonimportantValue;
  const char* form_control_type = "text";
  PasswordFieldPrediction prediction = {.type = autofill::MAX_VALID_FIELD_TYPE};
};

// Describes a test case for the parser.
struct FormParsingTestCase {
  const char* description_for_logging;
  std::vector<FieldDataDescription> fields;
};

// Returns numbers which are distinct from each other within the scope of one
// test.
uint32_t GetUniqueId() {
  static uint32_t counter = 0;
  return counter++;
}

// Use to add a number suffix which is unique in the scope of the test.
base::string16 StampUniqueSuffix(const char* base_str) {
  return ASCIIToUTF16(base_str) + ASCIIToUTF16("_") +
         base::UintToString16(GetUniqueId());
}

// Describes which renderer IDs are expected for username/password fields
// identified in a PasswordForm.
struct ParseResultIds {
  uint32_t username_id = FormFieldData::kNotSetFormControlRendererId;
  uint32_t password_id = FormFieldData::kNotSetFormControlRendererId;
  uint32_t new_password_id = FormFieldData::kNotSetFormControlRendererId;
  uint32_t confirmation_password_id =
      FormFieldData::kNotSetFormControlRendererId;

  bool IsEmpty() const {
    return username_id == FormFieldData::kNotSetFormControlRendererId &&
           password_id == FormFieldData::kNotSetFormControlRendererId &&
           new_password_id == FormFieldData::kNotSetFormControlRendererId &&
           confirmation_password_id ==
               FormFieldData::kNotSetFormControlRendererId;
  }
};

// Creates a FormData to be fed to the parser. Includes FormFieldData as
// described in |fields_description|. Generates |fill_result| and |save_result|
// expectations about the result in FILLING and SAVING mode, respectively. Also
// fills |predictions| with the predictions contained in FieldDataDescriptions.
FormData GetFormDataAndExpectation(
    const std::vector<FieldDataDescription>& fields_description,
    FormPredictions* predictions,
    ParseResultIds* fill_result,
    ParseResultIds* save_result) {
  FormData form_data;
  form_data.action = GURL("http://example1.com");
  form_data.origin = GURL("http://example2.com");
  for (const FieldDataDescription& field_description : fields_description) {
    FormFieldData field;
    const uint32_t unique_id = GetUniqueId();
    field.unique_renderer_id = unique_id;
    field.id = StampUniqueSuffix("html_id");
    field.name = StampUniqueSuffix("html_name");
    field.form_control_type = field_description.form_control_type;
    field.is_focusable = field_description.is_focusable;
    field.is_enabled = field_description.is_enabled;
    field.is_readonly = field_description.is_readonly;
    field.properties_mask = field_description.properties_mask;
    if (field_description.value == kNonimportantValue) {
      field.value = StampUniqueSuffix("value");
    } else {
      field.value = ASCIIToUTF16(field_description.value);
    }
    if (field_description.autocomplete_attribute)
      field.autocomplete_attribute = field_description.autocomplete_attribute;
    form_data.fields.push_back(field);
    switch (field_description.role) {
      case ElementRole::NONE:
        break;
      case ElementRole::USERNAME_FILLING:
        fill_result->username_id = unique_id;
        break;
      case ElementRole::USERNAME_SAVING:
        save_result->username_id = unique_id;
        break;
      case ElementRole::USERNAME:
        fill_result->username_id = unique_id;
        save_result->username_id = unique_id;
        break;
      case ElementRole::CURRENT_PASSWORD_FILLING:
        fill_result->password_id = unique_id;
        break;
      case ElementRole::CURRENT_PASSWORD_SAVING:
        save_result->password_id = unique_id;
        break;
      case ElementRole::CURRENT_PASSWORD:
        fill_result->password_id = unique_id;
        save_result->password_id = unique_id;
        break;
      case ElementRole::NEW_PASSWORD_FILLING:
        fill_result->new_password_id = unique_id;
        break;
      case ElementRole::NEW_PASSWORD_SAVING:
        save_result->new_password_id = unique_id;
        break;
      case ElementRole::NEW_PASSWORD:
        fill_result->new_password_id = unique_id;
        save_result->new_password_id = unique_id;
        break;
      case ElementRole::CONFIRMATION_PASSWORD_FILLING:
        fill_result->confirmation_password_id = unique_id;
        break;
      case ElementRole::CONFIRMATION_PASSWORD_SAVING:
        save_result->confirmation_password_id = unique_id;
        break;
      case ElementRole::CONFIRMATION_PASSWORD:
        fill_result->confirmation_password_id = unique_id;
        save_result->confirmation_password_id = unique_id;
        break;
    }
    if (field_description.prediction.type != autofill::MAX_VALID_FIELD_TYPE) {
      (*predictions)[unique_id] = field_description.prediction;
    }
  }
  return form_data;
}

// Check that |fields| has a field with unique renderer ID |renderer_id| which
// has the name |element_name| and value |*element_value|. If |renderer_id| is
// FormFieldData::kNotSetFormControlRendererId, then instead check that
// |element_name| and |*element_value| are empty. Set |element_kind| to identify
// the type of the field in logging: 'username', 'password', etc. The argument
// |element_value| can be null, in which case all checks involving it are
// skipped (useful for the confirmation password value, which is not represented
// in PasswordForm).
void CheckField(const std::vector<FormFieldData>& fields,
                uint32_t renderer_id,
                const base::string16& element_name,
                const base::string16* element_value,
                const char* element_kind) {
  SCOPED_TRACE(testing::Message("Looking for element of kind ")
               << element_kind);

  if (renderer_id == FormFieldData::kNotSetFormControlRendererId) {
    EXPECT_EQ(base::string16(), element_name);
    if (element_value)
      EXPECT_EQ(base::string16(), *element_value);
    return;
  }

  auto field_it = std::find_if(fields.begin(), fields.end(),
                               [renderer_id](const FormFieldData& field) {
                                 return field.unique_renderer_id == renderer_id;
                               });
  ASSERT_TRUE(field_it != fields.end())
      << "Could not find a field with renderer ID " << renderer_id;
  EXPECT_EQ(element_name, field_it->name);
  if (element_value)
    EXPECT_EQ(*element_value, field_it->value);
}

// Check that the information distilled from |form_data| into |password_form| is
// matching |expectations|.
void CheckPasswordFormFields(const PasswordForm& password_form,
                             const FormData& form_data,
                             const ParseResultIds& expectations) {
  CheckField(form_data.fields, expectations.username_id,
             password_form.username_element, &password_form.username_value,
             "username");

  CheckField(form_data.fields, expectations.password_id,
             password_form.password_element, &password_form.password_value,
             "password");

  CheckField(form_data.fields, expectations.new_password_id,
             password_form.new_password_element,
             &password_form.new_password_value, "new_password");

  CheckField(form_data.fields, expectations.confirmation_password_id,
             password_form.confirmation_password_element, nullptr,
             "confirmation_password");
}

// Iterates over |test_cases|, creates a FormData for each, runs the parser and
// checks the results.
void CheckTestData(const std::vector<FormParsingTestCase>& test_cases) {
  for (const FormParsingTestCase& test_case : test_cases) {
    FormPredictions predictions;
    ParseResultIds fill_result;
    ParseResultIds save_result;
    const FormData form_data = GetFormDataAndExpectation(
        test_case.fields, &predictions, &fill_result, &save_result);
    for (auto mode : {FormParsingMode::FILLING, FormParsingMode::SAVING}) {
      SCOPED_TRACE(
          testing::Message("Test description: ")
          << test_case.description_for_logging << ", parsing mode = "
          << (mode == FormParsingMode::FILLING ? "Filling" : "Saving"));

      std::unique_ptr<PasswordForm> parsed_form =
          ParseFormData(form_data, &predictions, mode);

      const ParseResultIds& expected_ids =
          mode == FormParsingMode::FILLING ? fill_result : save_result;

      if (expected_ids.IsEmpty()) {
        EXPECT_FALSE(parsed_form) << "Expected no parsed results";
      } else {
        ASSERT_TRUE(parsed_form) << "Expected successful parsing";
        CheckPasswordFormFields(*parsed_form, form_data, expected_ids);
      }
    }
  }
}

TEST(FormParserTest, NotPasswordForm) {
  CheckTestData({
      {
          "No fields", {},
      },
      {
          "No password fields",
          {
              {.form_control_type = "text"}, {.form_control_type = "text"},
          },
      },
  });
}

TEST(FormParserTest, SkipNotTextFields) {
  CheckTestData({
      {
          "Select between username and password fields",
          {
              {.role = ElementRole::USERNAME},
              {.form_control_type = "select"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
  });
}

TEST(FormParserTest, OnlyPasswordFields) {
  CheckTestData({
      {
          "1 password field",
          {
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "2 password fields, new and confirmation password",
          {
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .value = "pw"},
              {.role = ElementRole::CONFIRMATION_PASSWORD,
               .form_control_type = "password",
               .value = "pw"},
          },
      },
      {
          "2 password fields, current and new password",
          {
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .value = "pw1"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .value = "pw2"},
          },
      },
      {
          "3 password fields, current, new, confirm password",
          {
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .value = "pw1"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .value = "pw2"},
              {.role = ElementRole::CONFIRMATION_PASSWORD,
               .form_control_type = "password",
               .value = "pw2"},
          },
      },
      {
          "3 password fields with different values",
          {
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .value = "pw1"},
              {.form_control_type = "password", .value = "pw2"},
              {.form_control_type = "password", .value = "pw3"},
          },
      },
      {
          "4 password fields, only the first 3 are considered",
          {
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .value = "pw1"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .value = "pw2"},
              {.role = ElementRole::CONFIRMATION_PASSWORD,
               .form_control_type = "password",
               .value = "pw2"},
              {.form_control_type = "password", .value = "pw3"},
          },
      },
      {
          "4 password fields, 4th same value as 3rd and 2nd, only the first 3 "
          "are considered",
          {
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .value = "pw1"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .value = "pw2"},
              {.role = ElementRole::CONFIRMATION_PASSWORD,
               .form_control_type = "password",
               .value = "pw2"},
              {.form_control_type = "password", .value = "pw2"},
          },
      },
      {
          "4 password fields, all same value",
          {
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .value = "pw"},
              {.form_control_type = "password", .value = "pw"},
              {.form_control_type = "password", .value = "pw"},
              {.form_control_type = "password", .value = "pw"},
          },
      },
  });
}

TEST(FormParserTest, TestFocusability) {
  CheckTestData({
      {
          "non-focusable fields are considered when there are no focusable "
          "fields",
          {
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .is_focusable = false},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .is_focusable = false},
          },
      },
      {
          "non-focusable should be skipped when there are focusable fields",
          {
              {.form_control_type = "password", .is_focusable = false},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .is_focusable = true},
          },
      },
      {
          "non-focusable text fields before password",
          {
              {.form_control_type = "text", .is_focusable = false},
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .is_focusable = false},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .is_focusable = true},
          },
      },
      {
          "focusable and non-focusable text fields before password",
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .is_focusable = true},
              {.form_control_type = "text", .is_focusable = false},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .is_focusable = true},
          },
      },
      {
          "many passwords, some of them focusable",
          {
              {.form_control_type = "password", .is_focusable = false},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .is_focusable = true},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .is_focusable = true,
               .value = "pw"},
              {.form_control_type = "password", .is_focusable = false},
              {.form_control_type = "password", .is_focusable = false},
              {.form_control_type = "password", .is_focusable = false},
              {.form_control_type = "password", .is_focusable = false},
              {.role = ElementRole::CONFIRMATION_PASSWORD,
               .form_control_type = "password",
               .is_focusable = true,
               .value = "pw"},
              {.form_control_type = "password", .is_focusable = false},
              {.form_control_type = "password", .is_focusable = false},
          },
      },
  });
}

TEST(FormParserTest, TextAndPasswordFields) {
  CheckTestData({
      {
          "Simple empty sign-in form",
          // Forms with empty fields cannot be saved, so the parsing result for
          // saving is empty.
          {
              {.role = ElementRole::USERNAME_FILLING,
               .form_control_type = "text",
               .value = ""},
              {.role = ElementRole::CURRENT_PASSWORD_FILLING,
               .form_control_type = "password",
               .value = ""},
          },
      },
      {
          "Simple sign-in form with filled data",
          {
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Empty sign-in form with an extra text field",
          {
              {.form_control_type = "text", .value = ""},
              {.role = ElementRole::USERNAME_FILLING,
               .form_control_type = "text",
               .value = ""},
              {.role = ElementRole::CURRENT_PASSWORD_FILLING,
               .form_control_type = "password",
               .value = ""},
          },
      },
      {
          "Non-empty sign-in form with an extra text field",
          {
              {.role = ElementRole::USERNAME_SAVING,
               .form_control_type = "text"},
              {.role = ElementRole::USERNAME_FILLING,
               .form_control_type = "text",
               .value = ""},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Empty sign-in form with an extra invisible text field",
          {
              {.role = ElementRole::USERNAME_FILLING,
               .form_control_type = "text",
               .value = ""},
              {.form_control_type = "text", .is_focusable = false, .value = ""},
              {.role = ElementRole::CURRENT_PASSWORD_FILLING,
               .form_control_type = "password",
               .value = ""},
          },
      },
      {
          "Non-empty sign-in form with an extra invisible text field",
          {
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.form_control_type = "text", .is_focusable = false},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Simple empty sign-in form with empty username",
          // Filled forms with a username field which is left empty are
          // suspicious. The parser will just omit the username altogether.
          {
              {.role = ElementRole::USERNAME_FILLING,
               .form_control_type = "text",
               .value = ""},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Simple empty sign-in form with empty password",
          // Empty password, nothing to save.
          {
              {.role = ElementRole::USERNAME_FILLING,
               .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD_FILLING,
               .form_control_type = "password",
               .value = ""},
          },
      },
  });
}

TEST(FormParserTest, TestAutocomplete) {
  CheckTestData({
      {
          "All possible password autocomplete attributes and some fields "
          "without autocomplete",
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .autocomplete_attribute = "username"},
              {.form_control_type = "text"},
              {.form_control_type = "password"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .autocomplete_attribute = "current-password"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .autocomplete_attribute = "new-password",
               .value = "np"},
              {.form_control_type = "password"},
              {.role = ElementRole::CONFIRMATION_PASSWORD,
               .form_control_type = "password",
               .autocomplete_attribute = "new-password",
               .value = "np"},
          },
      },
      {
          "Non-password autocomplete attributes are skipped",
          {
              {.form_control_type = "text", .autocomplete_attribute = "email"},
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .value = "pw"},
              {.role = ElementRole::CONFIRMATION_PASSWORD,
               .form_control_type = "password",
               .value = "pw"},
              // NB: 'password' is not a valid autocomplete type hint.
              {.form_control_type = "password",
               .autocomplete_attribute = "password"},
          },
      },
      {
          "Basic heuristics kick in if autocomplete analysis fails",
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .autocomplete_attribute = "email"},
              // NB: 'password' is not a valid autocomplete type hint.
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .autocomplete_attribute = "password"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Partial autocomplete analysis is not complemented by basic "
          "heuristics",
          // Username not found because there was a valid autocomplete mark-up
          // but it did not include the plain text field.
          {
              {.form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .autocomplete_attribute = "current-password"},
          },
      },
      {
          "Partial autocomplete analysis fails if no passwords are found",
          // The attribute 'username' is ignored, because there was no password
          // marked up.
          {
              {.form_control_type = "text",
               .autocomplete_attribute = "username"},
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Multiple username autocomplete attributes, fallback to base "
          "heuristics",
          {
              {.form_control_type = "text",
               .autocomplete_attribute = "username"},
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .autocomplete_attribute = "username"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .autocomplete_attribute = "current-password"},
          },
      },
      {
          "Parsing complex autocomplete attributes",
          {
              // Valid information about form sections, in addition to the
              // username hint.
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .autocomplete_attribute = "section-test billing username"},
              {.form_control_type = "text"},
              // Invalid composition, but the parser is simplistic and just
              // grabs the last token.
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .autocomplete_attribute = "new-password current-password"},
              {.form_control_type = "password"},
          },
      },
      {
          "Ignored autocomplete attributes",
          {
              // 'off' is ignored.
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .autocomplete_attribute = "off"},
              // Invalid composition, the parser ignores all but the last token.
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .autocomplete_attribute = "new-password abc"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Swapped username/password autocomplete attributes",
          // Swap means ignoring autocomplete analysis and falling back to basic
          // heuristics.
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .autocomplete_attribute = "current-password"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .autocomplete_attribute = "username"},
          },
      },
      {
          "Autocomplete mark-up overrides visibility",
          {
              {.role = ElementRole::USERNAME,
               .is_focusable = false,
               .form_control_type = "text",
               .autocomplete_attribute = "username"},
              {.is_focusable = true, .form_control_type = "text"},
              {.is_focusable = true, .form_control_type = "password"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .is_focusable = false,
               .autocomplete_attribute = "current-password"},
          },
      },
  });
}

TEST(FormParserTest, DisabledFields) {
  CheckTestData({
      {
          "The disabled attribute is ignored",
          {
              {.is_enabled = true, .form_control_type = "text"},
              {.role = ElementRole::USERNAME,
               .is_enabled = false,
               .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password",
               .is_enabled = false},
              {.role = ElementRole::NEW_PASSWORD,
               .form_control_type = "password",
               .is_enabled = true},
          },
      },
  });
}

TEST(FormParserTest, SkippingFieldsWithCreditCardFields) {
  CheckTestData({
      {
          "Simple form, all fields are credit-card-related",
          {
              {.form_control_type = "text",
               .autocomplete_attribute = "cc-name"},
              {.form_control_type = "password",
               .autocomplete_attribute = "cc-any-string"},
          },
      },
      {
          "Non-CC fields are considered",
          {
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.form_control_type = "text",
               .autocomplete_attribute = "cc-name"},
              {.form_control_type = "password",
               .autocomplete_attribute = "cc-any-string"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
  });
}

TEST(FormParserTest, ReadonlyFields) {
  CheckTestData({
      {
          "For usernames, readonly does not matter",
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .is_readonly = true},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "For passwords, readonly means: 'give up', perhaps there is a "
          "virtual keyboard, filling might be ignored",
          {
              {.form_control_type = "text"},
              {.form_control_type = "password", .is_readonly = true},
          },
      },
      {
          "But correctly marked passwords are accepted even if readonly",
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .autocomplete_attribute = "username"},
              {.role = ElementRole::NEW_PASSWORD,
               .autocomplete_attribute = "new-password",
               .form_control_type = "password",
               .is_readonly = true},
              {.role = ElementRole::CONFIRMATION_PASSWORD,
               .autocomplete_attribute = "new-password",
               .form_control_type = "password",
               .is_readonly = true},
              {.role = ElementRole::CURRENT_PASSWORD,
               .autocomplete_attribute = "current-password",
               .form_control_type = "password",
               .is_readonly = true},
          },
      },
      {
          "And passwords already filled by user or Chrome are accepted even if "
          "readonly",
          {
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .properties_mask = FieldPropertiesFlags::AUTOFILLED,
               .form_control_type = "password",
               .is_readonly = true},
              {.role = ElementRole::NEW_PASSWORD,
               .properties_mask = FieldPropertiesFlags::USER_TYPED,
               .form_control_type = "password",
               .is_readonly = true},
              {.form_control_type = "password", .is_readonly = true},
          },
      },
  });
}

TEST(FormParserTest, ServerHints) {
  CheckTestData({
      {
          "Empty predictions don't cause panic",
          {
              {.form_control_type = "text"},
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Username-only predictions are ignored",
          {
              {.form_control_type = "text",
               .prediction = {.type = autofill::USERNAME}},
              {.role = ElementRole::USERNAME, .form_control_type = "text"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .form_control_type = "password"},
          },
      },
      {
          "Simple predictions work",
          {
              {.role = ElementRole::USERNAME,
               .form_control_type = "text",
               .prediction = {.type = autofill::USERNAME_AND_EMAIL_ADDRESS}},
              {.form_control_type = "text"},
              {.form_control_type = "password"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .prediction = {.type = autofill::PASSWORD},
               .form_control_type = "password"},
          },
      },
      {
          "Longer predictions work",
          {
              {.role = ElementRole::USERNAME,
               .prediction = {.type = autofill::USERNAME},
               .form_control_type = "text"},
              {.form_control_type = "text"},
              {.form_control_type = "password"},
              {.role = ElementRole::NEW_PASSWORD,
               .prediction = {.type = autofill::ACCOUNT_CREATION_PASSWORD},
               .form_control_type = "password"},
              {.role = ElementRole::CONFIRMATION_PASSWORD,
               .prediction = {.type = autofill::CONFIRMATION_PASSWORD},
               .form_control_type = "password"},
              {.role = ElementRole::CURRENT_PASSWORD,
               .prediction = {.type = autofill::PASSWORD},
               .form_control_type = "password"},
          },
      },
  });
}

}  // namespace

}  // namespace password_manager
