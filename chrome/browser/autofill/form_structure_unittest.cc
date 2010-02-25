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
