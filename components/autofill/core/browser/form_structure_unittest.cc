// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_structure.h"

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/variations/entropy_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;

namespace autofill {

namespace content {

std::ostream& operator<<(std::ostream& os, const FormData& form) {
  os << base::UTF16ToUTF8(form.name)
     << " "
     << form.origin.spec()
     << " "
     << form.action.spec()
     << " ";

  for (std::vector<FormFieldData>::const_iterator iter =
           form.fields.begin();
       iter != form.fields.end(); ++iter) {
    os << *iter
       << " ";
  }

  return os;
}

}  // namespace content

class FormStructureTest : public testing::Test {
 public:
  static std::string Hash64Bit(const std::string& str) {
    return FormStructure::Hash64Bit(str);
  }

  void SetUp() override {
    // By default this trial is enabled on tests.
    EnableAutofillMetadataFieldTrial();
  }

 protected:
  void DisableAutofillMetadataFieldTrial() {
    field_trial_list_.reset();
  }

 private:
  void EnableAutofillMetadataFieldTrial() {
    field_trial_list_.reset();
    field_trial_list_.reset(
        new base::FieldTrialList(new metrics::SHA1EntropyProvider("foo")));
    field_trial_ = base::FieldTrialList::CreateFieldTrial(
        "AutofillFieldMetadata", "Enabled");
    field_trial_->group();
  }

  scoped_ptr<base::FieldTrialList> field_trial_list_;
  scoped_refptr<base::FieldTrial> field_trial_;
};

TEST_F(FormStructureTest, FieldCount) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("address1");
  field.name = ASCIIToUTF16("address1");
  field.form_control_type = "text";
  field.should_autocomplete = false;
  form.fields.push_back(field);

  // The render process sends all fields to browser including fields with
  // autocomplete=off
  form_structure.reset(new FormStructure(form));
  EXPECT_EQ(4U, form_structure->field_count());
}

TEST_F(FormStructureTest, AutofillCount) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("city");
  field.name = ASCIIToUTF16("city");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("state");
  field.name = ASCIIToUTF16("state");
  field.form_control_type = "select-one";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  // Only text and select fields that are heuristically matched are counted.
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_EQ(3U, form_structure->autofill_count());

  // Add a field with should_autocomplete=false. This should not be considered a
  // fillable field.
  field.label = ASCIIToUTF16("address1");
  field.name = ASCIIToUTF16("address1");
  field.form_control_type = "text";
  field.should_autocomplete = false;
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_EQ(4U, form_structure->autofill_count());
}

TEST_F(FormStructureTest, SourceURL) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");
  FormStructure form_structure(form);

  EXPECT_EQ(form.origin, form_structure.source_url());
}

TEST_F(FormStructureTest, IsAutofillable) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  // We need at least three text fields to be auto-fillable.
  FormFieldData field;

  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_FALSE(form_structure->IsAutofillable());

  // We now have three text fields, but only two auto-fillable fields.
  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_FALSE(form_structure->IsAutofillable());

  // We now have three auto-fillable fields.
  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());

  // The target cannot include http(s)://*/search...
  form.action = GURL("http://google.com/search?q=hello");
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_FALSE(form_structure->IsAutofillable());

  // But search can be in the URL.
  form.action = GURL("http://search.com/?q=hello");
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
}

TEST_F(FormStructureTest, ShouldBeParsed) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  // We need at least three text fields to be parseable.
  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  FormFieldData checkable_field;
  checkable_field.is_checkable = true;
  checkable_field.name = ASCIIToUTF16("radiobtn");
  checkable_field.form_control_type = "radio";
  form.fields.push_back(checkable_field);

  checkable_field.name = ASCIIToUTF16("checkbox");
  checkable_field.form_control_type = "checkbox";
  form.fields.push_back(checkable_field);

  // We have only one text field, should not be parsed.
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->ShouldBeParsed());

  // We now have three text fields, though only two are auto-fillable.
  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.form_control_type = "text";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->ShouldBeParsed());

  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->IsAutofillable());
  EXPECT_TRUE(form_structure->ShouldBeParsed());

  // The target cannot include http(s)://*/search...
  form.action = GURL("http://google.com/search?q=hello");
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->ShouldBeParsed());

  // But search can be in the URL.
  form.action = GURL("http://search.com/?q=hello");
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->ShouldBeParsed());

  // The form need only have three fields, but at least one must be a text
  // field.
  form.fields.clear();

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State");
  field.name = ASCIIToUTF16("state");
  field.form_control_type = "select-one";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.form_control_type = "select-one";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->ShouldBeParsed());

  form.fields[0].form_control_type = "select-one";
  // Now, no text fields.
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->ShouldBeParsed());
}

TEST_F(FormStructureTest, HeuristicsContactInfo) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Zip code");
  field.name = ASCIIToUTF16("zipcode");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(8U, form_structure->field_count());
  ASSERT_EQ(7U, form_structure->autofill_count());

  // First name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  // Email.
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
      form_structure->field(3)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(4)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(5)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(6)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(7)->heuristic_type());
}

// Verify that we can correctly process the |autocomplete| attribute.
TEST_F(FormStructureTest, HeuristicsAutocompleteAttribute) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = base::string16();
  field.name = ASCIIToUTF16("field1");
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("field2");
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("field3");
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(3U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  EXPECT_EQ(HTML_TYPE_GIVEN_NAME, form_structure->field(0)->html_type());
  EXPECT_EQ(HTML_TYPE_FAMILY_NAME, form_structure->field(1)->html_type());
  EXPECT_EQ(HTML_TYPE_EMAIL, form_structure->field(2)->html_type());
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(2)->heuristic_type());
}

// Verify that we can correctly process the 'autocomplete' attribute for phone
// number types (especially phone prefixes and suffixes).
TEST_F(FormStructureTest, HeuristicsAutocompleteAttributePhoneTypes) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = base::string16();
  field.name = ASCIIToUTF16("field1");
  field.autocomplete_attribute = "tel-local";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("field2");
  field.autocomplete_attribute = "tel-local-prefix";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("field3");
  field.autocomplete_attribute = "tel-local-suffix";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(3U, form_structure->field_count());
  EXPECT_EQ(3U, form_structure->autofill_count());

  EXPECT_EQ(HTML_TYPE_TEL_LOCAL, form_structure->field(0)->html_type());
  EXPECT_EQ(AutofillField::IGNORED, form_structure->field(0)->phone_part());
  EXPECT_EQ(HTML_TYPE_TEL_LOCAL_PREFIX, form_structure->field(1)->html_type());
  EXPECT_EQ(AutofillField::PHONE_PREFIX,
            form_structure->field(1)->phone_part());
  EXPECT_EQ(HTML_TYPE_TEL_LOCAL_SUFFIX, form_structure->field(2)->html_type());
  EXPECT_EQ(AutofillField::PHONE_SUFFIX,
            form_structure->field(2)->phone_part());
}

// If at least one field includes type hints in the 'autocomplete' attribute, we
// should not try to apply any other heuristics.
TEST_F(FormStructureTest, AutocompleteAttributeOverridesOtherHeuristics) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  // Start with a regular contact form.
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  EXPECT_TRUE(form_structure->ShouldBeCrowdsourced());

  ASSERT_EQ(3U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());

  // Now update the first form field to include an 'autocomplete' attribute.
  form.fields.front().autocomplete_attribute = "x-other";
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_FALSE(form_structure->IsAutofillable());
  EXPECT_FALSE(form_structure->ShouldBeCrowdsourced());

  ASSERT_EQ(3U, form_structure->field_count());
  ASSERT_EQ(0U, form_structure->autofill_count());

  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(2)->heuristic_type());
}

// Even with an 'autocomplete' attribute set, ShouldBeCrowdsourced() should
// return true if the structure contains a password field, since there are
// no local heuristics to depend upon in this case. Fields will still not be
// considered autofillable though.
TEST_F(FormStructureTest, PasswordFormShouldBeCrowdsourced) {
  FormData form;

  // Start with a regular contact form.
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.autocomplete_attribute = "username";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Password");
  field.name = ASCIIToUTF16("Password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure.ShouldBeCrowdsourced());
}

// Verify that we can correctly process sections listed in the |autocomplete|
// attribute.
TEST_F(FormStructureTest, HeuristicsAutocompleteAttributeWithSections) {
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  // Some fields will have no section specified.  These fall into the default
  // section.
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);

  // We allow arbitrary section names.
  field.autocomplete_attribute = "section-foo email";
  form.fields.push_back(field);

  // "shipping" and "billing" are special section tokens that don't require the
  // "section-" prefix.
  field.autocomplete_attribute = "shipping email";
  form.fields.push_back(field);
  field.autocomplete_attribute = "billing email";
  form.fields.push_back(field);

  // "shipping" and "billing" can be combined with other section names.
  field.autocomplete_attribute = "section-foo shipping email";
  form.fields.push_back(field);
  field.autocomplete_attribute = "section-foo billing email";
  form.fields.push_back(field);

  // We don't do anything clever to try to coalesce sections; it's up to site
  // authors to avoid typos.
  field.autocomplete_attribute = "section--foo email";
  form.fields.push_back(field);

  // "shipping email" and "section--shipping" email should be parsed as
  // different sections.  This is only an interesting test due to how we
  // implement implicit section names from attributes like "shipping email"; see
  // the implementation for more details.
  field.autocomplete_attribute = "section--shipping email";
  form.fields.push_back(field);

  // Credit card fields are implicitly in a separate section from other fields.
  field.autocomplete_attribute = "section-foo cc-number";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure.IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(9U, form_structure.field_count());
  EXPECT_EQ(9U, form_structure.autofill_count());

  // All of the fields in this form should be parsed as belonging to different
  // sections.
  std::set<std::string> section_names;
  for (size_t i = 0; i < 9; ++i) {
    section_names.insert(form_structure.field(i)->section());
  }
  EXPECT_EQ(9U, section_names.size());
}

