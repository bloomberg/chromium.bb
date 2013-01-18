// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/form_structure.h"

#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/common/form_data.h"
#include "chrome/common/form_field_data.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"

using WebKit::WebInputElement;

namespace {

// Unlike the base AutofillMetrics, exposes copy and assignment constructors,
// which are handy for briefer test code.  The AutofillMetrics class is
// stateless, so this is safe.
class TestAutofillMetrics : public AutofillMetrics {
 public:
  TestAutofillMetrics() {}
  virtual ~TestAutofillMetrics() {}
};

}  // anonymous namespace


namespace content {

std::ostream& operator<<(std::ostream& os, const FormData& form) {
  os << UTF16ToUTF8(form.name)
     << " "
     << UTF16ToUTF8(form.method)
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

class FormStructureTest {
 public:
  static std::string Hash64Bit(const std::string& str) {
    return FormStructure::Hash64Bit(str);
  }

  static void SetPageDetails(FormStructure* form,
                             int page_number,
                             int total_pages) {
    form->current_page_number_ = page_number;
    form->total_pages_ = total_pages;
  }
};

TEST(FormStructureTest, FieldCount) {
  FormData form;
  form.method = ASCIIToUTF16("post");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  field.label = string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  FormStructure form_structure(form);

  // All fields are counted.
  EXPECT_EQ(3U, form_structure.field_count());
}

TEST(FormStructureTest, AutofillFlowInfo) {
  FormData form;
  form.method = ASCIIToUTF16("post");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  EXPECT_FALSE(form_structure.IsStartOfAutofillableFlow());
  EXPECT_FALSE(form_structure.IsInAutofillableFlow());

  FormStructureTest::SetPageDetails(&form_structure, -1, 0);
  EXPECT_FALSE(form_structure.IsStartOfAutofillableFlow());
  EXPECT_FALSE(form_structure.IsInAutofillableFlow());

  FormStructureTest::SetPageDetails(&form_structure, 0, 0);
  EXPECT_FALSE(form_structure.IsStartOfAutofillableFlow());
  EXPECT_FALSE(form_structure.IsInAutofillableFlow());

  FormStructureTest::SetPageDetails(&form_structure, 0, 1);
  EXPECT_TRUE(form_structure.IsStartOfAutofillableFlow());
  EXPECT_TRUE(form_structure.IsInAutofillableFlow());

  FormStructureTest::SetPageDetails(&form_structure, 1, 2);
  EXPECT_FALSE(form_structure.IsStartOfAutofillableFlow());
  EXPECT_TRUE(form_structure.IsInAutofillableFlow());

  FormStructureTest::SetPageDetails(&form_structure, 2, 2);
  EXPECT_FALSE(form_structure.IsStartOfAutofillableFlow());
  EXPECT_FALSE(form_structure.IsInAutofillableFlow());
}

TEST(FormStructureTest, AutofillCount) {
  FormData form;
  form.method = ASCIIToUTF16("post");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("state");
  field.name = ASCIIToUTF16("state");
  field.form_control_type = "select-one";
  form.fields.push_back(field);

  field.label = string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());

  // Only text and select fields that are heuristically matched are counted.
  EXPECT_EQ(1U, form_structure.autofill_count());
}

TEST(FormStructureTest, SourceURL) {
  FormData form;
  form.origin = GURL("http://www.foo.com/");
  form.method = ASCIIToUTF16("post");
  FormStructure form_structure(form);

  EXPECT_EQ(form.origin, form_structure.source_url());
}

TEST(FormStructureTest, IsAutofillable) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  // We need at least three text fields to be auto-fillable.
  form.method = ASCIIToUTF16("post");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("password");
  field.name = ASCIIToUTF16("password");
  field.form_control_type = "password";
  form.fields.push_back(field);

  field.label = string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_FALSE(form_structure->IsAutofillable(true));

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
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_FALSE(form_structure->IsAutofillable(true));

