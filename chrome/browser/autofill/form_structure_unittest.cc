// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/form_structure.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputElement.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"

using webkit_glue::FormData;
using WebKit::WebInputElement;

namespace webkit_glue {

std::ostream& operator<<(std::ostream& os, const FormData& form) {
  os << UTF16ToUTF8(form.name)
     << " "
     << UTF16ToUTF8(form.method)
     << " "
     << form.origin.spec()
     << " "
     << form.action.spec()
     << " ";

  for (std::vector<webkit_glue::FormField>::const_iterator iter =
           form.fields.begin();
       iter != form.fields.end(); ++iter) {
    os << *iter
       << " ";
  }

  return os;
}

}  // namespace webkit_glue

namespace {

TEST(FormStructureTest, FieldCount) {
  FormData form;
  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("username"),
                                               ASCIIToUTF16("username"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("password"),
                                               ASCIIToUTF16("password"),
                                               string16(),
                                               ASCIIToUTF16("password"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit"),
                                               0,
                                               false));
  FormStructure form_structure(form);

  // All fields are counted.
  EXPECT_EQ(3U, form_structure.field_count());
}

TEST(FormStructureTest, AutoFillCount) {
  FormData form;
  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("username"),
                                               ASCIIToUTF16("username"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("password"),
                                               ASCIIToUTF16("password"),
                                               string16(),
                                               ASCIIToUTF16("password"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("state"),
                                               ASCIIToUTF16("state"),
                                               string16(),
                                               ASCIIToUTF16("select-one"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit"),
                                               0,
                                               false));
  FormStructure form_structure(form);

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

TEST(FormStructureTest, HasAutoFillableValues) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");

  // Non text/select fields are not used when saving auto fill data.
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit1"),
                                               string16(),
                                               ASCIIToUTF16("submit"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit2"),
                                               ASCIIToUTF16("dummy value"),
                                               ASCIIToUTF16("submit"),
                                               0,
                                               false));
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->HasAutoFillableValues());

  // Empty text/select fields are also not used when saving auto fill data.
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Email"),
                                               ASCIIToUTF16("email"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("state"),
                                               ASCIIToUTF16("state"),
                                               string16(),
                                               ASCIIToUTF16("select-one"),
                                               0,
                                               false));
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->HasAutoFillableValues());

  // Non-empty fields can be saved in auto fill profile.
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                               ASCIIToUTF16("firstname"),
                                               ASCIIToUTF16("John"),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                               ASCIIToUTF16("lastname"),
                                               ASCIIToUTF16("Dear"),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->HasAutoFillableValues());

  // The fields must be recognized heuristically by AutoFill in addition to
  // being text or select and non-empty.
  form.fields.clear();
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Field1"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Field2"),
                                               ASCIIToUTF16("dummy value"),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->HasAutoFillableValues());

  // Add a field that we match heuristically, verify that this form has
  // auto-fillable values.
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Full Name"),
                                               ASCIIToUTF16("fullname"),
                                               ASCIIToUTF16("John Dear"),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->HasAutoFillableValues());
}

TEST(FormStructureTest, IsAutoFillable) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  // We need at least three text fields to be auto-fillable.
  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("username"),
                                               ASCIIToUTF16("username"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("password"),
                                               ASCIIToUTF16("password"),
                                               string16(),
                                               ASCIIToUTF16("password"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit"),
                                               0,
                                               false));
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->IsAutoFillable(true));

  // We now have three text fields, but only two auto-fillable fields.
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                               ASCIIToUTF16("firstname"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                               ASCIIToUTF16("lastname"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->IsAutoFillable(true));

  // We now have three auto-fillable fields.
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Email"),
                                               ASCIIToUTF16("email"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));

  // The method must be 'post', though we can intentionally ignore this
  // criterion for the sake of providing a helpful warning message to the user.
  form.method = ASCIIToUTF16("get");
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->IsAutoFillable(true));
  EXPECT_TRUE(form_structure->IsAutoFillable(false));

  // The target cannot include http(s)://*/search...
  form.method = ASCIIToUTF16("post");
  form.action = GURL("http://google.com/search?q=hello");
  form_structure.reset(new FormStructure(form));
  EXPECT_FALSE(form_structure->IsAutoFillable(true));

  // But search can be in the URL.
  form.action = GURL("http://search.com/?q=hello");
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));
}