// Verify that we can correctly process a degenerate section listed in the
// |autocomplete| attribute.
TEST_F(FormStructureTest,
       HeuristicsAutocompleteAttributeWithSectionsDegenerate) {
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  // Some fields will have no section specified.  These fall into the default
  // section.
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);

  // Specifying "section-" is equivalent to not specifying a section.
  field.autocomplete_attribute = "section- email";
  form.fields.push_back(field);

  // Invalid tokens should prevent us from setting a section name.
  field.autocomplete_attribute = "garbage section-foo email";
  form.fields.push_back(field);
  field.autocomplete_attribute = "garbage section-bar email";
  form.fields.push_back(field);
  field.autocomplete_attribute = "garbage shipping email";
  form.fields.push_back(field);
  field.autocomplete_attribute = "garbage billing email";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();

  // Expect the correct number of fields.
  ASSERT_EQ(6U, form_structure.field_count());
  EXPECT_EQ(2U, form_structure.autofill_count());

  // All of the fields in this form should be parsed as belonging to the same
  // section.
  std::set<std::string> section_names;
  for (size_t i = 0; i < 6; ++i) {
    section_names.insert(form_structure.field(i)->section());
  }
  EXPECT_EQ(1U, section_names.size());
}

// Verify that we can correctly process repeated sections listed in the
// |autocomplete| attribute.
TEST_F(FormStructureTest, HeuristicsAutocompleteAttributeWithSectionsRepeated) {
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.autocomplete_attribute = "section-foo email";
  form.fields.push_back(field);
  field.autocomplete_attribute = "section-foo address-line1";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();

  // Expect the correct number of fields.
  ASSERT_EQ(2U, form_structure.field_count());
  EXPECT_EQ(2U, form_structure.autofill_count());

  // All of the fields in this form should be parsed as belonging to the same
  // section.
  std::set<std::string> section_names;
  for (size_t i = 0; i < 2; ++i) {
    section_names.insert(form_structure.field(i)->section());
  }
  EXPECT_EQ(1U, section_names.size());
}

// Verify that we do not override the author-specified sections from a form with
// local heuristics.
TEST_F(FormStructureTest, HeuristicsDontOverrideAutocompleteAttributeSections) {
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.name = ASCIIToUTF16("one");
  field.autocomplete_attribute = "address-line1";
  form.fields.push_back(field);
  field.name = base::string16();
  field.autocomplete_attribute = "section-foo email";
  form.fields.push_back(field);
  field.name = base::string16();
  field.autocomplete_attribute = "name";
  form.fields.push_back(field);
  field.name = ASCIIToUTF16("two");
  field.autocomplete_attribute = "address-line1";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();

  // Expect the correct number of fields.
  ASSERT_EQ(4U, form_structure.field_count());
  EXPECT_EQ(4U, form_structure.autofill_count());

  // Normally, the two separate address fields would cause us to detect two
  // separate sections; but because there is an author-specified section in this
  // form, we do not apply these usual heuristics.
  EXPECT_EQ(ASCIIToUTF16("one"), form_structure.field(0)->name);
  EXPECT_EQ(ASCIIToUTF16("two"), form_structure.field(3)->name);
  EXPECT_EQ(form_structure.field(0)->section(),
            form_structure.field(3)->section());
}

TEST_F(FormStructureTest, HeuristicsSample8) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Your First Name:");
  field.name = ASCIIToUTF16("bill.first");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Your Last Name:");
  field.name = ASCIIToUTF16("bill.last");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Street Address Line 1:");
  field.name = ASCIIToUTF16("bill.street1");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Street Address Line 2:");
  field.name = ASCIIToUTF16("bill.street2");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("bill.city");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State (U.S.):");
  field.name = ASCIIToUTF16("bill.state");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Zip/Postal Code:");
  field.name = ASCIIToUTF16("BillTo.PostalCode");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country:");
  field.name = ASCIIToUTF16("bill.country");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone Number:");
  field.name = ASCIIToUTF16("BillTo.Phone");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(10U, form_structure->field_count());
  ASSERT_EQ(9U, form_structure->autofill_count());

  // First name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(2)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(3)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(4)->heuristic_type());
  // State.
  EXPECT_EQ(ADDRESS_HOME_STATE, form_structure->field(5)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(6)->heuristic_type());
  // Country.
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, form_structure->field(7)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
      form_structure->field(8)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(9)->heuristic_type());
}

TEST_F(FormStructureTest, HeuristicsSample6) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("E-mail address");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Full name");
  field.name = ASCIIToUTF16("name");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Company");
  field.name = ASCIIToUTF16("company");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Zip Code");
  field.name = ASCIIToUTF16("Home.PostalCode");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.value = ASCIIToUTF16("continue");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(7U, form_structure->field_count());
  ASSERT_EQ(6U, form_structure->autofill_count());

  // Email.
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(0)->heuristic_type());
  // Full name.
  EXPECT_EQ(NAME_FULL, form_structure->field(1)->heuristic_type());
  // Company
  EXPECT_EQ(COMPANY_NAME, form_structure->field(2)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(3)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(4)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(5)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(6)->heuristic_type());
}

// Tests a sequence of FormFields where only labels are supplied to heuristics
// for matching.  This works because FormFieldData labels are matched in the
// case that input element ids (or |name| fields) are missing.
TEST_F(FormStructureTest, HeuristicsLabelsOnly) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = base::string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = base::string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = base::string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = base::string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = base::string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = base::string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Zip code");
  field.name = base::string16();
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(8U, form_structure->field_count());
  ASSERT_EQ(7U, form_structure->autofill_count());

  // First name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  // Email.
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
      form_structure->field(3)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(4)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(5)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(6)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(7)->heuristic_type());
}

TEST_F(FormStructureTest, HeuristicsCreditCardInfo) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Verification");
  field.name = ASCIIToUTF16("verification");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(6U, form_structure->field_count());
  ASSERT_EQ(5U, form_structure->autofill_count());

  // Credit card name.
  EXPECT_EQ(CREDIT_CARD_NAME, form_structure->field(0)->heuristic_type());
  // Credit card number.
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(1)->heuristic_type());
  // Credit card expiration month.
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(2)->heuristic_type());
  // Credit card expiration year.
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(3)->heuristic_type());
  // CVV.
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            form_structure->field(4)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(5)->heuristic_type());
}

TEST_F(FormStructureTest, HeuristicsCreditCardInfoWithUnknownCardField) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  form.fields.push_back(field);

  // This is not a field we know how to process.  But we should skip over it
  // and process the other fields in the card block.
  field.label = ASCIIToUTF16("Card image");
  field.name = ASCIIToUTF16("card_image");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Exp Month");
  field.name = ASCIIToUTF16("ccmonth");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Exp Year");
  field.name = ASCIIToUTF16("ccyear");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Verification");
  field.name = ASCIIToUTF16("verification");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(7U, form_structure->field_count());
  ASSERT_EQ(5U, form_structure->autofill_count());

  // Credit card name.
  EXPECT_EQ(CREDIT_CARD_NAME, form_structure->field(0)->heuristic_type());
  // Credit card type.  This is an unknown type but related to the credit card.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(1)->heuristic_type());
  // Credit card number.
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(2)->heuristic_type());
  // Credit card expiration month.
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(3)->heuristic_type());
  // Credit card expiration year.
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(4)->heuristic_type());
  // CVV.
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            form_structure->field(5)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(6)->heuristic_type());
}

TEST_F(FormStructureTest, ThreeAddressLines) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address Line1");
  field.name = ASCIIToUTF16("Address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address Line2");
  field.name = ASCIIToUTF16("Address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address Line3");
  field.name = ASCIIToUTF16("Address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(4U, form_structure->field_count());
  ASSERT_EQ(4U, form_structure->autofill_count());

  // Address Line 1.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(0)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(1)->heuristic_type());
  // Address Line 3.
  EXPECT_EQ(ADDRESS_HOME_LINE3, form_structure->field(2)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(3)->heuristic_type());
}

// Numbered address lines after line two are ignored.
TEST_F(FormStructureTest, SurplusAddressLinesIgnored) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address Line1");
  field.name = ASCIIToUTF16("shipping.address.addressLine1");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address Line2");
  field.name = ASCIIToUTF16("shipping.address.addressLine2");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address Line3");
  field.name = ASCIIToUTF16("billing.address.addressLine3");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address Line4");
  field.name = ASCIIToUTF16("billing.address.addressLine4");
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  ASSERT_EQ(4U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  // Address Line 1.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(0)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(1)->heuristic_type());
  // Address Line 3.
  EXPECT_EQ(ADDRESS_HOME_LINE3, form_structure->field(2)->heuristic_type());
  // Address Line 4 (ignored).
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(3)->heuristic_type());
}

// This example comes from expedia.com where they used to use a "Suite" label
// to indicate a suite or apartment number (the form has changed since this
// test was written). We interpret this as address line 2. And the following
// "Street address second line" we interpret as address line 3.
// See http://crbug.com/48197 for details.
TEST_F(FormStructureTest, ThreeAddressLinesExpedia) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Street:");
  field.name = ASCIIToUTF16("FOPIH_RgWebCC_0_IHAddress_ads1");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Suite or Apt:");
  field.name = ASCIIToUTF16("FOPIH_RgWebCC_0_IHAddress_adap");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Street address second line");
  field.name = ASCIIToUTF16("FOPIH_RgWebCC_0_IHAddress_ads2");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City:");
  field.name = ASCIIToUTF16("FOPIH_RgWebCC_0_IHAddress_adct");
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(4U, form_structure->field_count());
  EXPECT_EQ(4U, form_structure->autofill_count());

  // Address Line 1.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(0)->heuristic_type());
  // Suite / Apt.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(1)->heuristic_type());
  // Address Line 3.
  EXPECT_EQ(ADDRESS_HOME_LINE3, form_structure->field(2)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(3)->heuristic_type());
}