  // We now have three auto-fillable fields.
  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));

  // The method must be 'post', though we can intentionally ignore this
  // criterion for the sake of providing a helpful warning message to the user.
  form.method = ASCIIToUTF16("get");
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_FALSE(form_structure->IsAutofillable(true));
  EXPECT_TRUE(form_structure->IsAutofillable(false));

  // The target cannot include http(s)://*/search...
  form.method = ASCIIToUTF16("post");
  form.action = GURL("http://google.com/search?q=hello");
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_FALSE(form_structure->IsAutofillable(true));

  // But search can be in the URL.
  form.action = GURL("http://search.com/?q=hello");
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));
}

TEST(FormStructureTest, ShouldBeParsed) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  // We need at least three text fields to be parseable.
  form.method = ASCIIToUTF16("post");

  FormFieldData field;
  field.label = ASCIIToUTF16("username");
  field.name = ASCIIToUTF16("username");
  field.form_control_type = "text";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->ShouldBeParsed(true));

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
  EXPECT_TRUE(form_structure->ShouldBeParsed(true));

  // The method must be 'post', though we can intentionally ignore this
  // criterion for the sake of providing a helpful warning message to the user.
  form.method = ASCIIToUTF16("get");
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->IsAutofillable(true));
  EXPECT_TRUE(form_structure->ShouldBeParsed(false));

  // The target cannot include http(s)://*/search...
  form.method = ASCIIToUTF16("post");
  form.action = GURL("http://google.com/search?q=hello");
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->ShouldBeParsed(true));

  // But search can be in the URL.
  form.action = GURL("http://search.com/?q=hello");
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->ShouldBeParsed(true));

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
  EXPECT_TRUE(form_structure->ShouldBeParsed(true));

  form.fields[0].form_control_type = "select-one";
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->ShouldBeParsed(true));
}

TEST(FormStructureTest, HeuristicsContactInfo) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

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

  field.label = string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));

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
TEST(FormStructureTest, HeuristicsAutocompleteAttribute) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = string16();
  field.name = ASCIIToUTF16("field1");
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);

  field.label = string16();
  field.name = ASCIIToUTF16("field2");
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);

  field.label = string16();
  field.name = ASCIIToUTF16("field3");
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));

  // Expect the correct number of fields.
  ASSERT_EQ(3U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());
}

// Verify that we can correctly process the 'autocomplete' attribute for phone
// number types (especially phone prefixes and suffixes).
TEST(FormStructureTest, HeuristicsAutocompleteAttributePhoneTypes) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = string16();
  field.name = ASCIIToUTF16("field1");
  field.autocomplete_attribute = "tel-local";
  form.fields.push_back(field);

  field.label = string16();
  field.name = ASCIIToUTF16("field2");
  field.autocomplete_attribute = "tel-local-prefix";
  form.fields.push_back(field);

  field.label = string16();
  field.name = ASCIIToUTF16("field3");
  field.autocomplete_attribute = "tel-local-suffix";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));

  // Expect the correct number of fields.
  ASSERT_EQ(3U, form_structure->field_count());
  EXPECT_EQ(3U, form_structure->autofill_count());

  EXPECT_EQ(PHONE_HOME_NUMBER, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(AutofillField::IGNORED, form_structure->field(0)->phone_part());
  EXPECT_EQ(PHONE_HOME_NUMBER, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(AutofillField::PHONE_PREFIX,
            form_structure->field(1)->phone_part());
  EXPECT_EQ(PHONE_HOME_NUMBER, form_structure->field(2)->heuristic_type());
  EXPECT_EQ(AutofillField::PHONE_SUFFIX,
            form_structure->field(2)->phone_part());
}

// If at least one field includes type hints in the 'autocomplete' attribute, we
// should not try to apply any other heuristics.
TEST(FormStructureTest, AutocompleteAttributeOverridesOtherHeuristics) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

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
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));
  EXPECT_TRUE(form_structure->ShouldBeCrowdsourced());

  ASSERT_EQ(3U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());

  // Now update the first form field to include an 'autocomplete' attribute.
  form.fields.front().autocomplete_attribute = "x-other";
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_FALSE(form_structure->IsAutofillable(true));
  EXPECT_FALSE(form_structure->ShouldBeCrowdsourced());

  ASSERT_EQ(3U, form_structure->field_count());
  ASSERT_EQ(0U, form_structure->autofill_count());

  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(2)->heuristic_type());
}

