// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/contact_info.h"

#include "base/basictypes.h"
#include "base/format_macros.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

TEST(NameInfoTest, SetFullName) {
  NameInfo name;
  name.SetRawInfo(NAME_FULL, ASCIIToUTF16("Virgil"));
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), ASCIIToUTF16("Virgil"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), base::string16());
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), base::string16());
  EXPECT_EQ(name.GetRawInfo(NAME_FULL), ASCIIToUTF16("Virgil"));

  name.SetRawInfo(NAME_FULL, ASCIIToUTF16("Murray Gell-Mann"));
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), ASCIIToUTF16("Murray"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), base::string16());
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), ASCIIToUTF16("Gell-Mann"));
  EXPECT_EQ(name.GetRawInfo(NAME_FULL), ASCIIToUTF16("Murray Gell-Mann"));

  name.SetRawInfo(NAME_FULL,
               ASCIIToUTF16("Mikhail Yevgrafovich Saltykov-Shchedrin"));
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), ASCIIToUTF16("Mikhail"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), ASCIIToUTF16("Yevgrafovich"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), ASCIIToUTF16("Saltykov-Shchedrin"));
  EXPECT_EQ(name.GetRawInfo(NAME_FULL),
            ASCIIToUTF16("Mikhail Yevgrafovich Saltykov-Shchedrin"));

  name.SetRawInfo(NAME_FULL, ASCIIToUTF16("Arthur Ignatius Conan Doyle"));
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), ASCIIToUTF16("Arthur"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), ASCIIToUTF16("Ignatius Conan"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), ASCIIToUTF16("Doyle"));
  EXPECT_EQ(name.GetRawInfo(NAME_FULL),
            ASCIIToUTF16("Arthur Ignatius Conan Doyle"));
}

TEST(NameInfoTest, GetFullName) {
  NameInfo name;
  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetRawInfo(NAME_MIDDLE, base::string16());
  name.SetRawInfo(NAME_LAST, base::string16());
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), ASCIIToUTF16("First"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), base::string16());
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), base::string16());
  EXPECT_EQ(name.GetRawInfo(NAME_FULL), ASCIIToUTF16("First"));

  name.SetRawInfo(NAME_FIRST, base::string16());
  name.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetRawInfo(NAME_LAST, base::string16());
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), base::string16());
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), ASCIIToUTF16("Middle"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), base::string16());
  EXPECT_EQ(name.GetRawInfo(NAME_FULL), ASCIIToUTF16("Middle"));

  name.SetRawInfo(NAME_FIRST, base::string16());
  name.SetRawInfo(NAME_MIDDLE, base::string16());
  name.SetRawInfo(NAME_LAST, ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), base::string16());
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), base::string16());
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetRawInfo(NAME_FULL), ASCIIToUTF16("Last"));

  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetRawInfo(NAME_LAST, base::string16());
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), ASCIIToUTF16("First"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), ASCIIToUTF16("Middle"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), base::string16());
  EXPECT_EQ(name.GetRawInfo(NAME_FULL), ASCIIToUTF16("First Middle"));

  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetRawInfo(NAME_MIDDLE, base::string16());
  name.SetRawInfo(NAME_LAST, ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), ASCIIToUTF16("First"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), base::string16());
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetRawInfo(NAME_FULL), ASCIIToUTF16("First Last"));

  name.SetRawInfo(NAME_FIRST, base::string16());
  name.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetRawInfo(NAME_LAST, ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), base::string16());
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), ASCIIToUTF16("Middle"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetRawInfo(NAME_FULL), ASCIIToUTF16("Middle Last"));

  name.SetRawInfo(NAME_FIRST, ASCIIToUTF16("First"));
  name.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("Middle"));
  name.SetRawInfo(NAME_LAST, ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetRawInfo(NAME_FIRST), ASCIIToUTF16("First"));
  EXPECT_EQ(name.GetRawInfo(NAME_MIDDLE), ASCIIToUTF16("Middle"));
  EXPECT_EQ(name.GetRawInfo(NAME_LAST), ASCIIToUTF16("Last"));
  EXPECT_EQ(name.GetRawInfo(NAME_FULL), ASCIIToUTF16("First Middle Last"));
}

TEST(NameInfoTest, EqualsIgnoreCase) {
  struct TestCase {
    std::string starting_names[3];
    std::string additional_names[3];
    bool expected_result;
  };

  struct TestCase test_cases[] = {
      // Identical name comparison.
      {{"Marion", "Mitchell", "Morrison"},
       {"Marion", "Mitchell", "Morrison"},
       true},

      // Case-insensative comparisons.
      {{"Marion", "Mitchell", "Morrison"},
       {"Marion", "Mitchell", "MORRISON"},
       true},
      {{"Marion", "Mitchell", "Morrison"},
       {"MARION", "Mitchell", "MORRISON"},
       true},
      {{"Marion", "Mitchell", "Morrison"},
       {"MARION", "MITCHELL", "MORRISON"},
       true},
      {{"Marion", "", "Mitchell Morrison"},
       {"MARION", "", "MITCHELL MORRISON"},
       true},
      {{"Marion Mitchell", "", "Morrison"},
       {"MARION MITCHELL", "", "MORRISON"},
       true},

      // Identical full names but different canonical forms.
      {{"Marion", "Mitchell", "Morrison"},
       {"Marion", "", "Mitchell Morrison"},
       false},
      {{"Marion", "Mitchell", "Morrison"},
       {"Marion Mitchell", "", "MORRISON"},
       false},

      // Different names.
      {{"Marion", "Mitchell", "Morrison"}, {"Marion", "M.", "Morrison"}, false},
      {{"Marion", "Mitchell", "Morrison"}, {"MARION", "M.", "MORRISON"}, false},
      {{"Marion", "Mitchell", "Morrison"},
       {"David", "Mitchell", "Morrison"},
       false},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    SCOPED_TRACE(base::StringPrintf("i: %" PRIuS, i));

    // Construct the starting_profile.
    NameInfo starting_profile;
    starting_profile.SetRawInfo(NAME_FIRST,
                                ASCIIToUTF16(test_cases[i].starting_names[0]));
    starting_profile.SetRawInfo(NAME_MIDDLE,
                                ASCIIToUTF16(test_cases[i].starting_names[1]));
    starting_profile.SetRawInfo(NAME_LAST,
                                ASCIIToUTF16(test_cases[i].starting_names[2]));

    // Construct the additional_profile.
    NameInfo additional_profile;
    additional_profile.SetRawInfo(
        NAME_FIRST, ASCIIToUTF16(test_cases[i].additional_names[0]));
    additional_profile.SetRawInfo(
        NAME_MIDDLE, ASCIIToUTF16(test_cases[i].additional_names[1]));
    additional_profile.SetRawInfo(
        NAME_LAST, ASCIIToUTF16(test_cases[i].additional_names[2]));

    // Verify the test expectations.
    EXPECT_EQ(test_cases[i].expected_result,
              starting_profile.EqualsIgnoreCase(additional_profile));
  }
}

}  // namespace autofill