// This example comes from ebay.com where the word "suite" appears in the label
// and the name "address2" clearly indicates that this is the address line 2.
// See http://crbug.com/48197 for details.
TEST_F(FormStructureTest, TwoAddressLinesEbay) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address Line1");
  field.name = ASCIIToUTF16("address1");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Floor number, suite number, etc");
  field.name = ASCIIToUTF16("address2");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City:");
  field.name = ASCIIToUTF16("city");
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(3U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  // Address Line 1.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(0)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(1)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(2)->heuristic_type());
}

TEST_F(FormStructureTest, HeuristicsStateWithProvince) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address Line1");
  field.name = ASCIIToUTF16("Address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address Line2");
  field.name = ASCIIToUTF16("Address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State/Province/Region");
  field.name = ASCIIToUTF16("State");
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(3U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  // Address Line 1.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(0)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(1)->heuristic_type());
  // State.
  EXPECT_EQ(ADDRESS_HOME_STATE, form_structure->field(2)->heuristic_type());
}

// This example comes from lego.com's checkout page.
TEST_F(FormStructureTest, HeuristicsWithBilling) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name*:");
  field.name = ASCIIToUTF16("editBillingAddress$firstNameBox");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name*:");
  field.name = ASCIIToUTF16("editBillingAddress$lastNameBox");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Company Name:");
  field.name = ASCIIToUTF16("editBillingAddress$companyBox");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address*:");
  field.name = ASCIIToUTF16("editBillingAddress$addressLine1Box");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Apt/Suite :");
  field.name = ASCIIToUTF16("editBillingAddress$addressLine2Box");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("City*:");
  field.name = ASCIIToUTF16("editBillingAddress$cityBox");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("State/Province*:");
  field.name = ASCIIToUTF16("editBillingAddress$stateDropDown");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Country*:");
  field.name = ASCIIToUTF16("editBillingAddress$countryDropDown");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Postal Code*:");
  field.name = ASCIIToUTF16("editBillingAddress$zipCodeBox");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone*:");
  field.name = ASCIIToUTF16("editBillingAddress$phoneBox");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email Address*:");
  field.name = ASCIIToUTF16("email$emailBox");
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(11U, form_structure->field_count());
  ASSERT_EQ(11U, form_structure->autofill_count());

  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(COMPANY_NAME, form_structure->field(2)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(3)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(4)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(5)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_STATE, form_structure->field(6)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, form_structure->field(7)->heuristic_type());
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(8)->heuristic_type());
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
            form_structure->field(9)->heuristic_type());
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(10)->heuristic_type());
}

TEST_F(FormStructureTest, ThreePartPhoneNumber) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Phone:");
  field.name = ASCIIToUTF16("dayphone1");
  field.max_length = 0;
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("-");
  field.name = ASCIIToUTF16("dayphone2");
  field.max_length = 3;  // Size of prefix is 3.
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("-");
  field.name = ASCIIToUTF16("dayphone3");
  field.max_length = 4;  // Size of suffix is 4.  If unlimited size is
                         // passed, phone will be parsed as
                         // <country code> - <area code> - <phone>.
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("ext.:");
  field.name = ASCIIToUTF16("dayphone4");
  field.max_length = 0;
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());
  ASSERT_EQ(4U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  // Area code.
  EXPECT_EQ(PHONE_HOME_CITY_CODE, form_structure->field(0)->heuristic_type());
  // Phone number suffix.
  EXPECT_EQ(PHONE_HOME_NUMBER,
            form_structure->field(1)->heuristic_type());
  // Phone number suffix.
  EXPECT_EQ(PHONE_HOME_NUMBER,
            form_structure->field(2)->heuristic_type());
  // Unknown.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(3)->heuristic_type());
}

TEST_F(FormStructureTest, HeuristicsInfernoCC) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("billing_address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Expiration Date");
  field.name = ASCIIToUTF16("expiration_month");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Expiration Year");
  field.name = ASCIIToUTF16("expiration_year");
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(5U, form_structure->field_count());
  EXPECT_EQ(5U, form_structure->autofill_count());

  // Name on Card.
  EXPECT_EQ(CREDIT_CARD_NAME, form_structure->field(0)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(1)->heuristic_type());
  // Card Number.
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(2)->heuristic_type());
  // Expiration Date.
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(3)->heuristic_type());
  // Expiration Year.
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(4)->heuristic_type());
}

TEST_F(FormStructureTest, CVCCodeClash) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Card number");
  field.name = ASCIIToUTF16("ccnumber");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("First name");
  field.name = ASCIIToUTF16("first_name");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last name");
  field.name = ASCIIToUTF16("last_name");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Expiration date");
  field.name = ASCIIToUTF16("ccexpiresmonth");
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("ccexpiresyear");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("cvc number");
  field.name = ASCIIToUTF16("csc");
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();
  EXPECT_TRUE(form_structure->IsAutofillable());

  // Expect the correct number of fields.
  ASSERT_EQ(6U, form_structure->field_count());
  ASSERT_EQ(5U, form_structure->autofill_count());

  // Card Number.
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(0)->heuristic_type());
  // First name, taken as name on card.
  EXPECT_EQ(CREDIT_CARD_NAME, form_structure->field(1)->heuristic_type());
  // Last name is not merged.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(2)->heuristic_type());
  // Expiration Date.
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(3)->heuristic_type());
  // Expiration Year.
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(4)->heuristic_type());
  // CVC code.
  EXPECT_EQ(CREDIT_CARD_VERIFICATION_CODE,
            form_structure->field(5)->heuristic_type());
}

TEST_F(FormStructureTest, EncodeQueryRequest) {
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Name on Card");
  field.name = ASCIIToUTF16("name_on_card");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("billing_address");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Card Number");
  field.name = ASCIIToUTF16("card_number");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Expiration Date");
  field.name = ASCIIToUTF16("expiration_month");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Expiration Year");
  field.name = ASCIIToUTF16("expiration_year");
  form.fields.push_back(field);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.is_checkable = true;
  checkable_field.label = ASCIIToUTF16("Checkable1");
  checkable_field.name = ASCIIToUTF16("Checkable1");
  form.fields.push_back(checkable_field);

  ScopedVector<FormStructure> forms;
  forms.push_back(new FormStructure(form));
  std::vector<std::string> encoded_signatures;
  std::string encoded_xml;
  const char kSignature1[] = "11337937696949187602";
  const char kResponse1[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<autofillquery clientversion=\"6.1.1715.1442/en (GGLL)\">"
      "<form signature=\"11337937696949187602\">"
      "<field signature=\"412125936\" name=\"name_on_card\" type=\"text\""
      " label=\"Name on Card\"/><field signature=\"1917667676\""
      " name=\"billing_address\" type=\"text\" label=\"Address\"/>"
      "<field signature=\"2226358947\" name=\"card_number\" type=\"text\""
      " label=\"Card Number\"/><field signature=\"747221617\""
      " name=\"expiration_month\" type=\"text\" label=\"Expiration Date\"/>"
      "<field signature=\"4108155786\" name=\"expiration_year\" type=\"text\""
      " label=\"Expiration Year\"/></form></autofillquery>";
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(
      forms.get(), &encoded_signatures, &encoded_xml));
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kSignature1, encoded_signatures[0]);
  EXPECT_EQ(kResponse1, encoded_xml);

  // Add the same form, only one will be encoded, so EncodeQueryRequest() should
  // return the same data.
  forms.push_back(new FormStructure(form));
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(
      forms.get(), &encoded_signatures, &encoded_xml));
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kSignature1, encoded_signatures[0]);
  EXPECT_EQ(kResponse1, encoded_xml);
  // Add 5 address fields - this should be still a valid form.
  for (size_t i = 0; i < 5; ++i) {
    field.label = ASCIIToUTF16("Address");
    field.name = ASCIIToUTF16("address");
    form.fields.push_back(field);
  }

  forms.push_back(new FormStructure(form));
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(
      forms.get(), &encoded_signatures, &encoded_xml));
  ASSERT_EQ(2U, encoded_signatures.size());
  EXPECT_EQ(kSignature1, encoded_signatures[0]);
  const char kSignature2[] = "8308881815906226214";
  EXPECT_EQ(kSignature2, encoded_signatures[1]);
  const char kResponse2[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
  "<autofillquery clientversion=\"6.1.1715.1442/en (GGLL)\">"
  "<form signature=\"11337937696949187602\"><field signature=\"412125936\""
  " name=\"name_on_card\" type=\"text\" label=\"Name on Card\"/>"
  "<field signature=\"1917667676\" name=\"billing_address\" type=\"text\""
  " label=\"Address\"/><field signature=\"2226358947\" name=\"card_number\""
  " type=\"text\" label=\"Card Number\"/>"
  "<field signature=\"747221617\" name=\"expiration_month\" type=\"text\""
  " label=\"Expiration Date\"/>"
  "<field signature=\"4108155786\" name=\"expiration_year\" type=\"text\""
  " label=\"Expiration Year\"/></form>"
  "<form signature=\"8308881815906226214\">"
  "<field signature=\"412125936\" name=\"name_on_card\" type=\"text\""
  " label=\"Name on Card\"/><field signature=\"1917667676\""
  " name=\"billing_address\" type=\"text\" label=\"Address\"/>"
  "<field signature=\"2226358947\" name=\"card_number\" type=\"text\""
  " label=\"Card Number\"/><field signature=\"747221617\""
  " name=\"expiration_month\" type=\"text\" label=\"Expiration Date\"/>"
  "<field signature=\"4108155786\" name=\"expiration_year\" type=\"text\""
  " label=\"Expiration Year\"/><field signature=\"509334676\" name=\"address\""
  " type=\"text\" label=\"Address\"/><field signature=\"509334676\""
  " name=\"address\" type=\"text\" label=\"Address\"/>"
  "<field signature=\"509334676\" name=\"address\" type=\"text\""
  " label=\"Address\"/><field signature=\"509334676\" name=\"address\""
  " type=\"text\" label=\"Address\"/><field signature=\"509334676\""
  " name=\"address\" type=\"text\" label=\"Address\"/></form></autofillquery>";
  EXPECT_EQ(kResponse2, encoded_xml);

  FormData malformed_form(form);
  // Add 50 address fields - the form is not valid anymore, but previous ones
  // are. The result should be the same as in previous test.
  for (size_t i = 0; i < 50; ++i) {
    field.label = ASCIIToUTF16("Address");
    field.name = ASCIIToUTF16("address");
    malformed_form.fields.push_back(field);
  }

  forms.push_back(new FormStructure(malformed_form));
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(
      forms.get(), &encoded_signatures, &encoded_xml));
  ASSERT_EQ(2U, encoded_signatures.size());
  EXPECT_EQ(kSignature1, encoded_signatures[0]);
  EXPECT_EQ(kSignature2, encoded_signatures[1]);
  EXPECT_EQ(kResponse2, encoded_xml);

  // Check that we fail if there are only bad form(s).
  ScopedVector<FormStructure> bad_forms;
  bad_forms.push_back(new FormStructure(malformed_form));
  EXPECT_FALSE(FormStructure::EncodeQueryRequest(
      bad_forms.get(), &encoded_signatures, &encoded_xml));
  EXPECT_EQ(0U, encoded_signatures.size());
  EXPECT_EQ("", encoded_xml);
}