// Verify that we can correctly process sections listed in the |autocomplete|
// attribute.
TEST(FormStructureTest, HeuristicsAutocompleteAttributeWithSections) {
  FormData form;
  form.method = ASCIIToUTF16("post");

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
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure.IsAutofillable(true));

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
TEST(FormStructureTest, HeuristicsAutocompleteAttributeWithSectionsDegenerate) {
  FormData form;
  form.method = ASCIIToUTF16("post");

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
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());

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
TEST(FormStructureTest, HeuristicsAutocompleteAttributeWithSectionsRepeated) {
  FormData form;
  form.method = ASCIIToUTF16("post");

  FormFieldData field;
  field.form_control_type = "text";

  field.autocomplete_attribute = "section-foo email";
  form.fields.push_back(field);
  field.autocomplete_attribute = "section-foo street-address";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());

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
TEST(FormStructureTest, HeuristicsDontOverrideAutocompleteAttributeSections) {
  FormData form;
  form.method = ASCIIToUTF16("post");

  FormFieldData field;
  field.form_control_type = "text";

  field.name = ASCIIToUTF16("one");
  field.autocomplete_attribute = "street-address";
  form.fields.push_back(field);
  field.name = string16();
  field.autocomplete_attribute = "section-foo email";
  form.fields.push_back(field);
  field.name = string16();
  field.autocomplete_attribute = "name";
  form.fields.push_back(field);
  field.name = ASCIIToUTF16("two");
  field.autocomplete_attribute = "street-address";
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes(TestAutofillMetrics());

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

TEST(FormStructureTest, HeuristicsSample8) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

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

  field.label = string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));
  ASSERT_EQ(10U, form_structure->field_count());
  ASSERT_EQ(9U, form_structure->autofill_count());

  // First name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_BILLING_LINE1, form_structure->field(2)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_BILLING_LINE2, form_structure->field(3)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_BILLING_CITY, form_structure->field(4)->heuristic_type());
  // State.
  EXPECT_EQ(ADDRESS_BILLING_STATE, form_structure->field(5)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_BILLING_ZIP, form_structure->field(6)->heuristic_type());
  // Country.
  EXPECT_EQ(ADDRESS_BILLING_COUNTRY,
      form_structure->field(7)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
      form_structure->field(8)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(9)->heuristic_type());
}

TEST(FormStructureTest, HeuristicsSample6) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

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

  field.label = string16();
  field.name = ASCIIToUTF16("Submit");
  field.value = ASCIIToUTF16("continue");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));
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
TEST(FormStructureTest, HeuristicsLabelsOnly) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Last Name");
  field.name = string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Email");
  field.name = string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Phone");
  field.name = string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address");
  field.name = string16();
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Zip code");
  field.name = string16();
  form.fields.push_back(field);

  field.label = string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));
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

TEST(FormStructureTest, HeuristicsCreditCardInfo) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

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

  field.label = string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));
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

TEST(FormStructureTest, HeuristicsCreditCardInfoWithUnknownCardField) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

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

  field.label = string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));
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

TEST(FormStructureTest, ThreeAddressLines) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

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
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));
  ASSERT_EQ(4U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  // Address Line 1.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(0)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(1)->heuristic_type());
  // Address Line 3.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(2)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(3)->heuristic_type());
}

// This test verifies that "addressLine1" and "addressLine2" matches heuristics.
// This occured in https://www.gorillaclothing.com/.  http://crbug.com/52126.
TEST(FormStructureTest, BillingAndShippingAddresses) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("Address Line1");
  field.name = ASCIIToUTF16("shipping.address.addressLine1");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address Line2");
  field.name = ASCIIToUTF16("shipping.address.addressLine2");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address Line1");
  field.name = ASCIIToUTF16("billing.address.addressLine1");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("Address Line2");
  field.name = ASCIIToUTF16("billing.address.addressLine2");
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));
  ASSERT_EQ(4U, form_structure->field_count());
  ASSERT_EQ(4U, form_structure->autofill_count());

  // Address Line 1.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(0)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(1)->heuristic_type());
  // Address Line 1.
  EXPECT_EQ(ADDRESS_BILLING_LINE1, form_structure->field(2)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_BILLING_LINE2, form_structure->field(3)->heuristic_type());
}


