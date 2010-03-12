// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/form_structure.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebInputElement.h"
#include "webkit/glue/form_field_values.h"

using WebKit::WebInputElement;

namespace {

TEST(FormStructureTest, FieldCount) {
  webkit_glue::FormFieldValues values;
  values.method = ASCIIToUTF16("post");
  values.elements.push_back(webkit_glue::FormField(ASCIIToUTF16("username"),
                                                   ASCIIToUTF16("username"),
                                                   string16(),
                                                   ASCIIToUTF16("text"),
                                                   WebInputElement::Text));
  values.elements.push_back(webkit_glue::FormField(ASCIIToUTF16("password"),
                                                   ASCIIToUTF16("password"),
                                                   string16(),
                                                   ASCIIToUTF16("password"),
                                                   WebInputElement::Password));
  values.elements.push_back(webkit_glue::FormField(string16(),
                                                   ASCIIToUTF16("Submit"),
                                                   string16(),
                                                   ASCIIToUTF16("submit"),
                                                   WebInputElement::Submit));

  FormStructure form_structure(values);

  // Only text fields are counted.
  EXPECT_EQ(1U, form_structure.field_count());
}

TEST(FormStructureTest, IsAutoFillable) {
  scoped_ptr<FormStructure> form_structure;
  webkit_glue::FormFieldValues values;

  // We need at least three text fields to be auto-fillable.
  values.method = ASCIIToUTF16("post");
  values.elements.push_back(webkit_glue::FormField(ASCIIToUTF16("username"),
                                                   ASCIIToUTF16("username"),
                                                   string16(),
                                                   ASCIIToUTF16("text"),
                                                   WebInputElement::Text));
  values.elements.push_back(webkit_glue::FormField(ASCIIToUTF16("password"),
                                                   ASCIIToUTF16("password"),
                                                   string16(),
                                                   ASCIIToUTF16("password"),
                                                   WebInputElement::Password));
  values.elements.push_back(webkit_glue::FormField(string16(),
                                                   ASCIIToUTF16("Submit"),
                                                   string16(),
                                                   ASCIIToUTF16("submit"),
                                                   WebInputElement::Submit));
  form_structure.reset(new FormStructure(values));
  EXPECT_FALSE(form_structure->IsAutoFillable());

  // We now have three text fields.
  values.elements.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                                   ASCIIToUTF16("firstname"),
                                                   string16(),
                                                   ASCIIToUTF16("text"),
                                                   WebInputElement::Text));
  values.elements.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                                   ASCIIToUTF16("lastname"),
                                                   string16(),
                                                   ASCIIToUTF16("text"),
                                                   WebInputElement::Text));
  form_structure.reset(new FormStructure(values));
  EXPECT_TRUE(form_structure->IsAutoFillable());

  // The method must be 'post'.
  values.method = ASCIIToUTF16("get");
  form_structure.reset(new FormStructure(values));
  EXPECT_FALSE(form_structure->IsAutoFillable());

  // The target cannot include http(s)://*/search...
  values.method = ASCIIToUTF16("post");
  values.target_url = GURL("http://google.com/search?q=hello");
  form_structure.reset(new FormStructure(values));
  EXPECT_FALSE(form_structure->IsAutoFillable());

  // But search can be in the URL.
  values.target_url = GURL("http://search.com/?q=hello");
  form_structure.reset(new FormStructure(values));
  EXPECT_TRUE(form_structure->IsAutoFillable());
}