TEST_F(FormStructureTest, EncodeUploadRequest) {
  scoped_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  FormData form;
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_FIRST);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_LAST);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(EMAIL_ADDRESS);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  field.form_control_type = "number";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(PHONE_HOME_WHOLE_NUMBER);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.form_control_type = "select-one";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(ADDRESS_HOME_COUNTRY);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.is_checkable = true;
  checkable_field.label = ASCIIToUTF16("Checkable1");
  checkable_field.name = ASCIIToUTF16("Checkable1");
  form.fields.push_back(checkable_field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(ADDRESS_HOME_COUNTRY);

  form_structure.reset(new FormStructure(form));

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->field(i)->set_possible_types(possible_field_types[i]);

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(ADDRESS_BILLING_LINE1);
  available_field_types.insert(ADDRESS_BILLING_LINE2);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);

  std::string encoded_xml;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), &encoded_xml));
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"8736493185895608956\" autofillused=\"false\""
            " datapresent=\"144200030e\""
            " actionsignature=\"15724779818122431245\">"
            "<field signature=\"3763331450\" name=\"firstname\" type=\"text\""
            " label=\"First Name\" autofilltype=\"3\"/>"
            "<field signature=\"3494530716\" name=\"lastname\" type=\"text\""
            " label=\"Last Name\" autofilltype=\"5\"/>"
            "<field signature=\"1029417091\" name=\"email\" type=\"email\""
            " label=\"Email\" autofilltype=\"9\"/>"
            "<field signature=\"466116101\" name=\"phone\" type=\"number\""
            " label=\"Phone\" autofilltype=\"14\"/>"
            "<field signature=\"2799270304\" name=\"country\""
            " type=\"select-one\" label=\"Country\" autofilltype=\"36\"/>"
            "</autofillupload>",
            encoded_xml);
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  std::string(), &encoded_xml));
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"8736493185895608956\""
            " autofillused=\"true\" datapresent=\"144200030e\""
            " actionsignature=\"15724779818122431245\">"
            "<field signature=\"3763331450\" name=\"firstname\" type=\"text\""
            " label=\"First Name\" autofilltype=\"3\"/>"
            "<field signature=\"3494530716\" name=\"lastname\" type=\"text\""
            " label=\"Last Name\" autofilltype=\"5\"/>"
            "<field signature=\"1029417091\" name=\"email\" type=\"email\""
            " label=\"Email\" autofilltype=\"9\"/>"
            "<field signature=\"466116101\" name=\"phone\" type=\"number\""
            " label=\"Phone\" autofilltype=\"14\"/>"
            "<field signature=\"2799270304\" name=\"country\""
            " type=\"select-one\" label=\"Country\" autofilltype=\"36\"/>"
            "</autofillupload>",
            encoded_xml);

  // Add 2 address fields - this should be still a valid form.
  for (size_t i = 0; i < 2; ++i) {
    field.label = ASCIIToUTF16("Address");
    field.name = ASCIIToUTF16("address");
    field.form_control_type = "text";
    form.fields.push_back(field);
    possible_field_types.push_back(ServerFieldTypeSet());
    possible_field_types.back().insert(ADDRESS_HOME_LINE1);
    possible_field_types.back().insert(ADDRESS_HOME_LINE2);
    possible_field_types.back().insert(ADDRESS_BILLING_LINE1);
    possible_field_types.back().insert(ADDRESS_BILLING_LINE2);
  }

  form_structure.reset(new FormStructure(form));
  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->field(i)->set_possible_types(possible_field_types[i]);

  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), &encoded_xml));
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"7816485729218079147\" autofillused=\"false\""
            " datapresent=\"144200030e\""
            " actionsignature=\"15724779818122431245\">"
            "<field signature=\"3763331450\" name=\"firstname\" type=\"text\""
            " label=\"First Name\" autofilltype=\"3\"/>"
            "<field signature=\"3494530716\" name=\"lastname\" type=\"text\""
            " label=\"Last Name\" autofilltype=\"5\"/>"
            "<field signature=\"1029417091\" name=\"email\" type=\"email\""
            " label=\"Email\" autofilltype=\"9\"/>"
            "<field signature=\"466116101\" name=\"phone\" type=\"number\""
            " label=\"Phone\" autofilltype=\"14\"/>"
            "<field signature=\"2799270304\" name=\"country\""
            " type=\"select-one\" label=\"Country\" autofilltype=\"36\"/>"
            "<field signature=\"509334676\" name=\"address\" type=\"text\""
            " label=\"Address\" autofilltype=\"30\"/>"
            "<field signature=\"509334676\" name=\"address\" type=\"text\""
            " label=\"Address\" autofilltype=\"31\"/>"
            "<field signature=\"509334676\" name=\"address\" type=\"text\""
            " label=\"Address\" autofilltype=\"37\"/>"
            "<field signature=\"509334676\" name=\"address\" type=\"text\""
            " label=\"Address\" autofilltype=\"38\"/>"
            "<field signature=\"509334676\" name=\"address\" type=\"text\""
            " label=\"Address\" autofilltype=\"30\"/>"
            "<field signature=\"509334676\" name=\"address\" type=\"text\""
            " label=\"Address\" autofilltype=\"31\"/>"
            "<field signature=\"509334676\" name=\"address\" type=\"text\""
            " label=\"Address\" autofilltype=\"37\"/>"
            "<field signature=\"509334676\" name=\"address\" type=\"text\""
            " label=\"Address\" autofilltype=\"38\"/></autofillupload>",
            encoded_xml);

  // Add 50 address fields - now the form is invalid, as it has too many fields.
  for (size_t i = 0; i < 50; ++i) {
    field.label = ASCIIToUTF16("Address");
    field.name = ASCIIToUTF16("address");
    field.form_control_type = "text";
    form.fields.push_back(field);
    possible_field_types.push_back(ServerFieldTypeSet());
    possible_field_types.back().insert(ADDRESS_HOME_LINE1);
    possible_field_types.back().insert(ADDRESS_HOME_LINE2);
    possible_field_types.back().insert(ADDRESS_BILLING_LINE1);
    possible_field_types.back().insert(ADDRESS_BILLING_LINE2);
  }
  form_structure.reset(new FormStructure(form));
  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
  EXPECT_FALSE(form_structure->EncodeUploadRequest(
      available_field_types, false, std::string(), &encoded_xml));
}

TEST_F(FormStructureTest,
       EncodeUploadRequestWithAdditionalPasswordFormSignature) {
  scoped_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  FormData form;
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_FIRST);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_LAST);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(EMAIL_ADDRESS);

  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(USERNAME);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(ACCOUNT_CREATION_PASSWORD);

  form_structure.reset(new FormStructure(form));

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->field(i)->set_possible_types(possible_field_types[i]);

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(USERNAME);
  available_field_types.insert(ACCOUNT_CREATION_PASSWORD);

  std::string encoded_xml;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  "42", &encoded_xml));
  EXPECT_EQ(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?><autofillupload "
      "clientversion=\"6.1.1715.1442/en (GGLL)\" "
      "formsignature=\"5810032074788446513\" autofillused=\"true\" "
      "datapresent=\"1440000000000000000802\" "
      "actionsignature=\"15724779818122431245\" "
      "loginformsignature=\"42\"><field signature=\"4224610201\" "
      "name=\"firstname\" type=\"\" label=\"First Name\" "
      "autocomplete=\"given-name\" autofilltype=\"3\"/><field "
      "signature=\"2786066110\" name=\"lastname\" type=\"\" label=\"Last "
      "Name\" autocomplete=\"family-name\" autofilltype=\"5\"/><field "
      "signature=\"1029417091\" name=\"email\" type=\"email\" label=\"Email\" "
      "autocomplete=\"email\" autofilltype=\"9\"/><field "
      "signature=\"239111655\" name=\"username\" type=\"text\" "
      "label=\"username\" autocomplete=\"email\" autofilltype=\"86\"/><field "
      "signature=\"2051817934\" name=\"password\" type=\"password\" "
      "label=\"password\" autocomplete=\"email\" "
      "autofilltype=\"76\"/></autofillupload>",
      encoded_xml);
}