// This example comes from expedia.com where they use a "Suite" label to
// indicate a suite or apartment number.  We interpret this as address line 2.
// And the following "Street address second line" we interpret as address line
// 3 and discard.
// See http://crbug.com/48197 for details.
TEST(FormStructureTest, ThreeAddressLinesExpedia) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

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
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));
  ASSERT_EQ(4U, form_structure->field_count());
  EXPECT_EQ(3U, form_structure->autofill_count());

  // Address Line 1.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(0)->heuristic_type());
  // Suite / Apt.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(1)->heuristic_type());
  // Address Line 3.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(2)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(3)->heuristic_type());
}

// This example comes from ebay.com where the word "suite" appears in the label
// and the name "address2" clearly indicates that this is the address line 2.
// See http://crbug.com/48197 for details.
TEST(FormStructureTest, TwoAddressLinesEbay) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

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
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));
  ASSERT_EQ(3U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

  // Address Line 1.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(0)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(1)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(2)->heuristic_type());
}

TEST(FormStructureTest, HeuristicsStateWithProvince) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

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
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));
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
TEST(FormStructureTest, HeuristicsWithBilling) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

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
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));
  ASSERT_EQ(11U, form_structure->field_count());
  ASSERT_EQ(11U, form_structure->autofill_count());

  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  EXPECT_EQ(COMPANY_NAME, form_structure->field(2)->heuristic_type());
  EXPECT_EQ(ADDRESS_BILLING_LINE1, form_structure->field(3)->heuristic_type());
  EXPECT_EQ(ADDRESS_BILLING_LINE2, form_structure->field(4)->heuristic_type());
  EXPECT_EQ(ADDRESS_BILLING_CITY, form_structure->field(5)->heuristic_type());
  EXPECT_EQ(ADDRESS_BILLING_STATE, form_structure->field(6)->heuristic_type());
  EXPECT_EQ(ADDRESS_BILLING_COUNTRY,
            form_structure->field(7)->heuristic_type());
  EXPECT_EQ(ADDRESS_BILLING_ZIP, form_structure->field(8)->heuristic_type());
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
            form_structure->field(9)->heuristic_type());
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(10)->heuristic_type());
}

TEST(FormStructureTest, ThreePartPhoneNumber) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

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
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));
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

TEST(FormStructureTest, HeuristicsInfernoCC) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

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
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));

  // Expect the correct number of fields.
  ASSERT_EQ(5U, form_structure->field_count());
  EXPECT_EQ(5U, form_structure->autofill_count());

  // Name on Card.
  EXPECT_EQ(CREDIT_CARD_NAME, form_structure->field(0)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_BILLING_LINE1, form_structure->field(1)->heuristic_type());
  // Card Number.
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(2)->heuristic_type());
  // Expiration Date.
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(3)->heuristic_type());
  // Expiration Year.
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(4)->heuristic_type());
}

TEST(FormStructureTest, CVCCodeClash) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

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

  field.label = string16();
  field.name = ASCIIToUTF16("ccexpiresyear");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("cvc number");
  field.name = ASCIIToUTF16("csc");
  form.fields.push_back(field);

  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());
  EXPECT_TRUE(form_structure->IsAutofillable(true));

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