TEST(FormStructureTest, Heuristics) {
  scoped_ptr<FormStructure> form_structure;
  webkit_glue::FormFieldValues values;

  values.method = ASCIIToUTF16("post");
  values.elements.push_back(webkit_glue::FormField(ASCIIToUTF16("First Name"),
                                                   ASCIIToUTF16("firstname"),
                                                   string16(),
                                                   ASCIIToUTF16("text"),
                                                   WebInputElement::Text));
  values.elements.push_back(webkit_glue::FormField(ASCIIToUTF16("Last Name"),
                                                   ASCIIToUTF16("lastname"),
                                                   string16(),
                                                   ASCIIToUTF16("text"),
                                                   WebInputElement::Text));
  values.elements.push_back(webkit_glue::FormField(ASCIIToUTF16("EMail"),
                                                   ASCIIToUTF16("email"),
                                                   string16(),
                                                   ASCIIToUTF16("text"),
                                                   WebInputElement::Text));
  values.elements.push_back(webkit_glue::FormField(ASCIIToUTF16("Phone"),
                                                   ASCIIToUTF16("phone"),
                                                   string16(),
                                                   ASCIIToUTF16("text"),
                                                   WebInputElement::Text));
  values.elements.push_back(webkit_glue::FormField(ASCIIToUTF16("Fax"),
                                                   ASCIIToUTF16("fax"),
                                                   string16(),
                                                   ASCIIToUTF16("text"),
                                                   WebInputElement::Text));
  values.elements.push_back(webkit_glue::FormField(ASCIIToUTF16("Address"),
                                                   ASCIIToUTF16("address"),
                                                   string16(),
                                                   ASCIIToUTF16("text"),
                                                   WebInputElement::Text));
  values.elements.push_back(webkit_glue::FormField(ASCIIToUTF16("City"),
                                                   ASCIIToUTF16("city"),
                                                   string16(),
                                                   ASCIIToUTF16("text"),
                                                   WebInputElement::Text));
  values.elements.push_back(webkit_glue::FormField(ASCIIToUTF16("Zip code"),
                                                   ASCIIToUTF16("zipcode"),
                                                   string16(),
                                                   ASCIIToUTF16("text"),
                                                   WebInputElement::Text));
  values.elements.push_back(webkit_glue::FormField(string16(),
                                                   ASCIIToUTF16("Submit"),
                                                   string16(),
                                                   ASCIIToUTF16("submit"),
                                                   WebInputElement::Submit));
  form_structure.reset(new FormStructure(values));
  EXPECT_TRUE(form_structure->IsAutoFillable());

  // Expect the correct number of fields.
  ASSERT_EQ(8UL, form_structure->field_count());

  // Check that heuristics are initialized as UNKNOWN_TYPE.
  std::vector<AutoFillField*>::const_iterator iter;
  size_t i;
  for (iter = form_structure->begin(), i = 0;
       iter != form_structure->end();
       ++iter, ++i) {
    // Expect last element to be NULL.
    if (i == form_structure->field_count()) {
      ASSERT_EQ(static_cast<AutoFillField*>(NULL), *iter);
    } else {
      ASSERT_NE(static_cast<AutoFillField*>(NULL), *iter);
      EXPECT_EQ(UNKNOWN_TYPE, (*iter)->heuristic_type());
    }
  }

  // Compute heuristic types.
  form_structure->GetHeuristicAutoFillTypes();

  // Check that heuristics are no longer UNKNOWN_TYPE.
  // First name.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->heuristic_type());
  // Last name.
  EXPECT_EQ(NAME_LAST, form_structure->field(1)->heuristic_type());
  // Email.
  EXPECT_EQ(EMAIL_ADDRESS, form_structure->field(2)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
      form_structure->field(3)->heuristic_type());
  // Fax.  Note, we don't currently match fax.
  EXPECT_EQ(UNKNOWN_TYPE, form_structure->field(4)->heuristic_type());
  // Address.
  EXPECT_EQ(ADDRESS_HOME_LINE1, form_structure->field(5)->heuristic_type());
  // City.
  EXPECT_EQ(ADDRESS_HOME_CITY, form_structure->field(6)->heuristic_type());
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_ZIP, form_structure->field(7)->heuristic_type());
}

TEST(FormStructureTest, HeuristicsSample8) {
  scoped_ptr<FormStructure> form_structure;
  webkit_glue::FormFieldValues values;

  values.method = ASCIIToUTF16("post");
  values.elements.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Your First Name:"),
                             ASCIIToUTF16("bill.first"),
                             string16(),
                             ASCIIToUTF16("text"),
                             WebInputElement::Text));
  values.elements.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Your Last Name:"),
                             ASCIIToUTF16("bill.last"),
                             string16(),
                             ASCIIToUTF16("text"),
                             WebInputElement::Text));
  values.elements.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Street Address Line 1:"),
                             ASCIIToUTF16("bill.street1"),
                             string16(),
                             ASCIIToUTF16("text"),
                             WebInputElement::Text));
  values.elements.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Street Address Line 2:"),
                             ASCIIToUTF16("bill.street2"),
                             string16(),
                             ASCIIToUTF16("text"),
                             WebInputElement::Text));
  values.elements.push_back(
      webkit_glue::FormField(ASCIIToUTF16("City:"),
                             ASCIIToUTF16("bill.city"),
                             string16(),
                             ASCIIToUTF16("text"),
                             WebInputElement::Text));
  values.elements.push_back(
      webkit_glue::FormField(ASCIIToUTF16("State (U.S.):"),
                             ASCIIToUTF16("bill.state"),
                             string16(),
                             ASCIIToUTF16("text"),
                             WebInputElement::Text));
  values.elements.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Zip/Postal Code:"),
                             ASCIIToUTF16("BillTo.PostalCode"),
                             string16(),
                             ASCIIToUTF16("text"),
                             WebInputElement::Text));
  values.elements.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Country:"),
                             ASCIIToUTF16("bill.country"),
                             string16(),
                             ASCIIToUTF16("text"),
                             WebInputElement::Text));
  values.elements.push_back(
      webkit_glue::FormField(ASCIIToUTF16("Phone Number:"),
                             ASCIIToUTF16("BillTo.Phone"),
                             string16(),
                             ASCIIToUTF16("text"),
                             WebInputElement::Text));
  values.elements.push_back(
      webkit_glue::FormField(string16(),
                             ASCIIToUTF16("Submit"),
                             string16(),
                             ASCIIToUTF16("submit"),
                             WebInputElement::Submit));
  form_structure.reset(new FormStructure(values));
  EXPECT_TRUE(form_structure->IsAutoFillable());

  // Check that heuristics are initialized as UNKNOWN_TYPE.
  std::vector<AutoFillField*>::const_iterator iter;
  size_t i;
  for (iter = form_structure->begin(), i = 0;
       iter != form_structure->end();
       ++iter, ++i) {
    // Expect last element to be NULL.
    if (i == form_structure->field_count()) {
      ASSERT_EQ(static_cast<AutoFillField*>(NULL), *iter);
    } else {
      ASSERT_NE(static_cast<AutoFillField*>(NULL), *iter);
      EXPECT_EQ(UNKNOWN_TYPE, (*iter)->heuristic_type());
    }
  }

  // Compute heuristic types.
  form_structure->GetHeuristicAutoFillTypes();

  // Check that heuristics are no longer UNKNOWN_TYPE.
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
  // Zip.
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, form_structure->field(7)->heuristic_type());
  // Phone.
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
      form_structure->field(8)->heuristic_type());
}

}  // namespace