TEST_F(FormStructureTest, EncodeUploadRequest_WithAutocomplete) {
  scoped_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  FormData form;
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_FIRST);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_LAST);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(EMAIL_ADDRESS);

  form_structure.reset(new FormStructure(form));

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->field(i)->set_possible_types(possible_field_types[i]);

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  std::string encoded_xml;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  std::string(), &encoded_xml));
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"14746822798145140279\" autofillused=\"true\""
            " datapresent=\"1440\" actionsignature=\"15724779818122431245\">"
            "<field signature=\"3763331450\" name=\"firstname\" type=\"text\""
            " label=\"First Name\" autocomplete=\"given-name\""
            " autofilltype=\"3\"/>"
            "<field signature=\"3494530716\" name=\"lastname\" type=\"text\""
            " label=\"Last Name\" autocomplete=\"family-name\""
            " autofilltype=\"5\"/><field signature=\"1029417091\""
            " name=\"email\" type=\"email\" label=\"Email\""
            " autocomplete=\"email\" autofilltype=\"9\"/></autofillupload>",
            encoded_xml);
}

TEST_F(FormStructureTest, EncodeUploadRequest_WithLabels) {
  scoped_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  FormData form;
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  // No label for the first field.
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_FIRST);

  field.label = ASCIIToUTF16("Last Name");
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_LAST);

  field.label = ASCIIToUTF16("Email");
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(EMAIL_ADDRESS);

  form_structure.reset(new FormStructure(form));

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->field(i)->set_possible_types(possible_field_types[i]);

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  std::string encoded_xml;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  std::string(), &encoded_xml));
  // Expected that the first field does not send the label but others do.
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"6949133589768631292\" autofillused=\"true\""
            " datapresent=\"1440\""
            " actionsignature=\"15724779818122431245\">"
            "<field signature=\"1318412689\" type=\"text\" autofilltype=\"3\"/>"
            "<field signature=\"1318412689\" type=\"text\" label=\"Last Name\""
            " autofilltype=\"5\"/><field signature=\"1318412689\" type=\"text\""
            " label=\"Email\" autofilltype=\"9\"/></autofillupload>",
            encoded_xml);
}

// Test that the form name is sent in the upload request.
TEST_F(FormStructureTest, EncodeUploadRequest_WithFormName) {
  scoped_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  FormData form;
  // Setting the form name which we expect to see in the upload.
  form.name = base::ASCIIToUTF16("myform");
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_FIRST);

  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_LAST);

  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(EMAIL_ADDRESS);

  form_structure.reset(new FormStructure(form));

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->field(i)->set_possible_types(possible_field_types[i]);

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  std::string encoded_xml;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  std::string(), &encoded_xml));
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?><autofillupload"
            " clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"2345951786066580868\" autofillused=\"true\""
            " datapresent=\"1440\" actionsignature=\"15724779818122431245\""
            " formname=\"myform\"><field signature=\"1318412689\" type=\"text\""
            " autofilltype=\"3\"/><field signature=\"1318412689\" type=\"text\""
            " autofilltype=\"5\"/><field signature=\"1318412689\" type=\"text\""
            " autofilltype=\"9\"/></autofillupload>",
            encoded_xml);
}

TEST_F(FormStructureTest, EncodeUploadRequestPartialMetadata) {
  scoped_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  FormData form;
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  // Some fields don't have "name" or "autocomplete" attributes, and some have
  // neither.
  // No label.
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_FIRST);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_LAST);

  field.label = ASCIIToUTF16("Email");
  field.form_control_type = "email";
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(EMAIL_ADDRESS);

  form_structure.reset(new FormStructure(form));

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->field(i)->set_possible_types(possible_field_types[i]);

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  std::string encoded_xml;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  std::string(), &encoded_xml));
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"13043654279838250996\" autofillused=\"true\""
            " datapresent=\"1440\" actionsignature=\"15724779818122431245\">"
            "<field signature=\"1318412689\" type=\"text\" autofilltype=\"3\"/>"
            "<field signature=\"3494530716\" name=\"lastname\" type=\"text\""
            " label=\"Last Name\" autocomplete=\"family-name\""
            " autofilltype=\"5\"/>"
            "<field signature=\"1545468175\" name=\"lastname\" type=\"email\""
            " label=\"Email\" autocomplete=\"email\" autofilltype=\"9\"/>"
            "</autofillupload>",
            encoded_xml);
}

// Sending field metadata to the server is disabled.
TEST_F(FormStructureTest, EncodeUploadRequest_DisabledMetadataTrial) {
  DisableAutofillMetadataFieldTrial();

  scoped_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  FormData form;
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_FIRST);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_LAST);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(EMAIL_ADDRESS);

  form_structure.reset(new FormStructure(form));

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->field(i)->set_possible_types(possible_field_types[i]);

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);

  std::string encoded_xml;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  std::string(), &encoded_xml));
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"14746822798145140279\" autofillused=\"true\""
            " datapresent=\"1440\"><field signature=\"3763331450\""
            " autofilltype=\"3\"/><field signature=\"3494530716\""
            " autofilltype=\"5\"/><field signature=\"1029417091\""
            " autofilltype=\"9\"/></autofillupload>",
            encoded_xml);
}

TEST_F(FormStructureTest, EncodeFieldAssignments) {
  scoped_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  FormData form;
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes();

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_FIRST);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_LAST);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(EMAIL_ADDRESS);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  field.form_control_type = "number";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(PHONE_HOME_WHOLE_NUMBER);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.form_control_type = "select-one";
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(ADDRESS_HOME_COUNTRY);

  // Add checkable field.
  FormFieldData checkable_field;
  checkable_field.is_checkable = true;
  checkable_field.label = ASCIIToUTF16("Checkable1");
  checkable_field.name = ASCIIToUTF16("Checkable1");
  form.fields.push_back(checkable_field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(ADDRESS_HOME_COUNTRY);

  form_structure.reset(new FormStructure(form));

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->field(i)->set_possible_types(possible_field_types[i]);

  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(ADDRESS_BILLING_LINE1);
  available_field_types.insert(ADDRESS_BILLING_LINE2);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);

  std::string encoded_xml;
  EXPECT_TRUE(form_structure->EncodeFieldAssignments(
      available_field_types, &encoded_xml));
  EXPECT_EQ(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<fieldassignments formsignature=\"8736493185895608956\">"
      "<fields fieldid=\"3763331450\" fieldtype=\"3\" name=\"firstname\"/>"
      "<fields fieldid=\"3494530716\" fieldtype=\"5\" name=\"lastname\"/>"
      "<fields fieldid=\"1029417091\" fieldtype=\"9\" name=\"email\"/>"
      "<fields fieldid=\"466116101\" fieldtype=\"14\" name=\"phone\"/>"
      "<fields fieldid=\"2799270304\" fieldtype=\"36\" name=\"country\"/>"
      "<fields fieldid=\"3410250678\" fieldtype=\"36\" name=\"Checkable1\"/>"
      "</fieldassignments>",
      encoded_xml);

  // Add 2 address fields - this should be still a valid form.
  for (size_t i = 0; i < 2; ++i) {
    field.label = ASCIIToUTF16("Address");
    field.name = ASCIIToUTF16("address");
    field.form_control_type = "text";
    form.fields.push_back(field);
    possible_field_types.push_back(ServerFieldTypeSet());
    possible_field_types.back().insert(ADDRESS_HOME_LINE1);
    possible_field_types.back().insert(ADDRESS_HOME_LINE2);
    possible_field_types.back().insert(ADDRESS_BILLING_LINE1);
    possible_field_types.back().insert(ADDRESS_BILLING_LINE2);
  }

  form_structure.reset(new FormStructure(form));
  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->field(i)->set_possible_types(possible_field_types[i]);

  EXPECT_TRUE(form_structure->EncodeFieldAssignments(
      available_field_types, &encoded_xml));
  EXPECT_EQ(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<fieldassignments formsignature=\"7816485729218079147\">"
      "<fields fieldid=\"3763331450\" fieldtype=\"3\" name=\"firstname\"/>"
      "<fields fieldid=\"3494530716\" fieldtype=\"5\" name=\"lastname\"/>"
      "<fields fieldid=\"1029417091\" fieldtype=\"9\" name=\"email\"/>"
      "<fields fieldid=\"466116101\" fieldtype=\"14\" name=\"phone\"/>"
      "<fields fieldid=\"2799270304\" fieldtype=\"36\" name=\"country\"/>"
      "<fields fieldid=\"3410250678\" fieldtype=\"36\" name=\"Checkable1\"/>"
      "<fields fieldid=\"509334676\" fieldtype=\"30\" name=\"address\"/>"
      "<fields fieldid=\"509334676\" fieldtype=\"31\" name=\"address\"/>"
      "<fields fieldid=\"509334676\" fieldtype=\"37\" name=\"address\"/>"
      "<fields fieldid=\"509334676\" fieldtype=\"38\" name=\"address\"/>"
      "<fields fieldid=\"509334676\" fieldtype=\"30\" name=\"address\"/>"
      "<fields fieldid=\"509334676\" fieldtype=\"31\" name=\"address\"/>"
      "<fields fieldid=\"509334676\" fieldtype=\"37\" name=\"address\"/>"
      "<fields fieldid=\"509334676\" fieldtype=\"38\" name=\"address\"/>"
      "</fieldassignments>",
      encoded_xml);
}