TEST(FormStructureTest, EncodeQueryRequest) {
  FormData form;
  form.method = ASCIIToUTF16("post");

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

  ScopedVector<FormStructure> forms;
  forms.push_back(new FormStructure(form));
  std::vector<std::string> encoded_signatures;
  std::string encoded_xml;
  const char * const kSignature1 = "11337937696949187602";
  const char * const kResponse1 =
      "<\?xml version=\"1.0\" encoding=\"UTF-8\"\?><autofillquery "
      "clientversion=\"6.1.1715.1442/en (GGLL)\" accepts=\"e\"><form "
      "signature=\"11337937696949187602\"><field signature=\"412125936\"/>"
      "<field signature=\"1917667676\"/><field signature=\"2226358947\"/>"
      "<field signature=\"747221617\"/><field signature=\"4108155786\"/></form>"
      "</autofillquery>";
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms.get(),
                                                &encoded_signatures,
                                                &encoded_xml));
  ASSERT_EQ(1U, encoded_signatures.size());
  EXPECT_EQ(kSignature1, encoded_signatures[0]);
  EXPECT_EQ(kResponse1, encoded_xml);

  // Add the same form, only one will be encoded, so EncodeQueryRequest() should
  // return the same data.
  forms.push_back(new FormStructure(form));
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms.get(),
                                                &encoded_signatures,
                                                &encoded_xml));
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
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms.get(),
                                                &encoded_signatures,
                                                &encoded_xml));
  ASSERT_EQ(2U, encoded_signatures.size());
  EXPECT_EQ(kSignature1, encoded_signatures[0]);
  const char * const kSignature2 = "8308881815906226214";
  EXPECT_EQ(kSignature2, encoded_signatures[1]);
  const char * const kResponse2 =
      "<\?xml version=\"1.0\" encoding=\"UTF-8\"\?><autofillquery "
      "clientversion=\"6.1.1715.1442/en (GGLL)\" accepts=\"e\"><form "
      "signature=\"11337937696949187602\"><field signature=\"412125936\"/>"
      "<field signature=\"1917667676\"/><field signature=\"2226358947\"/>"
      "<field signature=\"747221617\"/><field signature=\"4108155786\"/></form>"
      "<form signature=\"8308881815906226214\"><field signature=\"412125936\"/>"
      "<field signature=\"1917667676\"/><field signature=\"2226358947\"/>"
      "<field signature=\"747221617\"/><field signature=\"4108155786\"/><field "
      "signature=\"509334676\"/><field signature=\"509334676\"/><field "
      "signature=\"509334676\"/><field signature=\"509334676\"/><field "
      "signature=\"509334676\"/></form></autofillquery>";
  EXPECT_EQ(kResponse2, encoded_xml);

  // Add 50 address fields - the form is not valid anymore, but previous ones
  // are. The result should be the same as in previous test.
  for (size_t i = 0; i < 50; ++i) {
    field.label = ASCIIToUTF16("Address");
    field.name = ASCIIToUTF16("address");
    form.fields.push_back(field);
  }

  forms.push_back(new FormStructure(form));
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms.get(),
                                                &encoded_signatures,
                                                &encoded_xml));
  ASSERT_EQ(2U, encoded_signatures.size());
  EXPECT_EQ(kSignature1, encoded_signatures[0]);
  EXPECT_EQ(kSignature2, encoded_signatures[1]);
  EXPECT_EQ(kResponse2, encoded_xml);

  // Check that we fail if there are only bad form(s).
  ScopedVector<FormStructure> bad_forms;
  bad_forms.push_back(new FormStructure(form));
  EXPECT_FALSE(FormStructure::EncodeQueryRequest(bad_forms.get(),
                                                 &encoded_signatures,
                                                 &encoded_xml));
  EXPECT_EQ(0U, encoded_signatures.size());
  EXPECT_EQ("", encoded_xml);
}

