// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/contact_info.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

class ContactInfoTest : public testing::Test {
 public:
  ContactInfoTest() {}

  string16 first(const ContactInfo& contact) {
    return contact.first();
  }
  string16 middle(const ContactInfo& contact) {
    return contact.middle();
  }
  string16 last(const ContactInfo& contact) {
    return contact.last();
  }
  string16 FullName(const ContactInfo& contact) {
    return contact.FullName();
  }
  void SetFullName(ContactInfo* contact, const string16& full_name) {
    contact->SetFullName(full_name);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContactInfoTest);
};

TEST_F(ContactInfoTest, TestSetFullName) {
  ContactInfo contact_info;
  SetFullName(&contact_info, ASCIIToUTF16("Virgil"));
  EXPECT_EQ(first(contact_info), ASCIIToUTF16("Virgil"));
  EXPECT_EQ(middle(contact_info), ASCIIToUTF16(""));
  EXPECT_EQ(last(contact_info), ASCIIToUTF16(""));
  EXPECT_EQ(FullName(contact_info), ASCIIToUTF16("Virgil"));

  SetFullName(&contact_info, ASCIIToUTF16("Murray Gell-Mann"));
  EXPECT_EQ(first(contact_info), ASCIIToUTF16("Murray"));
  EXPECT_EQ(middle(contact_info), ASCIIToUTF16(""));
  EXPECT_EQ(last(contact_info), ASCIIToUTF16("Gell-Mann"));
  EXPECT_EQ(FullName(contact_info), ASCIIToUTF16("Murray Gell-Mann"));

  SetFullName(&contact_info,
              ASCIIToUTF16("Mikhail Yevgrafovich Saltykov-Shchedrin"));
  EXPECT_EQ(first(contact_info), ASCIIToUTF16("Mikhail"));
  EXPECT_EQ(middle(contact_info), ASCIIToUTF16("Yevgrafovich"));
  EXPECT_EQ(last(contact_info), ASCIIToUTF16("Saltykov-Shchedrin"));
  EXPECT_EQ(FullName(contact_info),
            ASCIIToUTF16("Mikhail Yevgrafovich Saltykov-Shchedrin"));

  SetFullName(&contact_info, ASCIIToUTF16("Arthur Ignatius Conan Doyle"));
  EXPECT_EQ(first(contact_info), ASCIIToUTF16("Arthur"));
  EXPECT_EQ(middle(contact_info), ASCIIToUTF16("Ignatius Conan"));
  EXPECT_EQ(last(contact_info), ASCIIToUTF16("Doyle"));
  EXPECT_EQ(FullName(contact_info),
            ASCIIToUTF16("Arthur Ignatius Conan Doyle"));
}