// Check that we compute the "datapresent" string correctly for the given
// |available_types|.
TEST_F(FormStructureTest, CheckDataPresence) {
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("first");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("last");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);

  FormStructure form_structure(form);

  ServerFieldTypeSet unknown_type;
  unknown_type.insert(UNKNOWN_TYPE);
  for (size_t i = 0; i < form_structure.field_count(); ++i)
    form_structure.field(i)->set_possible_types(unknown_type);

  // No available types.
  // datapresent should be "" == trimmmed(0x0000000000000000) ==
  //     0b0000000000000000000000000000000000000000000000000000000000000000
  ServerFieldTypeSet available_field_types;

  std::string encoded_xml;
  EXPECT_TRUE(form_structure.EncodeUploadRequest(available_field_types, false,
                                                 std::string(), &encoded_xml));
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"6402244543831589061\" autofillused=\"false\""
            " datapresent=\"\" actionsignature=\"15724779818122431245\">"
            "<field signature=\"1089846351\" name=\"first\" type=\"text\""
            " label=\"First Name\" autofilltype=\"1\"/>"
            "<field signature=\"2404144663\" name=\"last\" type=\"text\""
            " label=\"Last Name\" autofilltype=\"1\"/>"
            "<field signature=\"420638584\" name=\"email\" type=\"text\""
            " label=\"Email\" autofilltype=\"1\"/></autofillupload>",
            encoded_xml);

  // Only a few types available.
  // datapresent should be "1540000240" == trimmmed(0x1540000240000000) ==
  //     0b0001010101000000000000000000001001000000000000000000000000000000
  // The set bits are:
  //  3 == NAME_FIRST
  //  5 == NAME_LAST
  //  7 == NAME_FULL
  //  9 == EMAIL_ADDRESS
  // 30 == ADDRESS_HOME_LINE1
  // 33 == ADDRESS_HOME_CITY
  available_field_types.clear();
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(NAME_FULL);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_CITY);

  EXPECT_TRUE(form_structure.EncodeUploadRequest(available_field_types, false,
                                                 std::string(), &encoded_xml));
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"6402244543831589061\" autofillused=\"false\""
            " datapresent=\"1540000240\""
            " actionsignature=\"15724779818122431245\">"
            "<field signature=\"1089846351\" name=\"first\" type=\"text\""
            " label=\"First Name\" autofilltype=\"1\"/>"
            "<field signature=\"2404144663\" name=\"last\" type=\"text\""
            " label=\"Last Name\" autofilltype=\"1\"/>"
            "<field signature=\"420638584\" name=\"email\" type=\"text\""
            " label=\"Email\" autofilltype=\"1\"/></autofillupload>",
            encoded_xml);

  // All supported non-credit card types available.
  // datapresent should be "1f7e000378000008" == trimmmed(0x1f7e000378000008) ==
  //     0b0001111101111110000000000000001101111000000000000000000000001000
  // The set bits are:
  //  3 == NAME_FIRST
  //  4 == NAME_MIDDLE
  //  5 == NAME_LAST
  //  6 == NAME_MIDDLE_INITIAL
  //  7 == NAME_FULL
  //  9 == EMAIL_ADDRESS
  // 10 == PHONE_HOME_NUMBER,
  // 11 == PHONE_HOME_CITY_CODE,
  // 12 == PHONE_HOME_COUNTRY_CODE,
  // 13 == PHONE_HOME_CITY_AND_NUMBER,
  // 14 == PHONE_HOME_WHOLE_NUMBER,
  // 30 == ADDRESS_HOME_LINE1
  // 31 == ADDRESS_HOME_LINE2
  // 33 == ADDRESS_HOME_CITY
  // 34 == ADDRESS_HOME_STATE
  // 35 == ADDRESS_HOME_ZIP
  // 36 == ADDRESS_HOME_COUNTRY
  // 60 == COMPANY_NAME
  available_field_types.clear();
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_MIDDLE);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(NAME_MIDDLE_INITIAL);
  available_field_types.insert(NAME_FULL);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_NUMBER);
  available_field_types.insert(PHONE_HOME_CITY_CODE);
  available_field_types.insert(PHONE_HOME_COUNTRY_CODE);
  available_field_types.insert(PHONE_HOME_CITY_AND_NUMBER);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_CITY);
  available_field_types.insert(ADDRESS_HOME_STATE);
  available_field_types.insert(ADDRESS_HOME_ZIP);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(COMPANY_NAME);

  EXPECT_TRUE(form_structure.EncodeUploadRequest(available_field_types, false,
                                                 std::string(), &encoded_xml));
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"6402244543831589061\" autofillused=\"false\""
            " datapresent=\"1f7e000378000008\""
            " actionsignature=\"15724779818122431245\">"
            "<field signature=\"1089846351\" name=\"first\" type=\"text\""
            " label=\"First Name\" autofilltype=\"1\"/>"
            "<field signature=\"2404144663\" name=\"last\" type=\"text\""
            " label=\"Last Name\" autofilltype=\"1\"/>"
            "<field signature=\"420638584\" name=\"email\" type=\"text\""
            " label=\"Email\" autofilltype=\"1\"/></autofillupload>",
            encoded_xml);

  // All supported credit card types available.
  // datapresent should be "0000000000001fc0" == trimmmed(0x0000000000001fc0) ==
  //     0b0000000000000000000000000000000000000000000000000001111111000000
  // The set bits are:
  // 51 == CREDIT_CARD_NAME
  // 52 == CREDIT_CARD_NUMBER
  // 53 == CREDIT_CARD_EXP_MONTH
  // 54 == CREDIT_CARD_EXP_2_DIGIT_YEAR
  // 55 == CREDIT_CARD_EXP_4_DIGIT_YEAR
  // 56 == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR
  // 57 == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR
  available_field_types.clear();
  available_field_types.insert(CREDIT_CARD_NAME);
  available_field_types.insert(CREDIT_CARD_NUMBER);
  available_field_types.insert(CREDIT_CARD_EXP_MONTH);
  available_field_types.insert(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_4_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);

  EXPECT_TRUE(form_structure.EncodeUploadRequest(available_field_types, false,
                                                 std::string(), &encoded_xml));
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"6402244543831589061\" autofillused=\"false\""
            " datapresent=\"0000000000001fc0\""
            " actionsignature=\"15724779818122431245\">"
            "<field signature=\"1089846351\" name=\"first\" type=\"text\""
            " label=\"First Name\" autofilltype=\"1\"/>"
            "<field signature=\"2404144663\" name=\"last\" type=\"text\""
            " label=\"Last Name\" autofilltype=\"1\"/>"
            "<field signature=\"420638584\" name=\"email\" type=\"text\""
            " label=\"Email\" autofilltype=\"1\"/></autofillupload>",
            encoded_xml);

  // All supported types available.
  // datapresent should be "1f7e000378001fc8" == trimmmed(0x1f7e000378001fc8) ==
  //     0b0001111101111110000000000000001101111000000000000001111111001000
  // The set bits are:
  //  3 == NAME_FIRST
  //  4 == NAME_MIDDLE
  //  5 == NAME_LAST
  //  6 == NAME_MIDDLE_INITIAL
  //  7 == NAME_FULL
  //  9 == EMAIL_ADDRESS
  // 10 == PHONE_HOME_NUMBER,
  // 11 == PHONE_HOME_CITY_CODE,
  // 12 == PHONE_HOME_COUNTRY_CODE,
  // 13 == PHONE_HOME_CITY_AND_NUMBER,
  // 14 == PHONE_HOME_WHOLE_NUMBER,
  // 30 == ADDRESS_HOME_LINE1
  // 31 == ADDRESS_HOME_LINE2
  // 33 == ADDRESS_HOME_CITY
  // 34 == ADDRESS_HOME_STATE
  // 35 == ADDRESS_HOME_ZIP
  // 36 == ADDRESS_HOME_COUNTRY
  // 51 == CREDIT_CARD_NAME
  // 52 == CREDIT_CARD_NUMBER
  // 53 == CREDIT_CARD_EXP_MONTH
  // 54 == CREDIT_CARD_EXP_2_DIGIT_YEAR
  // 55 == CREDIT_CARD_EXP_4_DIGIT_YEAR
  // 56 == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR
  // 57 == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR
  // 60 == COMPANY_NAME
  available_field_types.clear();
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_MIDDLE);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(NAME_MIDDLE_INITIAL);
  available_field_types.insert(NAME_FULL);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(PHONE_HOME_NUMBER);
  available_field_types.insert(PHONE_HOME_CITY_CODE);
  available_field_types.insert(PHONE_HOME_COUNTRY_CODE);
  available_field_types.insert(PHONE_HOME_CITY_AND_NUMBER);
  available_field_types.insert(PHONE_HOME_WHOLE_NUMBER);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_CITY);
  available_field_types.insert(ADDRESS_HOME_STATE);
  available_field_types.insert(ADDRESS_HOME_ZIP);
  available_field_types.insert(ADDRESS_HOME_COUNTRY);
  available_field_types.insert(CREDIT_CARD_NAME);
  available_field_types.insert(CREDIT_CARD_NUMBER);
  available_field_types.insert(CREDIT_CARD_EXP_MONTH);
  available_field_types.insert(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_4_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR);
  available_field_types.insert(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
  available_field_types.insert(COMPANY_NAME);

  EXPECT_TRUE(form_structure.EncodeUploadRequest(available_field_types, false,
                                                 std::string(), &encoded_xml));
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"6402244543831589061\" autofillused=\"false\""
            " datapresent=\"1f7e000378001fc8\""
            " actionsignature=\"15724779818122431245\">"
            "<field signature=\"1089846351\" name=\"first\" type=\"text\""
            " label=\"First Name\" autofilltype=\"1\"/>"
            "<field signature=\"2404144663\" name=\"last\" type=\"text\""
            " label=\"Last Name\" autofilltype=\"1\"/>"
            "<field signature=\"420638584\" name=\"email\" type=\"text\""
            " label=\"Email\" autofilltype=\"1\"/></autofillupload>",
            encoded_xml);
}

