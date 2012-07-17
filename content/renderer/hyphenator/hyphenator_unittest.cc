// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/hyphenator/hyphenator.h"

#include "base/path_service.h"
#include "base/platform_file.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/hyphen/hyphen.h"

// A unit test for our hyphenator. This class loads a sample hyphenation
// dictionary and hyphenates words.
class HyphenatorTest : public testing::Test {
 public:
  HyphenatorTest() {
    Initialize();
  }

  bool Initialize() {
    FilePath dictionary_path;
    if (!PathService::Get(base::DIR_SOURCE_ROOT, &dictionary_path))
      return false;
    dictionary_path = dictionary_path.AppendASCII("third_party");
    dictionary_path = dictionary_path.AppendASCII("hyphen");
    dictionary_path = dictionary_path.AppendASCII("hyph_en_US.dic");
    base::PlatformFile file = base::CreatePlatformFile(
        dictionary_path, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
        NULL, NULL);
    hyphenator_.reset(new content::Hyphenator(file));
    return hyphenator_->Initialize();
  }

  // Creates a human-readable hyphenated word. This function inserts '-'
  // characters to all places where we can insert hyphens to improve the
  // readability of this unit test.
  string16 Hyphenate(const string16& word) {
    string16 hyphenated_word(word);
    size_t position = word.length();
    while (position > 0) {
      size_t new_position = hyphenator_->ComputeLastHyphenLocation(word,
                                                                   position);
      EXPECT_LT(new_position, position);
      if (new_position > 0)
        hyphenated_word.insert(new_position, 1, '-');
      position = new_position;
    }
    return hyphenated_word;
  }

 private:
  scoped_ptr<content::Hyphenator> hyphenator_;
};

// Verifies that our hyphenator yields the same hyphenated words as the original
// hyphen library does.
TEST_F(HyphenatorTest, HyphenateWords) {
  static const struct {
    const char* input;
    const char* expected;
  } kTestCases[] = {
    { "and", "and" },
    { "concupiscent,", "con-cu-pis-cent," },
    { "evidence.", "ev-i-dence." },
    { "first", "first" },
    { "getting", "get-ting" },
    { "hedgehog", "hedge-hog" },
    { "remarkable", "re-mark-able" },
    { "straightened", "straight-ened" },
    { "undid", "un-did" },
    { "were", "were" },
    { "Simply", "Sim-ply" },
    { "Undone.", "Un-done." },
    { "comfortably", "com-fort-ably"},
    { "declination", "dec-li-na-tion" },
    { "flamingo:", "flamin-go:" },
    { "lination", "lina-tion" },
    { "reciprocity", "rec-i-proc-i-ty" },
    { "throughout", "through-out" },
    { "undid", "un-did" },
    { "undone.", "un-done." },
    { "unnecessary", "un-nec-es-sary" },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    string16 input = ASCIIToUTF16(kTestCases[i].input);
    string16 expected = ASCIIToUTF16(kTestCases[i].expected);
    EXPECT_EQ(expected, Hyphenate(input));
  }
}