TEST(FormStructureTest, EncodeUploadRequest) {
  scoped_ptr<FormStructure> form_structure;
  std::vector<FieldTypeSet> possible_field_types;
  FormData form;
  form.method = ASCIIToUTF16("post");
  form_structure.reset(new FormStructure(form));
  form_structure->DetermineHeuristicTypes(TestAutofillMetrics());

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("firstname");
  form.fields.push_back(field);
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(NAME_FIRST);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("lastname");
  form.fields.push_back(field);
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(NAME_LAST);

  field.label = ASCIIToUTF16("Email");
  field.name = ASCIIToUTF16("email");
  field.form_control_type = "email";
  form.fields.push_back(field);
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(EMAIL_ADDRESS);

  field.label = ASCIIToUTF16("Phone");
  field.name = ASCIIToUTF16("phone");
  field.form_control_type = "number";
  form.fields.push_back(field);
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(PHONE_HOME_WHOLE_NUMBER);

  field.label = ASCIIToUTF16("Country");
  field.name = ASCIIToUTF16("country");
  field.form_control_type = "select-one";
  form.fields.push_back(field);
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(ADDRESS_HOME_COUNTRY);
  form_structure.reset(new FormStructure(form));

  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->field(i)->set_possible_types(possible_field_types[i]);

  FieldTypeSet available_field_types;
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
                                                  &encoded_xml));
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\" "
            "formsignature=\"8736493185895608956\" autofillused=\"false\" "
            "datapresent=\"144200030e\">"
            "<field signature=\"3763331450\" autofilltype=\"3\"/>"
            "<field signature=\"3494530716\" autofilltype=\"5\"/>"
            "<field signature=\"1029417091\" autofilltype=\"9\"/>"
            "<field signature=\"466116101\" autofilltype=\"14\"/>"
            "<field signature=\"2799270304\" autofilltype=\"36\"/>"
            "</autofillupload>",
            encoded_xml);
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, true,
                                                  &encoded_xml));
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\" "
            "formsignature=\"8736493185895608956\" autofillused=\"true\" "
            "datapresent=\"144200030e\">"
            "<field signature=\"3763331450\" autofilltype=\"3\"/>"
            "<field signature=\"3494530716\" autofilltype=\"5\"/>"
            "<field signature=\"1029417091\" autofilltype=\"9\"/>"
            "<field signature=\"466116101\" autofilltype=\"14\"/>"
            "<field signature=\"2799270304\" autofilltype=\"36\"/>"
            "</autofillupload>",
            encoded_xml);

  // Add 2 address fields - this should be still a valid form.
  for (size_t i = 0; i < 2; ++i) {
    field.label = ASCIIToUTF16("Address");
    field.name = ASCIIToUTF16("address");
    field.form_control_type = "text";
    form.fields.push_back(field);
    possible_field_types.push_back(FieldTypeSet());
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
                                                  &encoded_xml));
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\" "
            "formsignature=\"7816485729218079147\" autofillused=\"false\" "
            "datapresent=\"144200030e\">"
            "<field signature=\"3763331450\" autofilltype=\"3\"/>"
            "<field signature=\"3494530716\" autofilltype=\"5\"/>"
            "<field signature=\"1029417091\" autofilltype=\"9\"/>"
            "<field signature=\"466116101\" autofilltype=\"14\"/>"
            "<field signature=\"2799270304\" autofilltype=\"36\"/>"
            "<field signature=\"509334676\" autofilltype=\"30\"/>"
            "<field signature=\"509334676\" autofilltype=\"31\"/>"
            "<field signature=\"509334676\" autofilltype=\"37\"/>"
            "<field signature=\"509334676\" autofilltype=\"38\"/>"
            "<field signature=\"509334676\" autofilltype=\"30\"/>"
            "<field signature=\"509334676\" autofilltype=\"31\"/>"
            "<field signature=\"509334676\" autofilltype=\"37\"/>"
            "<field signature=\"509334676\" autofilltype=\"38\"/>"
            "</autofillupload>",
            encoded_xml);

  // Add 50 address fields - now the form is invalid, as it has too many fields.
  for (size_t i = 0; i < 50; ++i) {
    field.label = ASCIIToUTF16("Address");
    field.name = ASCIIToUTF16("address");
    field.form_control_type = "text";
    form.fields.push_back(field);
    possible_field_types.push_back(FieldTypeSet());
    possible_field_types.back().insert(ADDRESS_HOME_LINE1);
    possible_field_types.back().insert(ADDRESS_HOME_LINE2);
    possible_field_types.back().insert(ADDRESS_BILLING_LINE1);
    possible_field_types.back().insert(ADDRESS_BILLING_LINE2);
  }
  form_structure.reset(new FormStructure(form));
  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
  EXPECT_FALSE(form_structure->EncodeUploadRequest(available_field_types, false,
                                                   &encoded_xml));
}