TEST_F(FormStructureTest, CheckMultipleTypes) {
  // Throughout this test, datapresent should be
  // 0x1440000360000008 ==
  //     0b0001010001000000000000000000001101100000000000000000000000001000
  // The set bits are:
  //  3 == NAME_FIRST
  //  5 == NAME_LAST
  //  9 == EMAIL_ADDRESS
  // 30 == ADDRESS_HOME_LINE1
  // 31 == ADDRESS_HOME_LINE2
  // 33 == ADDRESS_HOME_CITY
  // 34 == ADDRESS_HOME_STATE
  // 60 == COMPANY_NAME
  ServerFieldTypeSet available_field_types;
  available_field_types.insert(NAME_FIRST);
  available_field_types.insert(NAME_LAST);
  available_field_types.insert(EMAIL_ADDRESS);
  available_field_types.insert(ADDRESS_HOME_LINE1);
  available_field_types.insert(ADDRESS_HOME_LINE2);
  available_field_types.insert(ADDRESS_HOME_CITY);
  available_field_types.insert(ADDRESS_HOME_STATE);
  available_field_types.insert(COMPANY_NAME);

  // Check that multiple types for the field are processed correctly.
  scoped_ptr<FormStructure> form_structure;
  std::vector<ServerFieldTypeSet> possible_field_types;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(EMAIL_ADDRESS);

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("first");
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_FIRST);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("last");
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(NAME_LAST);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);
  possible_field_types.push_back(ServerFieldTypeSet());
  possible_field_types.back().insert(ADDRESS_HOME_LINE1);

  form_structure.reset(new FormStructure(form));

  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
  std::string encoded_xml;

  // Now we matched both fields singularly.
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), &encoded_xml));
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"18062476096658145866\" autofillused=\"false\""
            " datapresent=\"1440000360000008\""
            " actionsignature=\"15724779818122431245\">"
            "<field signature=\"420638584\" name=\"email\" type=\"text\""
            " label=\"email\" autofilltype=\"9\"/>"
            "<field signature=\"1089846351\" name=\"first\" type=\"text\""
            " label=\"First Name\" autofilltype=\"3\"/>"
            "<field signature=\"2404144663\" name=\"last\" type=\"text\""
            " label=\"Last Name\" autofilltype=\"5\"/>"
            "<field signature=\"509334676\" name=\"address\" type=\"text\""
            " label=\"Address\" autofilltype=\"30\"/></autofillupload>",
            encoded_xml);
  // Match third field as both first and last.
  possible_field_types[2].insert(NAME_FIRST);
  form_structure->field(2)->set_possible_types(possible_field_types[2]);
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), &encoded_xml));
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"18062476096658145866\" autofillused=\"false\""
            " datapresent=\"1440000360000008\""
            " actionsignature=\"15724779818122431245\">"
            "<field signature=\"420638584\" name=\"email\" type=\"text\""
            " label=\"email\" autofilltype=\"9\"/>"
            "<field signature=\"1089846351\" name=\"first\" type=\"text\""
            " label=\"First Name\" autofilltype=\"3\"/>"
            "<field signature=\"2404144663\" name=\"last\" type=\"text\""
            " label=\"Last Name\" autofilltype=\"3\"/>"
            "<field signature=\"2404144663\" name=\"last\" type=\"text\""
            " label=\"Last Name\" autofilltype=\"5\"/>"
            "<field signature=\"509334676\" name=\"address\" type=\"text\""
            " label=\"Address\" autofilltype=\"30\"/></autofillupload>",
            encoded_xml);
  possible_field_types[3].insert(ADDRESS_HOME_LINE2);
  form_structure->field(form_structure->field_count() - 1)->set_possible_types(
      possible_field_types[form_structure->field_count() - 1]);
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), &encoded_xml));
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"18062476096658145866\" autofillused=\"false\""
            " datapresent=\"1440000360000008\""
            " actionsignature=\"15724779818122431245\">"
            "<field signature=\"420638584\" name=\"email\" type=\"text\""
            " label=\"email\" autofilltype=\"9\"/>"
            "<field signature=\"1089846351\" name=\"first\" type=\"text\""
            " label=\"First Name\" autofilltype=\"3\"/>"
            "<field signature=\"2404144663\" name=\"last\" type=\"text\""
            " label=\"Last Name\" autofilltype=\"3\"/>"
            "<field signature=\"2404144663\" name=\"last\" type=\"text\""
            " label=\"Last Name\" autofilltype=\"5\"/>"
            "<field signature=\"509334676\" name=\"address\" type=\"text\""
            " label=\"Address\" autofilltype=\"30\"/>"
            "<field signature=\"509334676\" name=\"address\" type=\"text\""
            " label=\"Address\" autofilltype=\"31\"/></autofillupload>",
            encoded_xml);
  possible_field_types[3].clear();
  possible_field_types[3].insert(ADDRESS_HOME_LINE1);
  possible_field_types[3].insert(COMPANY_NAME);
  form_structure->field(form_structure->field_count() - 1)->set_possible_types(
      possible_field_types[form_structure->field_count() - 1]);
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  std::string(), &encoded_xml));
  EXPECT_EQ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"18062476096658145866\" autofillused=\"false\""
            " datapresent=\"1440000360000008\""
            " actionsignature=\"15724779818122431245\">"
            "<field signature=\"420638584\" name=\"email\" type=\"text\""
            " label=\"email\" autofilltype=\"9\"/>"
            "<field signature=\"1089846351\" name=\"first\" type=\"text\""
            " label=\"First Name\" autofilltype=\"3\"/>"
            "<field signature=\"2404144663\" name=\"last\" type=\"text\""
            " label=\"Last Name\" autofilltype=\"3\"/>"
            "<field signature=\"2404144663\" name=\"last\" type=\"text\""
            " label=\"Last Name\" autofilltype=\"5\"/>"
            "<field signature=\"509334676\" name=\"address\" type=\"text\""
            " label=\"Address\" autofilltype=\"30\"/>"
            "<field signature=\"509334676\" name=\"address\" type=\"text\""
            " label=\"Address\" autofilltype=\"60\"/></autofillupload>",
            encoded_xml);
}

TEST_F(FormStructureTest, CheckFormSignature) {
  // Check that form signature is created correctly.
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("first");
  form.fields.push_back(field);

  // Checkable fields shouldn't affect the signature.
  field.label = ASCIIToUTF16("Select");
  field.name = ASCIIToUTF16("Select");
  field.form_control_type = "checkbox";
  field.is_checkable = true;
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));

  EXPECT_EQ(FormStructureTest::Hash64Bit(
      std::string("://&&email&first")),
      form_structure->FormSignature());

  form.origin = GURL(std::string("http://www.facebook.com"));
  form_structure.reset(new FormStructure(form));
  EXPECT_EQ(FormStructureTest::Hash64Bit(
      std::string("http://www.facebook.com&&email&first")),
      form_structure->FormSignature());

  form.action = GURL(std::string("https://login.facebook.com/path"));
  form_structure.reset(new FormStructure(form));
  EXPECT_EQ(FormStructureTest::Hash64Bit(
      std::string("https://login.facebook.com&&email&first")),
      form_structure->FormSignature());

  form.name = ASCIIToUTF16("login_form");
  form_structure.reset(new FormStructure(form));
  EXPECT_EQ(FormStructureTest::Hash64Bit(
      std::string("https://login.facebook.com&login_form&email&first")),
      form_structure->FormSignature());

  field.is_checkable = false;
  field.label = ASCIIToUTF16("Random Field label");
  field.name = ASCIIToUTF16("random1234");
  field.form_control_type = "text";
  form.fields.push_back(field);
  field.label = ASCIIToUTF16("Random Field label2");
  field.name = ASCIIToUTF16("random12345");
  form.fields.push_back(field);
  field.label = ASCIIToUTF16("Random Field label3");
  field.name = ASCIIToUTF16("1random12345678");
  form.fields.push_back(field);
  field.label = ASCIIToUTF16("Random Field label3");
  field.name = ASCIIToUTF16("12345random");
  form.fields.push_back(field);
  form_structure.reset(new FormStructure(form));
  EXPECT_EQ(FormStructureTest::Hash64Bit(
      std::string("https://login.facebook.com&login_form&email&first&"
                  "random1234&random&1random&random")),
      form_structure->FormSignature());
}

TEST_F(FormStructureTest, ToFormData) {
  FormData form;
  form.name = ASCIIToUTF16("the-name");
  form.origin = GURL("http://cool.com");
  form.action = form.origin.Resolve("/login");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  EXPECT_TRUE(form.SameFormAs(FormStructure(form).ToFormData()));
}

TEST_F(FormStructureTest, SkipFieldTest) {
  FormData form;
  form.name = ASCIIToUTF16("the-name");
  form.origin = GURL("http://cool.com");
  form.action = form.origin.Resolve("/login");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("select");
  field.name = ASCIIToUTF16("select");
  field.form_control_type = "checkbox";
  field.is_checkable = true;
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "text";
  field.is_checkable = false;
  form.fields.push_back(field);

  ScopedVector<FormStructure> forms;
  forms.push_back(new FormStructure(form));
  std::vector<std::string> encoded_signatures;
  std::string encoded_xml;

  const char kSignature[] = "18006745212084723782";
  const char kResponse[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<autofillquery clientversion=\"6.1.1715.1442/en (GGLL)\">"
      "<form signature=\"18006745212084723782\">"
      "<field signature=\"239111655\" name=\"username\" type=\"text\""
      " label=\"username\"/>"
      "<field signature=\"420638584\" name=\"email\" type=\"text\"/>"
      "</form></autofillquery>";
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(
      forms.get(), &encoded_signatures, &encoded_xml));
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kSignature, encoded_signatures[0]);
  EXPECT_EQ(kResponse, encoded_xml);
}