TEST(FormStructureTest, HeuristicsContactInfo) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                               ASCIIToUTF16("firstname"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                               ASCIIToUTF16("lastname"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("EMail"),
                                               ASCIIToUTF16("email"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Phone"),
                                               ASCIIToUTF16("phone"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Fax"),
                                               ASCIIToUTF16("fax"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                               ASCIIToUTF16("address"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("City"),
                                               ASCIIToUTF16("city"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Zip code"),
                                               ASCIIToUTF16("zipcode"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit"),
                                               0,
                                               false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));

  // Expect the correct number of fields.
  ASSERT_EQ(9U, form_structure->field_count());
  ASSERT_EQ(8U, form_structure->autofill_count());

  // First name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  // Email.
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
      form_structure->field(3)->heuristic_type());
  // Fax.
  EXPECT_EQ(PHONE_FAX_WHOLE_NUMBER, form_structure->field(4)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(5)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(6)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(7)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(8)->heuristic_type());
}

TEST(FormStructureTest, HeuristicsHiddenFields) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                               ASCIIToUTF16("firstname"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("hidden1"),
                                               string16(),
                                               ASCIIToUTF16("hidden"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                               ASCIIToUTF16("lastname"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("hidden2"),
                                               string16(),
                                               ASCIIToUTF16("hidden"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("EMail"),
                                               ASCIIToUTF16("email"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("hidden3"),
                                               string16(),
                                               ASCIIToUTF16("hidden"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Phone"),
                                               ASCIIToUTF16("phone"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("hidden4"),
                                               string16(),
                                               ASCIIToUTF16("hidden"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Fax"),
                                               ASCIIToUTF16("fax"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("hidden5"),
                                               string16(),
                                               ASCIIToUTF16("hidden"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                               ASCIIToUTF16("address"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("hidden6"),
                                               string16(),
                                               ASCIIToUTF16("hidden"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("City"),
                                               ASCIIToUTF16("city"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("hidden7"),
                                               string16(),
                                               ASCIIToUTF16("hidden"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Zip code"),
                                               ASCIIToUTF16("zipcode"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("hidden8"),
                                               string16(),
                                               ASCIIToUTF16("hidden"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit"),
                                               0,
                                               false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));

  // Expect the correct number of fields.
  ASSERT_EQ(17U, form_structure->field_count());
  ASSERT_EQ(8U, form_structure->autofill_count());

  // First name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(NAME_LAST, form_structure->field(2)->heuristic_type());
  // Email.
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(4)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
      form_structure->field(6)->heuristic_type());
  // Fax.
  EXPECT_EQ(PHONE_FAX_WHOLE_NUMBER, form_structure->field(8)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(10)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(12)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(14)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(16)->heuristic_type());
}

TEST(FormStructureTest, HeuristicsSample8) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  form.method = ASCIIToUTF16("post");
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Your First Name:"),
                             ASCIIToUTF16("bill.first"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Your Last Name:"),
                             ASCIIToUTF16("bill.last"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Street Address Line 1:"),
                             ASCIIToUTF16("bill.street1"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Street Address Line 2:"),
                             ASCIIToUTF16("bill.street2"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("City:"),
                             ASCIIToUTF16("bill.city"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("State (U.S.):"),
                             ASCIIToUTF16("bill.state"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Zip/Postal Code:"),
                             ASCIIToUTF16("BillTo.PostalCode"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Country:"),
                             ASCIIToUTF16("bill.country"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Phone Number:"),
                             ASCIIToUTF16("BillTo.Phone"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(string16(),
                             ASCIIToUTF16("Submit"),
                             string16(),
                             ASCIIToUTF16("submit"),
                             0,
                             false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));
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
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("E-mail address"),
                             ASCIIToUTF16("email"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Full name"),
                             ASCIIToUTF16("name"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Company"),
                             ASCIIToUTF16("company"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Address"),
                             ASCIIToUTF16("address"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("City"),
                             ASCIIToUTF16("city"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  // TODO(jhawkins): Add state select control.
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Zip Code"),
                             ASCIIToUTF16("Home.PostalCode"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  // TODO(jhawkins): Phone number.
  form.fields.push_back(
      webkit_glue::FormField(string16(),
                             ASCIIToUTF16("Submit"),
                             ASCIIToUTF16("continue"),
                             ASCIIToUTF16("submit"),
                             0,
                             false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));
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
// for matching.  This works because FormField labels are matched in the case
// that input element ids (or |name| fields) are missing.
TEST(FormStructureTest, HeuristicsLabelsOnly) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("EMail"),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Phone"),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Fax"),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Zip code"),
                                               string16(),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit"),
                                               0,
                                               false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));
  ASSERT_EQ(9U, form_structure->field_count());
  ASSERT_EQ(8U, form_structure->autofill_count());

  // First name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  // Email.
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
      form_structure->field(3)->heuristic_type());
  // Fax.
  EXPECT_EQ(PHONE_FAX_WHOLE_NUMBER, form_structure->field(4)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(5)->heuristic_type());
  // Address Line 2.
  EXPECT_EQ(ADDRESS_HOME_LINE2, form_structure->field(6)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(7)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(8)->heuristic_type());
}

TEST(FormStructureTest, HeuristicsCreditCardInfo) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Name on Card"),
                                               ASCIIToUTF16("name on card"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Card Number"),
                                               ASCIIToUTF16("card_number"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Exp Month"),
                                               ASCIIToUTF16("ccmonth"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Exp Year"),
                                               ASCIIToUTF16("ccyear"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Verification"),
                                               ASCIIToUTF16("verification"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit"),
                                               0,
                                               false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));
  ASSERT_EQ(6U, form_structure->field_count());
  ASSERT_EQ(4U, form_structure->autofill_count());

  // Credit card name.
  EXPECT_EQ(CREDIT_CARD_NAME, form_structure->field(0)->heuristic_type());
  // Credit card number.
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure->field(1)->heuristic_type());
  // Credit card expiration month.
  EXPECT_EQ(CREDIT_CARD_EXP_MONTH, form_structure->field(2)->heuristic_type());
  // Credit card expiration year.
  EXPECT_EQ(CREDIT_CARD_EXP_4_DIGIT_YEAR,
            form_structure->field(3)->heuristic_type());
  // We don't determine CVV.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(4)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(5)->heuristic_type());
}

TEST(FormStructureTest, HeuristicsCreditCardInfoWithUnknownCardField) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Name on Card"),
                                               ASCIIToUTF16("name on card"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  // This is not a field we know how to process.  But we should skip over it
  // and process the other fields in the card block.
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Card Type"),
                                               ASCIIToUTF16("card_type"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Card Number"),
                                               ASCIIToUTF16("card_number"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Exp Month"),
                                               ASCIIToUTF16("ccmonth"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Exp Year"),
                                               ASCIIToUTF16("ccyear"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Verification"),
                                               ASCIIToUTF16("verification"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit"),
                                               0,
                                               false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));
  ASSERT_EQ(7U, form_structure->field_count());
  ASSERT_EQ(4U, form_structure->autofill_count());

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
  // We don't determine CVV.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(5)->heuristic_type());
  // Submit.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(6)->heuristic_type());
}

TEST(FormStructureTest, ThreeAddressLines) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;

  form.method = ASCIIToUTF16("post");
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Address Line1"),
                             ASCIIToUTF16("Address"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Address Line2"),
                             ASCIIToUTF16("Address"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Address Line3"),
                             ASCIIToUTF16("Address"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("City"),
                             ASCIIToUTF16("city"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));
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
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Address Line1"),
                             ASCIIToUTF16("shipping.address.addressLine1"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Address Line2"),
                             ASCIIToUTF16("shipping.address.addressLine2"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Address Line1"),
                             ASCIIToUTF16("billing.address.addressLine1"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Address Line2"),
                             ASCIIToUTF16("billing.address.addressLine2"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));
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
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Street:"),
                             ASCIIToUTF16("FOPIH_RgWebCC_0_IHAddress_ads1"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Suite or Apt:"),
                             ASCIIToUTF16("FOPIH_RgWebCC_0_IHAddress_adap"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Street address second line"),
                             ASCIIToUTF16("FOPIH_RgWebCC_0_IHAddress_ads2"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("City:"),
                             ASCIIToUTF16("FOPIH_RgWebCC_0_IHAddress_adct"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));
  ASSERT_EQ(4U, form_structure->field_count());
  ASSERT_EQ(3U, form_structure->autofill_count());

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
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Address Line1"),
                             ASCIIToUTF16("address1"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Floor number, suite number, etc"),
                             ASCIIToUTF16("address2"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("City"),
                             ASCIIToUTF16("city"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));
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
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Address Line1"),
                             ASCIIToUTF16("Address"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Address Line2"),
                             ASCIIToUTF16("Address"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("State/Province/Region"),
                             ASCIIToUTF16("State"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));
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
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("First Name*:"),
                             ASCIIToUTF16("editBillingAddress$firstNameBox"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Last Name*:"),
                             ASCIIToUTF16("editBillingAddress$lastNameBox"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Company Name:"),
                             ASCIIToUTF16("editBillingAddress$companyBox"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Address*:"),
                             ASCIIToUTF16("editBillingAddress$addressLine1Box"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Apt/Suite :"),
                             ASCIIToUTF16("editBillingAddress$addressLine2Box"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("City*:"),
                             ASCIIToUTF16("editBillingAddress$cityBox"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("State/Province*:"),
                             ASCIIToUTF16("editBillingAddress$stateDropDown"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Country*:"),
                             ASCIIToUTF16("editBillingAddress$countryDropDown"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Postal Code*:"),
                             ASCIIToUTF16("editBillingAddress$zipCodeBox"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Phone*:"),
                             ASCIIToUTF16("editBillingAddress$phoneBox"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Email Address*:"),
                             ASCIIToUTF16("email$emailBox"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));
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
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Phone:"),
                             ASCIIToUTF16("dayphone1"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("-"),
                             ASCIIToUTF16("dayphone2"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("-"),
                             ASCIIToUTF16("dayphone3"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form.fields.push_back(
      webkit_glue::FormField(ASCIIToUTF16("ext.:"),
                             ASCIIToUTF16("dayphone4"),
                             string16(),
                             ASCIIToUTF16("text"),
                             0,
                             false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));
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

TEST(FormStructureTest, MatchSpecificInputTypes) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                               ASCIIToUTF16("firstname"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                               ASCIIToUTF16("lastname"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("EMail"),
                                               ASCIIToUTF16("email"),
                                               string16(),
                                               ASCIIToUTF16("email"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Phone"),
                                               ASCIIToUTF16("phone"),
                                               string16(),
                                               ASCIIToUTF16("number"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Country"),
                                               ASCIIToUTF16("country"),
                                               string16(),
                                               ASCIIToUTF16("select-one"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Fax"),
                                               ASCIIToUTF16("fax"),
                                               string16(),
                                               ASCIIToUTF16("tel"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                               ASCIIToUTF16("address"),
                                               string16(),
                                               ASCIIToUTF16("radio"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("City"),
                                               ASCIIToUTF16("city"),
                                               string16(),
                                               ASCIIToUTF16("checkbox"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("State"),
                                               ASCIIToUTF16("state"),
                                               string16(),
                                               ASCIIToUTF16("hidden"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(string16(),
                                               ASCIIToUTF16("Submit"),
                                               string16(),
                                               ASCIIToUTF16("submit"),
                                               0,
                                               false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));

  // Expect the correct number of fields.
  ASSERT_EQ(10U, form_structure->field_count());
  ASSERT_EQ(6U, form_structure->autofill_count());

  // First name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  // Email.
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
            form_structure->field(3)->heuristic_type());
  // Country.
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, form_structure->field(4)->heuristic_type());
  // Fax.
  EXPECT_EQ(PHONE_FAX_WHOLE_NUMBER, form_structure->field(5)->heuristic_type());
  // Address.  Invalid input type.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(6)->heuristic_type());
  // City.  Invalid input type.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(7)->heuristic_type());
  // State.  Invalid input type.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(8)->heuristic_type());
  // Submit.  Invalid input type.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(9)->heuristic_type());
}

TEST(FormStructureTest, HeuristicsInfernoCC) {
  scoped_ptr<FormStructure> form_structure;
  FormData form;
  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Name on Card"),
                                               ASCIIToUTF16("name_on_card"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                               ASCIIToUTF16("billing_address"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Card Number"),
                                               ASCIIToUTF16("card_number"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Expiration Date"),
                                               ASCIIToUTF16("expiration_month"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Expiration Year"),
                                               ASCIIToUTF16("expiration_year"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));

  // Expect the correct number of fields.
  ASSERT_EQ(5U, form_structure->field_count());
  ASSERT_EQ(5U, form_structure->autofill_count());

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
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Card number"),
                                               ASCIIToUTF16("ccnumber"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("First name"),
                                               ASCIIToUTF16("first_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Last name"),
                                               ASCIIToUTF16("last_name"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Expiration date"),
                                               ASCIIToUTF16("ccexpiresmonth"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16(""),
                                               ASCIIToUTF16("ccexpiresyear"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("cvc number"),
                                               ASCIIToUTF16("csc"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form_structure.reset(new FormStructure(form));
  EXPECT_TRUE(form_structure->IsAutoFillable(true));

  // Expect the correct number of fields.
  ASSERT_EQ(6U, form_structure->field_count());
  ASSERT_EQ(4U, form_structure->autofill_count());

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
  // CVC code should not match.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(5)->heuristic_type());
}

TEST(FormStructureTest, EncodeQueryRequest) {
  FormData form;
  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Name on Card"),
                                               ASCIIToUTF16("name_on_card"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                               ASCIIToUTF16("billing_address"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Card Number"),
                                               ASCIIToUTF16("card_number"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Expiration Date"),
                                               ASCIIToUTF16("expiration_month"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Expiration Year"),
                                               ASCIIToUTF16("expiration_year"),
                                               string16(),
                                               ASCIIToUTF16("text"),
                                               0,
                                               false));
  ScopedVector<FormStructure> forms;
  forms.push_back(new FormStructure(form));
  std::vector<std::string> encoded_signatures;
  std::string encoded_xml;
  const char * const kSignature1 = "11337937696949187602";
  const char * const kResponse1 =
      "<\?xml version=\"1.0\" encoding=\"UTF-8\"\?><autofillquery "
      "clientversion=\"6.1.1715.1442/en (GGLL)\"><form signature=\""
      "11337937696949187602\"><field signature=\"412125936\"/><field "
      "signature=\"1917667676\"/><field signature=\"2226358947\"/><field "
      "signature=\"747221617\"/><field signature=\"4108155786\"/></form>"
      "</autofillquery>";
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_signatures,
                                                &encoded_xml));
  ASSERT_EQ(encoded_signatures.size(), 1U);
  EXPECT_EQ(encoded_signatures[0], kSignature1);
  EXPECT_EQ(encoded_xml, kResponse1);

  // Add the same form, only one will be encoded, so EncodeQueryRequest() should
  // return the same data.
  forms.push_back(new FormStructure(form));
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_signatures,
                                                &encoded_xml));
  ASSERT_EQ(encoded_signatures.size(), 1U);
  EXPECT_EQ(encoded_signatures[0], kSignature1);
  EXPECT_EQ(encoded_xml, kResponse1);
  // Add 5 address fields - this should be still a valid form.
  for (size_t i = 0; i < 5; ++i) {
    form.fields.push_back(
        webkit_glue::FormField(ASCIIToUTF16("Address"),
                               ASCIIToUTF16("address"),
                               string16(),
                               ASCIIToUTF16("text"),
                               0,
                               false));
  }

  forms.push_back(new FormStructure(form));
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_signatures,
                                                &encoded_xml));
  ASSERT_EQ(encoded_signatures.size(), 2U);
  EXPECT_EQ(encoded_signatures[0], kSignature1);
  const char * const kSignature2 = "8308881815906226214";
  EXPECT_EQ(encoded_signatures[1], kSignature2);
  const char * const kResponse2 =
      "<\?xml version=\"1.0\" encoding=\"UTF-8\"\?><autofillquery "
      "clientversion=\"6.1.1715.1442/en (GGLL)\"><form signature=\""
      "11337937696949187602\"><field signature=\"412125936\"/><field signature="
      "\"1917667676\"/><field signature=\"2226358947\"/><field signature=\""
      "747221617\"/><field signature=\"4108155786\"/></form><form signature=\""
      "8308881815906226214\"><field signature=\"412125936\"/><field signature="
      "\"1917667676\"/><field signature=\"2226358947\"/><field signature=\""
      "747221617\"/><field signature=\"4108155786\"/><field signature=\""
      "509334676\"/><field signature=\"509334676\"/><field signature=\""
      "509334676\"/><field signature=\"509334676\"/><field signature=\""
      "509334676\"/></form></autofillquery>";
  EXPECT_EQ(encoded_xml, kResponse2);

  // Add 50 address fields - the form is not valid anymore, but previous ones
  // are. The result should be the same as in previous test.
  for (size_t i = 0; i < 50; ++i) {
    form.fields.push_back(
        webkit_glue::FormField(ASCIIToUTF16("Address"),
                               ASCIIToUTF16("address"),
                               string16(),
                               ASCIIToUTF16("text"),
                               0,
                               false));
  }

  forms.push_back(new FormStructure(form));
  ASSERT_TRUE(FormStructure::EncodeQueryRequest(forms, &encoded_signatures,
                                                &encoded_xml));
  ASSERT_EQ(encoded_signatures.size(), 2U);
  EXPECT_EQ(encoded_signatures[0], kSignature1);
  EXPECT_EQ(encoded_signatures[1], kSignature2);
  EXPECT_EQ(encoded_xml, kResponse2);

  // Check that we fail if there are only bad form(s).
  ScopedVector<FormStructure> bad_forms;
  bad_forms.push_back(new FormStructure(form));
  EXPECT_FALSE(FormStructure::EncodeQueryRequest(bad_forms, &encoded_signatures,
                                                 &encoded_xml));
  EXPECT_EQ(encoded_signatures.size(), 0U);
  EXPECT_EQ(encoded_xml, "");
}

TEST(FormStructureTest, EncodeUploadRequest) {
  scoped_ptr<FormStructure> form_structure;
  std::vector<FieldTypeSet> possible_field_types;
  FormData form;
  form.method = ASCIIToUTF16("post");
  form_structure.reset(new FormStructure(form));
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                        ASCIIToUTF16("firstname"),
                        string16(),
                        ASCIIToUTF16("text"),
                        0,
                        false));
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(NAME_FIRST);
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                        ASCIIToUTF16("lastname"),
                        string16(),
                        ASCIIToUTF16("text"),
                        0,
                        false));
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(NAME_LAST);
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("EMail"),
                        ASCIIToUTF16("email"),
                        string16(),
                        ASCIIToUTF16("email"),
                        0,
                        false));
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(EMAIL_ADDRESS);
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Phone"),
                        ASCIIToUTF16("phone"),
                        string16(),
                        ASCIIToUTF16("number"),
                        0,
                        false));
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(PHONE_HOME_WHOLE_NUMBER);
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Country"),
                        ASCIIToUTF16("country"),
                        string16(),
                        ASCIIToUTF16("select-one"),
                        0,
                        false));
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(ADDRESS_HOME_COUNTRY);
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Fax"),
                        ASCIIToUTF16("fax"),
                        string16(),
                        ASCIIToUTF16("tel"),
                        0,
                        false));
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(PHONE_FAX_WHOLE_NUMBER);
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Address"),
                        ASCIIToUTF16("address"),
                        string16(),
                        ASCIIToUTF16("radio"),
                        0,
                        false));
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(ADDRESS_HOME_LINE1);
  form_structure.reset(new FormStructure(form));
  std::string encoded_xml;
  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->set_possible_types(i, possible_field_types[i]);
  EXPECT_TRUE(form_structure->EncodeUploadRequest(false, &encoded_xml));
  EXPECT_EQ(encoded_xml,
      "<\?xml version=\"1.0\" encoding=\"UTF-8\"\?><autofillupload "
      "clientversion=\"6.1.1715.1442/en (GGLL)\" formsignature=\""
      "8269229441054798720\" autofillused=\"false\" datapresent=\"1442008208\">"
      "<field signature=\"3763331450\" autofilltype=\"3\"/><field signature=\""
      "3494530716\" autofilltype=\"5\"/><field signature=\"1029417091\" "
      "autofilltype=\"9\"/><field signature=\"466116101\" autofilltype="
      "\"14\"/><field signature=\"2799270304\" autofilltype=\"36\"/><field "
      "signature=\"1876771436\" autofilltype=\"24\"/><field signature="
      "\"263446779\" autofilltype=\"30\"/></autofillupload>");
  EXPECT_TRUE(form_structure->EncodeUploadRequest(true, &encoded_xml));
  EXPECT_EQ(encoded_xml,
      "<\?xml version=\"1.0\" encoding=\"UTF-8\"\?><autofillupload "
      "clientversion=\"6.1.1715.1442/en (GGLL)\" formsignature=\""
      "8269229441054798720\" autofillused=\"true\" datapresent=\"1442008208\">"
      "<field signature=\"3763331450\" autofilltype=\"3\"/><field signature=\""
      "3494530716\" autofilltype=\"5\"/><field signature=\"1029417091\" "
      "autofilltype=\"9\"/><field signature=\"466116101\" autofilltype="
      "\"14\"/><field signature=\"2799270304\" autofilltype=\"36\"/><field "
      "signature=\"1876771436\" autofilltype=\"24\"/><field signature="
      "\"263446779\" autofilltype=\"30\"/></autofillupload>");
  // Add 4 address fields - this should be still a valid form.
  for (size_t i = 0; i < 2; ++i) {
    form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                                 ASCIIToUTF16("address"),
                                                 string16(),
                                                 ASCIIToUTF16("text"),
                                                 0,
                                                 false));
    possible_field_types.push_back(FieldTypeSet());
    possible_field_types.back().insert(ADDRESS_HOME_LINE1);
    possible_field_types.back().insert(ADDRESS_HOME_LINE2);
    possible_field_types.back().insert(ADDRESS_BILLING_LINE1);
    possible_field_types.back().insert(ADDRESS_BILLING_LINE2);
  }
  form_structure.reset(new FormStructure(form));
  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->set_possible_types(i, possible_field_types[i]);
  EXPECT_TRUE(form_structure->EncodeUploadRequest(false, &encoded_xml));
  EXPECT_EQ(encoded_xml,
      "<\?xml version=\"1.0\" encoding=\"UTF-8\"\?><autofillupload "
      "clientversion=\"6.1.1715.1442/en (GGLL)\" formsignature=\""
      "9124126510289951497\" autofillused=\"false\" datapresent=\"144200830e\">"
      "<field signature=\"3763331450\" autofilltype=\"3\"/><field signature=\""
      "3494530716\" autofilltype=\"5\"/><field signature=\"1029417091\" "
      "autofilltype=\"9\"/><field signature=\"466116101\" autofilltype=\"14\"/>"
      "<field signature=\"2799270304\" autofilltype=\"36\"/><field signature=\""
      "1876771436\" autofilltype=\"24\"/><field signature=\"263446779\" "
      "autofilltype=\"30\"/>"
      "<field signature=\"509334676\" autofilltype=\"30\"/>"
      "<field signature=\"509334676\" autofilltype=\"31\"/>"
      "<field signature=\"509334676\" autofilltype=\"37\"/>"
      "<field signature=\"509334676\" autofilltype=\"38\"/>"
      "<field signature=\"509334676\" autofilltype=\"30\"/>"
      "<field signature=\"509334676\" autofilltype=\"31\"/>"
      "<field signature=\"509334676\" autofilltype=\"37\"/>"
      "<field signature=\"509334676\" autofilltype=\"38\"/>"
      "</autofillupload>");
  // Add 50 address fields - now the form is invalid.
  for (size_t i = 0; i < 50; ++i) {
    form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                                 ASCIIToUTF16("address"),
                                                 string16(),
                                                 ASCIIToUTF16("text"),
                                                 0,
                                                 false));
    possible_field_types.push_back(FieldTypeSet());
    possible_field_types.back().insert(ADDRESS_HOME_LINE1);
    possible_field_types.back().insert(ADDRESS_HOME_LINE2);
    possible_field_types.back().insert(ADDRESS_BILLING_LINE1);
    possible_field_types.back().insert(ADDRESS_BILLING_LINE2);
  }
  form_structure.reset(new FormStructure(form));
  ASSERT_EQ(form_structure->field_count(), possible_field_types.size());
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->set_possible_types(i, possible_field_types[i]);
  EXPECT_FALSE(form_structure->EncodeUploadRequest(false, &encoded_xml));
  EXPECT_EQ(encoded_xml, "");
}

TEST(FormStructureTest, CheckDataPresence) {
  // Checks bits set in the datapresence field: for each type in the form
  // relevant bit in datapresence has to be set.
  scoped_ptr<FormStructure> form_structure;
  std::vector<FieldTypeSet> possible_field_types;
  FormData form;
  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                        ASCIIToUTF16("first"),
                        string16(),
                        ASCIIToUTF16("text"),
                        0,
                        false));
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(NAME_FIRST);
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                        ASCIIToUTF16("last"),
                        string16(),
                        ASCIIToUTF16("text"),
                        0,
                        false));
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(NAME_LAST);
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("email"),
                        ASCIIToUTF16("email"),
                        string16(),
                        ASCIIToUTF16("text"),
                        0,
                        false));
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(EMAIL_ADDRESS);
  form_structure.reset(new FormStructure(form));
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->set_possible_types(i, possible_field_types[i]);
  std::string encoded_xml;
  EXPECT_TRUE(form_structure->EncodeUploadRequest(false, &encoded_xml));
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?><autofillupload "
            "clientversion=\"6.1.1715.1442/en (GGLL)\" formsignature=\""
            "6402244543831589061\" autofillused=\"false\" "
            "datapresent=\"1440\"><field signature=\"1089846351\" ",
            encoded_xml.substr(0, 200));

  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Address"),
                        ASCIIToUTF16("address"),
                        string16(),
                        ASCIIToUTF16("text"),
                        0,
                        false));
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(ADDRESS_HOME_LINE1);
  form_structure.reset(new FormStructure(form));
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->set_possible_types(i, possible_field_types[i]);
  EXPECT_TRUE(form_structure->EncodeUploadRequest(false, &encoded_xml));
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?><autofillupload "
            "clientversion=\"6.1.1715.1442/en (GGLL)\" formsignature=\""
            "11817937699000629499\" autofillused=\"false\" "
            "datapresent=\"14400002\"><field signature=\"1089846",
            encoded_xml.substr(0, 200));

  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("F4"),
                        ASCIIToUTF16("f4"),
                        string16(),
                        ASCIIToUTF16("text"),
                        0,
                        false));
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(CREDIT_CARD_TYPE);
  form_structure.reset(new FormStructure(form));
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->set_possible_types(i, possible_field_types[i]);
  EXPECT_TRUE(form_structure->EncodeUploadRequest(false, &encoded_xml));
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?><autofillupload "
            "clientversion=\"6.1.1715.1442/en (GGLL)\" formsignature=\""
            "15126663683491865216\" autofillused=\"false\" "
            "datapresent=\"1440000200000020\"><field signature=",
            encoded_xml.substr(0, 200));
}

TEST(FormStructureTest, CheckMultipleTypes) {
  // Check that multiple types for the field are processed correctly, both in
  // datapresence and in actual field data.
  scoped_ptr<FormStructure> form_structure;
  std::vector<FieldTypeSet> possible_field_types;
  FormData form;
  form.method = ASCIIToUTF16("post");
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("email"),
                        ASCIIToUTF16("email"),
                        string16(),
                        ASCIIToUTF16("text"),
                        0,
                        false));
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(EMAIL_ADDRESS);
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                        ASCIIToUTF16("first"),
                        string16(),
                        ASCIIToUTF16("text"),
                        0,
                        false));
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(NAME_FIRST);
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                        ASCIIToUTF16("last"),
                        string16(),
                        ASCIIToUTF16("text"),
                        0,
                        false));
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(NAME_LAST);
  form.fields.push_back(webkit_glue::FormField(ASCIIToUTF16("Address"),
                        ASCIIToUTF16("address"),
                        string16(),
                        ASCIIToUTF16("text"),
                        0,
                        false));
  possible_field_types.push_back(FieldTypeSet());
  possible_field_types.back().insert(ADDRESS_HOME_LINE1);
  form_structure.reset(new FormStructure(form));
  for (size_t i = 0; i < form_structure->field_count(); ++i)
    form_structure->set_possible_types(i, possible_field_types[i]);
  std::string encoded_xml;
  // Now we matched both fields singularly.
  EXPECT_TRUE(form_structure->EncodeUploadRequest(false, &encoded_xml));
  // datapresent==14400002==00010100010000000000000000000010b set bits are:
  // #3 == NAME_FIRST
  // #5 == NAME_LAST
  // #9 == EMAIL_ADDRESS
  // #30 == ADDRESS_HOME_LINE1
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?><autofillupload "
            "clientversion=\"6.1.1715.1442/en (GGLL)\" formsignature=\""
            "18062476096658145866\" autofillused=\"false\" datapresent="
            "\"14400002\"><field signature=\"420638584\" autofilltype="
            "\"9\"/><field signature=\"1089846351\" autofilltype=\"3\"/><field "
            "signature=\"2404144663\" autofilltype=\"5\"/><field signature="
            "\"509334676\" autofilltype=\"30\"/></autofillupload>",
            encoded_xml);
  // Match third field as both first and last.
  possible_field_types[2].insert(NAME_FIRST);
  form_structure->set_possible_types(2, possible_field_types[2]);
  EXPECT_TRUE(form_structure->EncodeUploadRequest(false, &encoded_xml));
  // datapresent==14400002==00010100010000000000000000000010b set bits are:
  // #3 == NAME_FIRST
  // #5 == NAME_LAST
  // #9 == EMAIL_ADDRESS
  // #30 == ADDRESS_HOME_LINE1
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?><autofillupload "
            "clientversion=\"6.1.1715.1442/en (GGLL)\" formsignature=\""
            "18062476096658145866\" autofillused=\"false\" datapresent="
            "\"14400002\"><field signature=\"420638584\" autofilltype="
            "\"9\"/><field signature=\"1089846351\" autofilltype=\"3\"/><field "
            "signature=\"2404144663\" autofilltype=\"3\"/><field "
            "signature=\"2404144663\" autofilltype=\"5\"/><field signature="
            "\"509334676\" autofilltype=\"30\"/></autofillupload>",
            encoded_xml);
  possible_field_types[3].insert(ADDRESS_BILLING_LINE1);
  form_structure->set_possible_types(
      form_structure->field_count() - 1,
      possible_field_types[form_structure->field_count() - 1]);
  EXPECT_TRUE(form_structure->EncodeUploadRequest(false, &encoded_xml));
  // datapresent==1440000204==0001010001000000000000000000001000000100b set bits
  // are:
  // #3 == NAME_FIRST
  // #5 == NAME_LAST
  // #9 == EMAIL_ADDRESS
  // #30 == ADDRESS_HOME_LINE1
  // #37 == ADDRESS_BILLING_LINE1
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?><autofillupload "
            "clientversion=\"6.1.1715.1442/en (GGLL)\" formsignature=\""
            "18062476096658145866\" autofillused=\"false\" datapresent="
            "\"1440000204\"><field signature=\"420638584\" autofilltype="
            "\"9\"/><field signature=\"1089846351\" autofilltype=\"3\"/><field "
            "signature=\"2404144663\" autofilltype=\"3\"/><field "
            "signature=\"2404144663\" autofilltype=\"5\"/><field signature="
            "\"509334676\" autofilltype=\"30\"/><field signature=\"509334676\" "
            "autofilltype=\"37\"/></autofillupload>",
            encoded_xml);
  possible_field_types[3].clear();
  possible_field_types[3].insert(ADDRESS_HOME_LINE1);
  possible_field_types[3].insert(ADDRESS_BILLING_LINE2);
  form_structure->set_possible_types(
      form_structure->field_count() - 1,
      possible_field_types[form_structure->field_count() - 1]);
  EXPECT_TRUE(form_structure->EncodeUploadRequest(false, &encoded_xml));
  // datapresent==1440000202==0001010001000000000000000000001000000010b set bits
  // are:
  // #3 == NAME_FIRST
  // #5 == NAME_LAST
  // #9 == EMAIL_ADDRESS
  // #30 == ADDRESS_HOME_LINE1
  // #38 == ADDRESS_BILLING_LINE2
  EXPECT_EQ("<\?xml version=\"1.0\" encoding=\"UTF-8\"\?><autofillupload "
            "clientversion=\"6.1.1715.1442/en (GGLL)\" formsignature=\""
            "18062476096658145866\" autofillused=\"false\" datapresent="
            "\"1440000202\"><field signature=\"420638584\" autofilltype="
            "\"9\"/><field signature=\"1089846351\" autofilltype=\"3\"/><field "
            "signature=\"2404144663\" autofilltype=\"3\"/><field "
            "signature=\"2404144663\" autofilltype=\"5\"/><field signature="
            "\"509334676\" autofilltype=\"30\"/><field signature=\"509334676\" "
            "autofilltype=\"38\"/></autofillupload>",
            encoded_xml);
}

}  // namespace