// Check that we compute the "datapresent" string correctly for the given
// |available_types|.
TEST(FormStructureTest, CheckDataPresence) {
  FormData form;
  form.method = ASCIIToUTF16("post");

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

  FieldTypeSet unknown_type;
  unknown_type.insert(UNKNOWN_TYPE);
  for (size_t i = 0; i < form_structure.field_count(); ++i)
    form_structure.field(i)->set_possible_types(unknown_type);

  // No available types.
  // datapresent should be "" == trimmmed(0x0000000000000000) ==
  //     0b0000000000000000000000000000000000000000000000000000000000000000
  FieldTypeSet available_field_types;

  std::string encoded_xml;
  EXPECT_TRUE(form_structure.EncodeUploadRequest(available_field_types, false,
                                                 &encoded_xml));
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"6402244543831589061\" autofillused=\"false\""
            " datapresent=\"\">"
            "<field signature=\"1089846351\" autofilltype=\"1\"/>"
            "<field signature=\"2404144663\" autofilltype=\"1\"/>"
            "<field signature=\"420638584\" autofilltype=\"1\"/>"
            "</autofillupload>",
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
                                                 &encoded_xml));
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"6402244543831589061\" autofillused=\"false\""
            " datapresent=\"1540000240\">"
            "<field signature=\"1089846351\" autofilltype=\"1\"/>"
            "<field signature=\"2404144663\" autofilltype=\"1\"/>"
            "<field signature=\"420638584\" autofilltype=\"1\"/>"
            "</autofillupload>",
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
                                                 &encoded_xml));
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"6402244543831589061\" autofillused=\"false\""
            " datapresent=\"1f7e000378000008\">"
            "<field signature=\"1089846351\" autofilltype=\"1\"/>"
            "<field signature=\"2404144663\" autofilltype=\"1\"/>"
            "<field signature=\"420638584\" autofilltype=\"1\"/>"
            "</autofillupload>",
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
                                                 &encoded_xml));
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"6402244543831589061\" autofillused=\"false\""
            " datapresent=\"0000000000001fc0\">"
            "<field signature=\"1089846351\" autofilltype=\"1\"/>"
            "<field signature=\"2404144663\" autofilltype=\"1\"/>"
            "<field signature=\"420638584\" autofilltype=\"1\"/>"
            "</autofillupload>",
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
                                                 &encoded_xml));
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"6402244543831589061\" autofillused=\"false\""
            " datapresent=\"1f7e000378001fc8\">"
            "<field signature=\"1089846351\" autofilltype=\"1\"/>"
            "<field signature=\"2404144663\" autofilltype=\"1\"/>"
            "<field signature=\"420638584\" autofilltype=\"1\"/>"
            "</autofillupload>",
            encoded_xml);
}