TEST_F(FormStructureTest, EncodeQueryRequest_WithLabels) {
  FormData form;
  form.name = ASCIIToUTF16("the-name");
  form.origin = GURL("http://cool.com");
  form.action = form.origin.Resolve("/login");

  FormFieldData field;
  // No label on the first field.
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Enter your Email address");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Enter your Password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  ScopedVector<FormStructure> forms;
  forms.push_back(new FormStructure(form));
  std::vector<std::string> encoded_signatures;
  std::string encoded_xml;

  const char kRequest[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<autofillquery clientversion=\"6.1.1715.1442/en (GGLL)\">"
      "<form signature=\"13906559713264665730\">"
      "<field signature=\"239111655\" name=\"username\" type=\"text\"/>"
      "<field signature=\"420638584\" name=\"email\" type=\"text\""
      " label=\"Enter your Email address\"/>"
      "<field signature=\"2051817934\" name=\"password\" type=\"password\""
      " label=\"Enter your Password\"/></form></autofillquery>";
  EXPECT_TRUE(FormStructure::EncodeQueryRequest(
      forms.get(), &encoded_signatures, &encoded_xml));
  EXPECT_EQ(kRequest, encoded_xml);
}

TEST_F(FormStructureTest, EncodeQueryRequest_WithLongLabels) {
  FormData form;
  form.name = ASCIIToUTF16("the-name");
  form.origin = GURL("http://cool.com");
  form.action = form.origin.Resolve("/login");

  FormFieldData field;
  // No label on the first field.
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  // This label will be truncated in the XML request.
  field.label = ASCIIToUTF16(
      "Enter Your Really Really Really (Really!) Long Email Address Which We "
      "Hope To Get In Order To Send You Unwanted Publicity Because That's What "
      "Marketers Do! We Know That Your Email Address Has The Possibility Of "
      "Exceeding A Certain Number Of Characters...");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Enter your Password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  ScopedVector<FormStructure> forms;
  forms.push_back(new FormStructure(form));
  std::vector<std::string> encoded_signatures;
  std::string encoded_xml;

  const char kRequest[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<autofillquery clientversion=\"6.1.1715.1442/en (GGLL)\">"
      "<form signature=\"13906559713264665730\">"
      "<field signature=\"239111655\" name=\"username\" type=\"text\"/>"
      "<field signature=\"420638584\" name=\"email\" type=\"text\""
      " label=\"Enter Your Really Really Really (Really!) Long Email Address"
      " Which We Hope To Get In Order To Send You Unwanted Publicity Because"
      " That's What Marketers Do! We Know That Your Email Address Has The"
      " Poss\"/>"
      "<field signature=\"2051817934\" name=\"password\" type=\"password\""
      " label=\"Enter your Password\"/></form></autofillquery>";
  EXPECT_TRUE(FormStructure::EncodeQueryRequest(
      forms.get(), &encoded_signatures, &encoded_xml));
  EXPECT_EQ(kRequest, encoded_xml);
}

// One name is missing from one field.
TEST_F(FormStructureTest, EncodeQueryRequest_MissingNames) {
  FormData form;
  // No name set for the form.
  form.origin = GURL("http://cool.com");
  form.action = form.origin.Resolve("/login");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = base::string16();
  // No name set for this field.
  field.name = ASCIIToUTF16("");
  field.form_control_type = "text";
  field.is_checkable = false;
  form.fields.push_back(field);

  ScopedVector<FormStructure> forms;
  forms.push_back(new FormStructure(form));
  std::vector<std::string> encoded_signatures;
  std::string encoded_xml;

  const char kSignature[] = "16416961345885087496";
  const char kResponse[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<autofillquery clientversion=\"6.1.1715.1442/en (GGLL)\">"
      "<form signature=\"16416961345885087496\">"
      "<field signature=\"239111655\" name=\"username\" type=\"text\""
      " label=\"username\"/><field signature=\"1318412689\" type=\"text\"/>"
      "</form></autofillquery>";
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(
      forms.get(), &encoded_signatures, &encoded_xml));
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kSignature, encoded_signatures[0]);
  EXPECT_EQ(kResponse, encoded_xml);
}

// Sending field metadata to the server is disabled.
TEST_F(FormStructureTest, EncodeQueryRequest_DisabledMetadataTrial) {
  DisableAutofillMetadataFieldTrial();

  FormData form;
  // No name set for the form.
  form.origin = GURL("http://cool.com");
  form.action = form.origin.Resolve("/login");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = base::string16();
  field.name = ASCIIToUTF16("country");
  field.form_control_type = "text";
  field.is_checkable = false;
  form.fields.push_back(field);

  ScopedVector<FormStructure> forms;
  forms.push_back(new FormStructure(form));
  std::vector<std::string> encoded_signatures;
  std::string encoded_xml;

  const char kSignature[] = "7635954436925888745";
  const char kResponse[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<autofillquery clientversion=\"6.1.1715.1442/en (GGLL)\">"
      "<form signature=\"7635954436925888745\">"
      "<field signature=\"239111655\"/>"
      "<field signature=\"3654076265\"/>"
      "</form></autofillquery>";
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(
      forms.get(), &encoded_signatures, &encoded_xml));
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kSignature, encoded_signatures[0]);
  EXPECT_EQ(kResponse, encoded_xml);
}

TEST_F(FormStructureTest, PossibleValues) {
  FormData form_data;
  FormFieldData field;
  field.autocomplete_attribute = "billing country";
  field.option_contents.push_back(ASCIIToUTF16("Down Under"));
  field.option_values.push_back(ASCIIToUTF16("AU"));
  field.option_contents.push_back(ASCIIToUTF16("Fr"));
  field.option_values.push_back(ASCIIToUTF16(""));
  field.option_contents.push_back(ASCIIToUTF16("Germany"));
  field.option_values.push_back(ASCIIToUTF16("GRMNY"));
  form_data.fields.push_back(field);
  FormStructure form_structure(form_data);

  bool unused;
  form_structure.ParseFieldTypesFromAutocompleteAttributes(&unused, &unused);

  // All values in <option> value= or contents are returned, set to upper case.
  std::set<base::string16> possible_values =
      form_structure.PossibleValues(ADDRESS_BILLING_COUNTRY);
  EXPECT_EQ(5U, possible_values.size());
  EXPECT_EQ(1U, possible_values.count(ASCIIToUTF16("AU")));
  EXPECT_EQ(1U, possible_values.count(ASCIIToUTF16("FR")));
  EXPECT_EQ(1U, possible_values.count(ASCIIToUTF16("DOWN UNDER")));
  EXPECT_EQ(1U, possible_values.count(ASCIIToUTF16("GERMANY")));
  EXPECT_EQ(1U, possible_values.count(ASCIIToUTF16("GRMNY")));
  EXPECT_EQ(0U, possible_values.count(ASCIIToUTF16("Fr")));
  EXPECT_EQ(0U, possible_values.count(ASCIIToUTF16("DE")));

  // No field for the given type; empty value set.
  EXPECT_EQ(0U, form_structure.PossibleValues(ADDRESS_HOME_COUNTRY).size());

  // A freeform input (<input>) allows any value (overriding other <select>s).
  FormFieldData freeform_field;
  freeform_field.autocomplete_attribute = "billing country";
  form_data.fields.push_back(freeform_field);
  FormStructure form_structure2(form_data);
  form_structure2.ParseFieldTypesFromAutocompleteAttributes(&unused, &unused);
  EXPECT_EQ(0U, form_structure2.PossibleValues(ADDRESS_BILLING_COUNTRY).size());
}

TEST_F(FormStructureTest, ParseQueryResponse) {
  FormData form;
  form.origin = GURL("http://foo.com");
  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("fullname");
  field.name = ASCIIToUTF16("fullname");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);

  // Checkable fields should be ignored in parsing
  FormFieldData checkable_field;
  checkable_field.label = ASCIIToUTF16("radio_button");
  checkable_field.form_control_type = "radio";
  checkable_field.is_checkable = true;
  form.fields.push_back(checkable_field);

  ScopedVector<FormStructure> forms;
  forms.push_back(new FormStructure(form));

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  forms.push_back(new FormStructure(form));

  std::string response =
      "<autofillqueryresponse>"
      "<field autofilltype=\"7\" />"
      "<field autofilltype=\"30\" />"
      "<field autofilltype=\"9\" />"
      "<field autofilltype=\"0\" />"
      "</autofillqueryresponse>";

  FormStructure::ParseQueryResponse(response, forms.get(), nullptr);

  ASSERT_GE(forms[0]->field_count(), 2U);
  ASSERT_GE(forms[1]->field_count(), 2U);
  EXPECT_EQ(7, forms[0]->field(0)->server_type());
  EXPECT_EQ(30, forms[0]->field(1)->server_type());
  EXPECT_EQ(9, forms[1]->field(0)->server_type());
  EXPECT_EQ(0, forms[1]->field(1)->server_type());
}

// If user defined types are present, only parse password fields.
TEST_F(FormStructureTest, ParseQueryResponseAuthorDefinedTypes) {
  FormData form;
  form.origin = GURL("http://foo.com");
  FormFieldData field;

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "text";
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  field.autocomplete_attribute = "new-password";
  form.fields.push_back(field);

  ScopedVector<FormStructure> forms;
  forms.push_back(new FormStructure(form));
  forms.front()->DetermineHeuristicTypes();

  std::string response =
      "<autofillqueryresponse>"
      "<field autofilltype=\"9\" />"
      "<field autofilltype=\"76\" />"
      "</autofillqueryresponse>";

  FormStructure::ParseQueryResponse(response, forms.get(), nullptr);

  ASSERT_GE(forms[0]->field_count(), 2U);
  EXPECT_EQ(NO_SERVER_DATA, forms[0]->field(0)->server_type());
  EXPECT_EQ(76, forms[0]->field(1)->server_type());
}

}  // namespace autofill
