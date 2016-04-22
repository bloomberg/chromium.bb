// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_field.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::StringToInt;
using base::UTF8ToUTF16;

namespace autofill {
namespace {

const std::vector<const char*> NotNumericMonthsContentsNoPlaceholder()
{
  const std::vector<const char*> result = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  return result;
}

const std::vector<const char*> NotNumericMonthsContentsWithPlaceholder()
{
  const std::vector<const char*> result = {
    "Select a Month",
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec"};
  return result;
}

// Returns the index of |value| in |values|.
void GetIndexOfValue(const std::vector<base::string16>& values,
                     const base::string16& value,
                     size_t* index) {
  for (size_t i = 0; i < values.size(); ++i) {
    if (values[i] == value) {
      *index = i;
      return;
    }
  }
  ASSERT_TRUE(false) << "Passing invalid arguments to GetIndexOfValue";
}

// Creates the select field from the specified |values| and |contents| and tests
// filling with 3 different values.
void TestFillingExpirationMonth(const std::vector<const char*>& values,
                                const std::vector<const char*>& contents,
                                size_t select_size) {
  // Create the select field.
  AutofillField field;
  test::CreateTestSelectField("", "", "", values, contents, select_size,
                              &field);
  field.set_heuristic_type(CREDIT_CARD_EXP_MONTH);

  size_t content_index = 0;

  // Try with a leading zero.
  AutofillField::FillFormField(field, ASCIIToUTF16("03"), "en-US", "en-US",
                               &field);
  GetIndexOfValue(field.option_values, field.value, &content_index);
  EXPECT_EQ(ASCIIToUTF16("Mar"), field.option_contents[content_index]);

  // Try without a leading zero.
  AutofillField::FillFormField(field, ASCIIToUTF16("4"), "en-US", "en-US",
                               &field);
  GetIndexOfValue(field.option_values, field.value, &content_index);
  EXPECT_EQ(ASCIIToUTF16("Apr"), field.option_contents[content_index]);

  // Try a two-digit month.
  AutofillField::FillFormField(field, ASCIIToUTF16("11"), "en-US", "en-US",
                               &field);
  GetIndexOfValue(field.option_values, field.value, &content_index);
  EXPECT_EQ(ASCIIToUTF16("Nov"), field.option_contents[content_index]);
}

struct TestCase {
  std::string card_number_;
  size_t total_spilts_;
  std::vector<int> splits_;
  std::vector<std::string> expected_results_;
};

// Returns the offset to be set within the credit card number field.
size_t GetNumberOffset(size_t index, const TestCase& test) {
  size_t result = 0;
  for (size_t i = 0; i < index; ++i)
    result += test.splits_[i];
  return result;
}

class AutofillFieldTest : public testing::Test {
 public:
  AutofillFieldTest() { CountryNames::SetLocaleString("en-US"); }
};

TEST_F(AutofillFieldTest, Type) {
  AutofillField field;
  ASSERT_EQ(NO_SERVER_DATA, field.server_type());
  ASSERT_EQ(UNKNOWN_TYPE, field.heuristic_type());

  // |server_type_| is NO_SERVER_DATA, so |heuristic_type_| is returned.
  EXPECT_EQ(UNKNOWN_TYPE, field.Type().GetStorableType());

  // Set the heuristic type and check it.
  field.set_heuristic_type(NAME_FIRST);
  EXPECT_EQ(NAME_FIRST, field.Type().GetStorableType());
  EXPECT_EQ(NAME, field.Type().group());

  // Set the server type and check it.
  field.set_server_type(ADDRESS_BILLING_LINE1);
  EXPECT_EQ(ADDRESS_HOME_LINE1, field.Type().GetStorableType());
  EXPECT_EQ(ADDRESS_BILLING, field.Type().group());

  // Remove the server type to make sure the heuristic type is preserved.
  field.set_server_type(NO_SERVER_DATA);
  EXPECT_EQ(NAME_FIRST, field.Type().GetStorableType());
  EXPECT_EQ(NAME, field.Type().group());
}

// Tests that a credit card related prediction made by the heuristics overrides
// an unrecognized autocomplete attribute.
TEST_F(AutofillFieldTest, Type_CreditCardOverrideHtml_Heuristics) {
  AutofillField field;

  field.SetHtmlType(HTML_TYPE_UNRECOGNIZED, HTML_MODE_NONE);

  // A credit card heuristic prediction overrides the unrecognized type.
  field.set_heuristic_type(CREDIT_CARD_NUMBER);
  EXPECT_EQ(CREDIT_CARD_NUMBER, field.Type().GetStorableType());

  // A non credit card heuristic prediction doesn't override the unrecognized
  // type.
  field.set_heuristic_type(NAME_FIRST);
  EXPECT_EQ(UNKNOWN_TYPE, field.Type().GetStorableType());

  // A credit card heuristic prediction doesn't override a known specified html
  // type.
  field.SetHtmlType(HTML_TYPE_NAME, HTML_MODE_NONE);
  field.set_heuristic_type(CREDIT_CARD_NUMBER);
  EXPECT_EQ(NAME_FULL, field.Type().GetStorableType());
}

// Tests that a credit card related prediction made by the server overrides an
// unrecognized autocomplete attribute.
TEST_F(AutofillFieldTest, Type_CreditCardOverrideHtml_ServerPredicitons) {
  AutofillField field;

  field.SetHtmlType(HTML_TYPE_UNRECOGNIZED, HTML_MODE_NONE);

  // A credit card server prediction overrides the unrecognized type.
  field.set_server_type(CREDIT_CARD_NUMBER);
  EXPECT_EQ(CREDIT_CARD_NUMBER, field.Type().GetStorableType());

  // A non credit card server prediction doesn't override the unrecognized
  // type.
  field.set_server_type(NAME_FIRST);
  EXPECT_EQ(UNKNOWN_TYPE, field.Type().GetStorableType());

  // A credit card server prediction doesn't override a known specified html
  // type.
  field.SetHtmlType(HTML_TYPE_NAME, HTML_MODE_NONE);
  field.set_server_type(CREDIT_CARD_NUMBER);
  EXPECT_EQ(NAME_FULL, field.Type().GetStorableType());
}

TEST_F(AutofillFieldTest, IsEmpty) {
  AutofillField field;
  ASSERT_EQ(base::string16(), field.value);

  // Field value is empty.
  EXPECT_TRUE(field.IsEmpty());

  // Field value is non-empty.
  field.value = ASCIIToUTF16("Value");
  EXPECT_FALSE(field.IsEmpty());
}

TEST_F(AutofillFieldTest, FieldSignature) {
  AutofillField field;
  ASSERT_EQ(base::string16(), field.name);
  ASSERT_EQ(std::string(), field.form_control_type);

  // Signature is empty.
  EXPECT_EQ("2085434232", field.FieldSignature());

  // Field name is set.
  field.name = ASCIIToUTF16("Name");
  EXPECT_EQ("1606968241", field.FieldSignature());

  // Field form control type is set.
  field.form_control_type = "text";
  EXPECT_EQ("502192749", field.FieldSignature());

  // Heuristic type does not affect FieldSignature.
  field.set_heuristic_type(NAME_FIRST);
  EXPECT_EQ("502192749", field.FieldSignature());

  // Server type does not affect FieldSignature.
  field.set_server_type(NAME_LAST);
  EXPECT_EQ("502192749", field.FieldSignature());
}

TEST_F(AutofillFieldTest, IsFieldFillable) {
  AutofillField field;
  ASSERT_EQ(UNKNOWN_TYPE, field.Type().GetStorableType());

  // Type is unknown.
  EXPECT_FALSE(field.IsFieldFillable());

  // Only heuristic type is set.
  field.set_heuristic_type(NAME_FIRST);
  EXPECT_TRUE(field.IsFieldFillable());

  // Only server type is set.
  field.set_heuristic_type(UNKNOWN_TYPE);
  field.set_server_type(NAME_LAST);
  EXPECT_TRUE(field.IsFieldFillable());

  // Both types set.
  field.set_heuristic_type(NAME_FIRST);
  field.set_server_type(NAME_LAST);
  EXPECT_TRUE(field.IsFieldFillable());

  // Field has autocomplete="off" set. Since autofill was able to make a
  // prediction, it is still considered a fillable field.
  field.should_autocomplete = false;
  EXPECT_TRUE(field.IsFieldFillable());
}

// Verify that non credit card related fields with the autocomplete attribute
// set to off don't get filled on desktop.
TEST_F(AutofillFieldTest, FillFormField_AutocompleteOff_AddressField) {
  AutofillField field;
  field.should_autocomplete = false;

  // Non credit card related field.
  AutofillField::FillFormField(field, ASCIIToUTF16("Test"), "en-US", "en-US",
                               &field);

  // Verifiy that the field is filled on mobile but not on desktop.
  if (IsDesktopPlatform()) {
    EXPECT_EQ(base::string16(), field.value);
  } else {
    EXPECT_EQ(ASCIIToUTF16("Test"), field.value);
  }
}

// Verify that credit card related fields with the autocomplete attribute
// set to off get filled.
TEST_F(AutofillFieldTest, FillFormField_AutocompleteOff_CreditCardField) {
  AutofillField field;
  field.should_autocomplete = false;

  // Credit card related field.
  field.set_heuristic_type(CREDIT_CARD_NUMBER);
  AutofillField::FillFormField(field, ASCIIToUTF16("4111111111111111"), "en-US",
                               "en-US", &field);

  // Verify that the field is filled.
  EXPECT_EQ(ASCIIToUTF16("4111111111111111"), field.value);
}

// TODO(crbug.com/581514): Add support for filling only the prefix/suffix for
// phone numbers with 10 or 11 digits.
TEST_F(AutofillFieldTest, FillPhoneNumber) {
  typedef struct {
    HtmlFieldType field_type;
    size_t field_max_length;
    std::string value_to_fill;
    std::string expected_value;

  } TestCase;

  TestCase test_cases[] = {
      // Filling a phone type field with text should fill the text as is.
      {HTML_TYPE_TEL, /* default value */ 0, "Oh hai", "Oh hai"},
      // Filling a prefix type field with a phone number of 7 digits should just
      // fill the prefix.
      {HTML_TYPE_TEL_LOCAL_PREFIX, /* default value */ 0, "5551234", "555"},
      // Filling a suffix type field with a phone number of 7 digits should just
      // fill the suffix.
      {HTML_TYPE_TEL_LOCAL_SUFFIX, /* default value */ 0, "5551234", "1234"},
      // Filling a phone type field with a max length of 3 with a phone number
      // of
      // 7 digits should fill only the prefix.
      {HTML_TYPE_TEL, 3, "5551234", "555"},
      // Filling a phone type field with a max length of 4 with a phone number
      // of
      // 7 digits should fill only the suffix.
      {HTML_TYPE_TEL, 4, "5551234", "1234"},
      // Filling a phone type field with a max length of 10 with a phone number
      // including the country code should fill the phone number without the
      // country code.
      {HTML_TYPE_TEL, 10, "15141254578", "5141254578"},
      // Filling a phone type field with a max length of 5 with a phone number
      // should fill with the last 5 digits of that phone number.
      {HTML_TYPE_TEL, 5, "5141254578", "54578"}};

  for (TestCase test_case : test_cases) {
    AutofillField field;
    field.SetHtmlType(test_case.field_type, HtmlFieldMode());
    field.max_length = test_case.field_max_length;

    AutofillField::FillFormField(field, ASCIIToUTF16(test_case.value_to_fill),
                                 "en-US", "en-US", &field);
    EXPECT_EQ(ASCIIToUTF16(test_case.expected_value), field.value);
  }
}

TEST_F(AutofillFieldTest, FillSelectControlByValue) {
  std::vector<const char*> kOptions = {
      "Eenie", "Meenie", "Miney", "Mo",
  };

  AutofillField field;
  test::CreateTestSelectField(kOptions, &field);

  // Set semantically empty contents for each option, so that only the values
  // can be used for matching.
  for (size_t i = 0; i < field.option_contents.size(); ++i)
    field.option_contents[i] = base::SizeTToString16(i);

  AutofillField::FillFormField(
      field, ASCIIToUTF16("Meenie"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("Meenie"), field.value);
}

TEST_F(AutofillFieldTest, FillSelectControlByContents) {
  std::vector<const char*> kOptions = {
      "Eenie", "Meenie", "Miney", "Mo",
  };
  AutofillField field;
  test::CreateTestSelectField(kOptions, &field);

  // Set semantically empty values for each option, so that only the contents
  // can be used for matching.
  for (size_t i = 0; i < field.option_values.size(); ++i)
    field.option_values[i] = base::SizeTToString16(i);

  AutofillField::FillFormField(
      field, ASCIIToUTF16("Miney"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("2"), field.value);  // Corresponds to "Miney".
}

TEST_F(AutofillFieldTest, FillSelectControlWithFullCountryNames) {
  std::vector<const char*> kCountries = {"Albania", "Canada"};
  AutofillField field;
  test::CreateTestSelectField(kCountries, &field);
  field.set_heuristic_type(ADDRESS_HOME_COUNTRY);

  AutofillField::FillFormField(
      field, ASCIIToUTF16("CA"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("Canada"), field.value);
}

TEST_F(AutofillFieldTest, FillSelectControlWithAbbreviatedCountryNames) {
  std::vector<const char*> kCountries = {"AL", "CA"};
  AutofillField field;
  test::CreateTestSelectField(kCountries, &field);
  field.set_heuristic_type(ADDRESS_HOME_COUNTRY);

  AutofillField::FillFormField(
      field, ASCIIToUTF16("Canada"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("CA"), field.value);
}

TEST_F(AutofillFieldTest, FillSelectControlWithFullStateNames) {
  std::vector<const char*> kStates = {"Alabama", "California"};
  AutofillField field;
  test::CreateTestSelectField(kStates, &field);
  field.set_heuristic_type(ADDRESS_HOME_STATE);

  AutofillField::FillFormField(
      field, ASCIIToUTF16("CA"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("California"), field.value);
}

TEST_F(AutofillFieldTest, FillSelectControlWithAbbreviateStateNames) {
  std::vector<const char*> kStates = {"AL", "CA"};
  AutofillField field;
  test::CreateTestSelectField(kStates, &field);
  field.set_heuristic_type(ADDRESS_HOME_STATE);

  AutofillField::FillFormField(
      field, ASCIIToUTF16("California"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("CA"), field.value);
}

TEST_F(AutofillFieldTest, FillSelectControlWithInexactFullStateNames) {
  {
    std::vector<const char*> kStates = {
        "SC - South Carolina", "CA - California", "NC - North Carolina",
    };
    AutofillField field;
    test::CreateTestSelectField(kStates, &field);
    field.set_heuristic_type(ADDRESS_HOME_STATE);

    AutofillField::FillFormField(
        field, ASCIIToUTF16("California"), "en-US", "en-US", &field);
    EXPECT_EQ(ASCIIToUTF16("CA - California"), field.value);
  }

  // Don't accidentally match "Virginia" to "West Virginia".
  {
    std::vector<const char*> kStates = {
        "WV - West Virginia", "VA - Virginia", "NV - North Virginia",
    };
    AutofillField field;
    test::CreateTestSelectField(kStates, &field);
    field.set_heuristic_type(ADDRESS_HOME_STATE);

    AutofillField::FillFormField(
        field, ASCIIToUTF16("Virginia"), "en-US", "en-US", &field);
    EXPECT_EQ(ASCIIToUTF16("VA - Virginia"), field.value);
  }

  // Do accidentally match "Virginia" to "West Virginia". NB: Ideally, Chrome
  // would fail this test. It's here to document behavior rather than enforce
  // it.
  {
    std::vector<const char*> kStates = {
        "WV - West Virginia", "TX - Texas",
    };
    AutofillField field;
    test::CreateTestSelectField(kStates, &field);
    field.set_heuristic_type(ADDRESS_HOME_STATE);

    AutofillField::FillFormField(
        field, ASCIIToUTF16("Virginia"), "en-US", "en-US", &field);
    EXPECT_EQ(ASCIIToUTF16("WV - West Virginia"), field.value);
  }

  // Tests that substring matches work for full state names (a full token
  // match isn't required). Also tests that matches work for states with
  // whitespace in the middle.
  {
    std::vector<const char*> kStates = {
        "California.", "North Carolina.",
    };
    AutofillField field;
    test::CreateTestSelectField(kStates, &field);
    field.set_heuristic_type(ADDRESS_HOME_STATE);

    AutofillField::FillFormField(
        field, ASCIIToUTF16("North Carolina"), "en-US", "en-US", &field);
    EXPECT_EQ(ASCIIToUTF16("North Carolina."), field.value);
  }
}

TEST_F(AutofillFieldTest, FillSelectControlWithInexactAbbreviations) {
  {
    std::vector<const char*> kStates = {
        "NC - North Carolina", "CA - California",
    };
    AutofillField field;
    test::CreateTestSelectField(kStates, &field);
    field.set_heuristic_type(ADDRESS_HOME_STATE);

    AutofillField::FillFormField(
        field, ASCIIToUTF16("CA"), "en-US", "en-US", &field);
    EXPECT_EQ(ASCIIToUTF16("CA - California"), field.value);
  }

  {
    std::vector<const char*> kNotStates = {
        "NCNCA", "SCNCA",
    };
    AutofillField field;
    test::CreateTestSelectField(kNotStates, &field);
    field.set_heuristic_type(ADDRESS_HOME_STATE);

    AutofillField::FillFormField(
        field, ASCIIToUTF16("NC"), "en-US", "en-US", &field);
    EXPECT_EQ(base::string16(), field.value);
  }
}

TEST_F(AutofillFieldTest, FillSelectControlWithExpirationMonth) {
  typedef struct {
    std::vector<const char*> select_values;
    std::vector<const char*> select_contents;
  } TestCase;

  TestCase test_cases[] = {
      // Values start at 1.
      {{"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"},
       NotNumericMonthsContentsNoPlaceholder()},
      // Values start at 1 but single digits are whitespace padded!
      {{" 1", " 2", " 3", " 4", " 5", " 6", " 7", " 8", " 9", "10", "11", "12"},
       NotNumericMonthsContentsNoPlaceholder()},
      // Values start at 0.
      {{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"},
       NotNumericMonthsContentsNoPlaceholder()},
      // Values start at 00.
      {{"00", "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11"},
       NotNumericMonthsContentsNoPlaceholder()},
      // The AngularJS framework adds a prefix to number types. Test that it is
      // removed.
      {{"number:1", "number:2", "number:3", "number:4", "number:5", "number:6",
        "number:7", "number:8", "number:9", "number:10", "number:11",
        "number:12"},
       NotNumericMonthsContentsNoPlaceholder()},
      // Values start at 0 and the first content is a placeholder.
      {{"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"},
       NotNumericMonthsContentsWithPlaceholder()},
      // Values start at 1 and the first content is a placeholder.
      {{"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13"},
       NotNumericMonthsContentsWithPlaceholder()},
      // Values start at 01 and the first content is a placeholder.
      {{"01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12",
        "13"},
       NotNumericMonthsContentsWithPlaceholder()},
      // Values start at 0 after a placeholder.
      {{"?", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"},
       NotNumericMonthsContentsWithPlaceholder()},
      // Values start at 1 after a placeholder.
      {{"?", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"},
       NotNumericMonthsContentsWithPlaceholder()},
      // Values start at 0 after a negative number.
      {{"-1", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11"},
       NotNumericMonthsContentsWithPlaceholder()},
      // Values start at 1 after a negative number.
      {{"-1", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12"},
       NotNumericMonthsContentsWithPlaceholder()}};

  for (TestCase test_case : test_cases) {
    ASSERT_EQ(test_case.select_values.size(), test_case.select_contents.size());

    TestFillingExpirationMonth(test_case.select_values,
                               test_case.select_contents,
                               test_case.select_values.size());
  }
}

TEST_F(AutofillFieldTest, FillSelectControlWithAbbreviatedMonthName) {
  std::vector<const char*> kMonthsAbbreviated = {
      "Jan", "Feb", "Mar", "Apr", "May", "Jun",
      "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
  };
  AutofillField field;
  test::CreateTestSelectField(kMonthsAbbreviated, &field);
  field.set_heuristic_type(CREDIT_CARD_EXP_MONTH);

  AutofillField::FillFormField(
      field, ASCIIToUTF16("04"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("Apr"), field.value);
}

TEST_F(AutofillFieldTest, FillSelectControlWithFullMonthName) {
  std::vector<const char*> kMonthsFull = {
      "January", "February", "March",     "April",   "May",      "June",
      "July",    "August",   "September", "October", "November", "December",
  };
  AutofillField field;
  test::CreateTestSelectField(kMonthsFull, &field);
  field.set_heuristic_type(CREDIT_CARD_EXP_MONTH);

  AutofillField::FillFormField(
      field, ASCIIToUTF16("04"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("April"), field.value);
}

TEST_F(AutofillFieldTest, FillSelectControlWithFrenchMonthName) {
  std::vector<const char*> kMonthsFrench = {"JANV", "FÉVR.", "MARS",
                                            "décembre"};
  AutofillField field;
  test::CreateTestSelectField(kMonthsFrench, &field);
  field.set_heuristic_type(CREDIT_CARD_EXP_MONTH);

  AutofillField::FillFormField(
      field, ASCIIToUTF16("02"), "fr-FR", "fr-FR", &field);
  EXPECT_EQ(UTF8ToUTF16("FÉVR."), field.value);

  AutofillField::FillFormField(
      field, ASCIIToUTF16("01"), "fr-FR", "fr-FR", &field);
  EXPECT_EQ(UTF8ToUTF16("JANV"), field.value);

  AutofillField::FillFormField(
      field, ASCIIToUTF16("12"), "fr-FR", "fr-FR", &field);
  EXPECT_EQ(UTF8ToUTF16("décembre"), field.value);
}

TEST_F(AutofillFieldTest, FillSelectControlWithNumericMonthSansLeadingZero) {
  std::vector<const char*> kMonthsNumeric = {
      "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
  };
  AutofillField field;
  test::CreateTestSelectField(kMonthsNumeric, &field);
  field.set_heuristic_type(CREDIT_CARD_EXP_MONTH);

  AutofillField::FillFormField(
      field, ASCIIToUTF16("04"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("4"), field.value);
}

TEST_F(AutofillFieldTest, FillSelectControlWithTwoDigitCreditCardYear) {
  std::vector<const char*> kYears = {"12", "13", "14", "15",
                                     "16", "17", "18", "19"};
  AutofillField field;
  test::CreateTestSelectField(kYears, &field);
  field.set_heuristic_type(CREDIT_CARD_EXP_2_DIGIT_YEAR);

  AutofillField::FillFormField(
      field, ASCIIToUTF16("2017"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("17"), field.value);
}

TEST_F(AutofillFieldTest, FillSelectControlWithCreditCardType) {
  std::vector<const char*> kCreditCardTypes = {"Visa", "Master Card", "AmEx",
                                               "discover"};
  AutofillField field;
  test::CreateTestSelectField(kCreditCardTypes, &field);
  field.set_heuristic_type(CREDIT_CARD_TYPE);

  // Normal case:
  AutofillField::FillFormField(
      field, ASCIIToUTF16("Visa"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("Visa"), field.value);

  // Filling should be able to handle intervening whitespace:
  AutofillField::FillFormField(
      field, ASCIIToUTF16("MasterCard"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("Master Card"), field.value);

  // American Express is sometimes abbreviated as AmEx:
  AutofillField::FillFormField(
      field, ASCIIToUTF16("American Express"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("AmEx"), field.value);

  // Case insensitivity:
  AutofillField::FillFormField(
      field, ASCIIToUTF16("Discover"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("discover"), field.value);
}

TEST_F(AutofillFieldTest, FillMonthControl) {
  AutofillField field;
  field.form_control_type = "month";

  // Try a month with two digits.
  AutofillField::FillFormField(
      field, ASCIIToUTF16("12/2017"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("2017-12"), field.value);

  // Try a month with a leading zero.
  AutofillField::FillFormField(
      field, ASCIIToUTF16("03/2019"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("2019-03"), field.value);

  // Try a month without a leading zero.
  AutofillField::FillFormField(
      field, ASCIIToUTF16("4/2018"), "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("2018-04"), field.value);
}

TEST_F(AutofillFieldTest, FillStreetAddressTextArea) {
  AutofillField field;
  field.form_control_type = "textarea";

  base::string16 value = ASCIIToUTF16("123 Fake St.\n"
                                      "Apt. 42");
  AutofillField::FillFormField(field, value, "en-US", "en-US", &field);
  EXPECT_EQ(value, field.value);

  base::string16 ja_value = UTF8ToUTF16("桜丘町26-1\n"
                                        "セルリアンタワー6階");
  AutofillField::FillFormField(field, ja_value, "ja-JP", "en-US", &field);
  EXPECT_EQ(ja_value, field.value);
}

TEST_F(AutofillFieldTest, FillStreetAddressTextField) {
  AutofillField field;
  field.form_control_type = "text";
  field.set_server_type(ADDRESS_HOME_STREET_ADDRESS);

  base::string16 value = ASCIIToUTF16("123 Fake St.\n"
                                      "Apt. 42");
  AutofillField::FillFormField(field, value, "en-US", "en-US", &field);
  EXPECT_EQ(ASCIIToUTF16("123 Fake St., Apt. 42"), field.value);

  AutofillField::FillFormField(field,
                               UTF8ToUTF16("桜丘町26-1\n"
                                           "セルリアンタワー6階"),
                               "ja-JP",
                               "en-US",
                               &field);
  EXPECT_EQ(UTF8ToUTF16("桜丘町26-1セルリアンタワー6階"), field.value);
}

TEST_F(AutofillFieldTest, FillCreditCardNumberWithoutSplits) {
  // Case 1: card number without any spilt.
  AutofillField cc_number_full;
  cc_number_full.set_heuristic_type(CREDIT_CARD_NUMBER);
  AutofillField::FillFormField(cc_number_full,
                               ASCIIToUTF16("4111111111111111"),
                               "en-US",
                               "en-US",
                               &cc_number_full);

  // Verify that full card-number shall get fill properly.
  EXPECT_EQ(ASCIIToUTF16("4111111111111111"), cc_number_full.value);
  EXPECT_EQ(0U, cc_number_full.credit_card_number_offset());
}

TEST_F(AutofillFieldTest, FillCreditCardNumberWithEqualSizeSplits) {
  // Case 2: card number broken up into four equal groups, of length 4.
  TestCase test;
  test.card_number_ = "5187654321098765";
  test.total_spilts_ = 4;
  int splits[] = {4, 4, 4, 4};
  test.splits_ = std::vector<int>(splits, splits + arraysize(splits));
  std::string results[] = {"5187", "6543", "2109", "8765"};
  test.expected_results_ =
      std::vector<std::string>(results, results + arraysize(results));

  for (size_t i = 0; i < test.total_spilts_; ++i) {
    AutofillField cc_number_part;
    cc_number_part.set_heuristic_type(CREDIT_CARD_NUMBER);
    cc_number_part.max_length = test.splits_[i];
    cc_number_part.set_credit_card_number_offset(4 * i);

    // Fill with a card-number; should fill just the card_number_part.
    AutofillField::FillFormField(cc_number_part,
                                 ASCIIToUTF16(test.card_number_),
                                 "en-US",
                                 "en-US",
                                 &cc_number_part);

    // Verify for expected results.
    EXPECT_EQ(ASCIIToUTF16(test.expected_results_[i]),
              cc_number_part.value.substr(0, cc_number_part.max_length));
    EXPECT_EQ(4 * i, cc_number_part.credit_card_number_offset());
  }

  // Verify that full card-number shall get fill properly as well.
  AutofillField cc_number_full;
  cc_number_full.set_heuristic_type(CREDIT_CARD_NUMBER);
  AutofillField::FillFormField(cc_number_full,
                               ASCIIToUTF16(test.card_number_),
                               "en-US",
                               "en-US",
                               &cc_number_full);

  // Verify for expected results.
  EXPECT_EQ(ASCIIToUTF16(test.card_number_), cc_number_full.value);
}

TEST_F(AutofillFieldTest, FillCreditCardNumberWithUnequalSizeSplits) {
  // Case 3: card with 15 digits number, broken up into three unequal groups, of
  // lengths 4, 6, and 5.
  TestCase test;
  test.card_number_ = "423456789012345";
  test.total_spilts_ = 3;
  int splits[] = {4, 6, 5};
  test.splits_ = std::vector<int>(splits, splits + arraysize(splits));
  std::string results[] = {"4234", "567890", "12345"};
  test.expected_results_ =
      std::vector<std::string>(results, results + arraysize(results));

  // Start executing test cases to verify parts and full credit card number.
  for (size_t i = 0; i < test.total_spilts_; ++i) {
    AutofillField cc_number_part;
    cc_number_part.set_heuristic_type(CREDIT_CARD_NUMBER);
    cc_number_part.max_length = test.splits_[i];
    cc_number_part.set_credit_card_number_offset(GetNumberOffset(i, test));

    // Fill with a card-number; should fill just the card_number_part.
    AutofillField::FillFormField(cc_number_part,
                                 ASCIIToUTF16(test.card_number_),
                                 "en-US",
                                 "en-US",
                                 &cc_number_part);

    // Verify for expected results.
    EXPECT_EQ(ASCIIToUTF16(test.expected_results_[i]),
              cc_number_part.value.substr(0, cc_number_part.max_length));
    EXPECT_EQ(GetNumberOffset(i, test),
              cc_number_part.credit_card_number_offset());
  }

  // Verify that full card-number shall get fill properly as well.
  AutofillField cc_number_full;
  cc_number_full.set_heuristic_type(CREDIT_CARD_NUMBER);
  AutofillField::FillFormField(cc_number_full,
                               ASCIIToUTF16(test.card_number_),
                               "en-US",
                               "en-US",
                               &cc_number_full);

  // Verify for expected results.
  EXPECT_EQ(ASCIIToUTF16(test.card_number_), cc_number_full.value);
}

TEST_F(AutofillFieldTest, FindShortestSubstringMatchInSelect) {
  std::vector<const char*> kCountries = {"États-Unis", "Canada"};
  AutofillField field;
  test::CreateTestSelectField(kCountries, &field);

  // Case 1: Exact match
  int ret = AutofillField::FindShortestSubstringMatchInSelect(
      ASCIIToUTF16("Canada"), false, &field);
  EXPECT_EQ(1, ret);

  // Case 2: Case-insensitive
  ret = AutofillField::FindShortestSubstringMatchInSelect(
      ASCIIToUTF16("CANADA"), false, &field);
  EXPECT_EQ(1, ret);

  // Case 3: Proper substring
  ret = AutofillField::FindShortestSubstringMatchInSelect(UTF8ToUTF16("États"),
                                                          false, &field);
  EXPECT_EQ(0, ret);

  // Case 4: Accent-insensitive
  ret = AutofillField::FindShortestSubstringMatchInSelect(
      UTF8ToUTF16("Etats-Unis"), false, &field);
  EXPECT_EQ(0, ret);

  // Case 5: Whitespace-insensitive
  ret = AutofillField::FindShortestSubstringMatchInSelect(
      ASCIIToUTF16("Ca na da"), true, &field);
  EXPECT_EQ(1, ret);

  // Case 6: No match (whitespace-sensitive)
  ret = AutofillField::FindShortestSubstringMatchInSelect(
      ASCIIToUTF16("Ca Na Da"), false, &field);
  EXPECT_EQ(-1, ret);

  // Case 7: No match (not present)
  ret = AutofillField::FindShortestSubstringMatchInSelect(
      ASCIIToUTF16("Canadia"), true, &field);
  EXPECT_EQ(-1, ret);
}

// Tests that text state fields are filled correctly depending on their
// maxlength attribute value.
TEST_F(AutofillFieldTest, FillStateText) {
  typedef struct {
    HtmlFieldType field_type;
    size_t field_max_length;
    std::string value_to_fill;
    std::string expected_value;
    bool should_fill;
  } TestCase;

  TestCase test_cases[] = {
      // Filling a state to a text field with the default maxlength value should
      // fill the state value as is.
      {HTML_TYPE_ADDRESS_LEVEL1, /* default value */ 0, "New York", "New York",
       true},
      {HTML_TYPE_ADDRESS_LEVEL1, /* default value */ 0, "NY", "NY", true},
      // Filling a state to a text field with a maxlength value equal to the
      // value's length should fill the state value as is.
      {HTML_TYPE_ADDRESS_LEVEL1, 8, "New York", "New York", true},
      // Filling a state to a text field with a maxlength value lower than the
      // value's length but higher than the value's abbreviation should fill the
      // state abbreviation.
      {HTML_TYPE_ADDRESS_LEVEL1, 2, "New York", "NY", true},
      {HTML_TYPE_ADDRESS_LEVEL1, 2, "NY", "NY", true},
      // Filling a state to a text field with a maxlength value lower than the
      // value's length and the value's abbreviation should not fill at all.
      {HTML_TYPE_ADDRESS_LEVEL1, 1, "New York", "", false},
      {HTML_TYPE_ADDRESS_LEVEL1, 1, "NY", "", false},
      // Filling a state to a text field with a maxlength value lower than the
      // value's length and that has no associated abbreviation should not fill
      // at all.
      {HTML_TYPE_ADDRESS_LEVEL1, 3, "Quebec", "", false}};

  for (const TestCase& test_case : test_cases) {
    AutofillField field;
    field.SetHtmlType(test_case.field_type, HtmlFieldMode());
    field.max_length = test_case.field_max_length;

    bool has_filled = AutofillField::FillFormField(
        field, ASCIIToUTF16(test_case.value_to_fill), "en-US", "en-US", &field);

    EXPECT_EQ(test_case.should_fill, has_filled);
    EXPECT_EQ(ASCIIToUTF16(test_case.expected_value), field.value);
  }
}

}  // namespace
}  // namespace autofill