TEST(FormStructureTest, CheckMultipleTypes) {
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
  FieldTypeSet available_field_types;
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
  std::vector<FieldTypeSet> possible_field_types;
  FormData form;
  form.method = ASCIIToUTF16("post");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(EMAIL_ADDRESS);

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("first");
  form.fields.push_back(field);
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(NAME_FIRST);

  field.label = ASCIIToUTF16("Last Name");
  field.name = ASCIIToUTF16("last");
  form.fields.push_back(field);
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(NAME_LAST);

  field.label = ASCIIToUTF16("Address");
  field.name = ASCIIToUTF16("address");
  form.fields.push_back(field);
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(ADDRESS_HOME_LINE1);

  form_structure.reset(new FormStructure(form));

  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->field(i)->set_possible_types(possible_field_types[i]);
  std::string encoded_xml;

  // Now we matched both fields singularly.
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  &encoded_xml));
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"18062476096658145866\" autofillused=\"false\""
            " datapresent=\"1440000360000008\">"
            "<field signature=\"420638584\" autofilltype=\"9\"/>"
            "<field signature=\"1089846351\" autofilltype=\"3\"/>"
            "<field signature=\"2404144663\" autofilltype=\"5\"/>"
            "<field signature=\"509334676\" autofilltype=\"30\"/>"
            "</autofillupload>",
            encoded_xml);
  // Match third field as both first and last.
  possible_field_types[2].insert(NAME_FIRST);
  form_structure->field(2)->set_possible_types(possible_field_types[2]);
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  &encoded_xml));
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"18062476096658145866\" autofillused=\"false\""
            " datapresent=\"1440000360000008\">"
            "<field signature=\"420638584\" autofilltype=\"9\"/>"
            "<field signature=\"1089846351\" autofilltype=\"3\"/>"
            "<field signature=\"2404144663\" autofilltype=\"3\"/>"
            "<field signature=\"2404144663\" autofilltype=\"5\"/>"
            "<field signature=\"509334676\" autofilltype=\"30\"/>"
            "</autofillupload>",
            encoded_xml);
  possible_field_types[3].insert(ADDRESS_HOME_LINE2);
  form_structure->field(form_structure->field_count() - 1)->set_possible_types(
      possible_field_types[form_structure->field_count() - 1]);
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  &encoded_xml));
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"18062476096658145866\" autofillused=\"false\""
            " datapresent=\"1440000360000008\">"
            "<field signature=\"420638584\" autofilltype=\"9\"/>"
            "<field signature=\"1089846351\" autofilltype=\"3\"/>"
            "<field signature=\"2404144663\" autofilltype=\"3\"/>"
            "<field signature=\"2404144663\" autofilltype=\"5\"/>"
            "<field signature=\"509334676\" autofilltype=\"30\"/>"
            "<field signature=\"509334676\" autofilltype=\"31\"/>"
            "</autofillupload>",
            encoded_xml);
  possible_field_types[3].clear();
  possible_field_types[3].insert(ADDRESS_HOME_LINE1);
  possible_field_types[3].insert(COMPANY_NAME);
  form_structure->field(form_structure->field_count() - 1)->set_possible_types(
      possible_field_types[form_structure->field_count() - 1]);
  EXPECT_TRUE(form_structure->EncodeUploadRequest(available_field_types, false,
                                                  &encoded_xml));
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?>"
            "<autofillupload clientversion=\"6.1.1715.1442/en (GGLL)\""
            " formsignature=\"18062476096658145866\" autofillused=\"false\""
            " datapresent=\"1440000360000008\">"
            "<field signature=\"420638584\" autofilltype=\"9\"/>"
            "<field signature=\"1089846351\" autofilltype=\"3\"/>"
            "<field signature=\"2404144663\" autofilltype=\"3\"/>"
            "<field signature=\"2404144663\" autofilltype=\"5\"/>"
            "<field signature=\"509334676\" autofilltype=\"30\"/>"
            "<field signature=\"509334676\" autofilltype=\"60\"/>"
            "</autofillupload>",
            encoded_xml);
}

TEST(FormStructureTest, CheckFormSignature) {
  // Check that form signature is created correctly.
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

  FormFieldData field;
  field.form_control_type = "text";

  field.label = ASCIIToUTF16("email");
  field.name = ASCIIToUTF16("email");
  form.fields.push_back(field);

  field.label = ASCIIToUTF16("First Name");
  field.name = ASCIIToUTF16("first");
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
}

TEST(FormStructureTest, ToFormData) {
  FormData form;
  form.name = ASCIIToUTF16("the-name");
  form.method = ASCIIToUTF16("POST");
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

  field.label = string16();
  field.name = ASCIIToUTF16("Submit");
  field.form_control_type = "submit";
  form.fields.push_back(field);

  EXPECT_EQ(form, FormStructure(form).ToFormData());

  // Currently |FormStructure(form_data)ToFormData().user_submitted| is always
  // false. This forces a future author that changes this to update this test.
  form.user_submitted = true;
  EXPECT_NE(form, FormStructure(form).ToFormData());
}
