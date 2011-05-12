// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/format_macros.h"
#include "base/stringprintf.h"
#include "base/string_split.h"
#include "base/utf_string_conversions.h"
#include "chrome/renderer/spellchecker/spellcheck_worditerator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct TestCase {
    const char* language;
    bool allow_contraction;
    const wchar_t* expected_words;
};

}  // namespace

// Tests whether or not our SpellcheckWordIterator can extract only words used
// by the specified language from a multi-language text.
TEST(SpellcheckWordIteratorTest, SplitWord) {
  // An input text. This text includes words of several languages. (Some words
  // are not separated with whitespace characters.) Our SpellcheckWordIterator
  // should extract only the words used by the specified language from this text
  // and normalize them so our spell-checker can check their spellings.
  const wchar_t kTestText[] =
      // Numbers
      L"0123456789"
      // Latin (including a contraction character and a ligature).
      L"hello:hello a\xFB03x"
      // Greek
      L"\x03B3\x03B5\x03B9\x03AC\x0020\x03C3\x03BF\x03C5"
      // Cyrillic
      L"\x0437\x0434\x0440\x0430\x0432\x0441\x0442\x0432"
      L"\x0443\x0439\x0442\x0435"
      // Hebrew (including niqquds)
      L"\x05e9\x05c1\x05b8\x05dc\x05d5\x05b9\x05dd "
      // Hebrew words with U+0027 and U+05F3
      L"\x05e6\x0027\x05d9\x05e4\x05e1 \x05e6\x05F3\x05d9\x05e4\x05e1 "
      // Hebrew words with U+0022 and U+05F4
      L"\x05e6\x05d4\x0022\x05dc \x05e6\x05d4\x05f4\x05dc "
      // Hebrew words enclosed with ASCII quotes.
      L"\"\x05e6\x05d4\x0022\x05dc\" '\x05e9\x05c1\x05b8\x05dc\x05d5'"
      // Arabic (including vowel marks)
      L"\x0627\x064e\x0644\x0633\x064e\x0651\x0644\x0627"
      L"\x0645\x064f\x0020\x0639\x064e\x0644\x064e\x064a"
      L"\x0652\x0643\x064f\x0645\x0652"
      // Hindi
      L"\x0930\x093E\x091C\x0927\x093E\x0928"
      // Thai
      L"\x0e2a\x0e27\x0e31\x0e2a\x0e14\x0e35\x0020\x0e04"
      L"\x0e23\x0e31\x0e1a"
      // Hiraganas
      L"\x3053\x3093\x306B\x3061\x306F"
      // CJKV ideographs
      L"\x4F60\x597D"
      // Hangul Syllables
      L"\xC548\xB155\xD558\xC138\xC694"
      // Full-width latin : Hello
      L"\xFF28\xFF45\xFF4C\xFF4C\xFF4F "
      L"e.g.,";

  // The languages and expected results used in this test.
  static const TestCase kTestCases[] = {
    {
      // English (keep contraction words)
      "en-US", true, L"hello:hello affix Hello e.g"
    }, {
      // English (split contraction words)
      "en-US", false, L"hello hello affix Hello e g"
    }, {
      // Greek
      "el-GR", true,
      L"\x03B3\x03B5\x03B9\x03AC\x0020\x03C3\x03BF\x03C5"
    }, {
      // Russian
      "ru-RU", true,
      L"\x0437\x0434\x0440\x0430\x0432\x0441\x0442\x0432"
      L"\x0443\x0439\x0442\x0435"
    }, {
      // Hebrew
      "he-IL", true,
      L"\x05e9\x05dc\x05d5\x05dd "
      L"\x05e6\x0027\x05d9\x05e4\x05e1 \x05e6\x05F3\x05d9\x05e4\x05e1 "
      L"\x05e6\x05d4\x0022\x05dc \x05e6\x05d4\x05f4\x05dc "
      L"\x05e6\x05d4\x0022\x05dc \x05e9\x05dc\x05d5"
    }, {
      // Arabic
      "ar", true,
      L"\x0627\x0644\x0633\x0644\x0627\x0645\x0020\x0639"
      L"\x0644\x064a\x0643\x0645"
    }, {
      // Hindi
      "hi-IN", true,
      L"\x0930\x093E\x091C\x0927\x093E\x0928"
    }, {
      // Thai
      "th-TH", true,
      L"\x0e2a\x0e27\x0e31\x0e2a\x0e14\x0e35\x0020\x0e04"
      L"\x0e23\x0e31\x0e1a"
    }, {
      // Korean
      "ko-KR", true,
      L"\x110b\x1161\x11ab\x1102\x1167\x11bc\x1112\x1161"
      L"\x1109\x1166\x110b\x116d"
    },
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    SCOPED_TRACE(base::StringPrintf("kTestCases[%" PRIuS "]: language=%s", i,
                                    kTestCases[i].language));

    SpellcheckCharAttribute attributes;
    attributes.SetDefaultLanguage(kTestCases[i].language);

    string16 input(WideToUTF16(kTestText));
    SpellcheckWordIterator iterator;
    EXPECT_TRUE(iterator.Initialize(&attributes, input.c_str(), input.length(),
                                    kTestCases[i].allow_contraction));

    std::vector<string16> expected_words;
    base::SplitString(
        WideToUTF16(kTestCases[i].expected_words), ' ', &expected_words);

    string16 actual_word;
    int actual_start, actual_end;
    size_t index = 0;
    while (iterator.GetNextWord(&actual_word, &actual_start, &actual_end)) {
      EXPECT_TRUE(index < expected_words.size());
      if (index < expected_words.size())
        EXPECT_EQ(expected_words[index], actual_word);
      ++index;
    }
  }
}

// Tests whether our SpellcheckWordIterator extracts an empty word without
// getting stuck in an infinite loop when inputting a Khmer text. (This is a
// regression test for Issue 46278.)
TEST(SpellcheckWordIteratorTest, RuleSetConsistency) {
  SpellcheckCharAttribute attributes;
  attributes.SetDefaultLanguage("en-US");

  const wchar_t kTestText[] = L"\x1791\x17c1\x002e";
  string16 input(WideToUTF16(kTestText));

  SpellcheckWordIterator iterator;
  EXPECT_TRUE(iterator.Initialize(&attributes, input.c_str(), input.length(),
                                  true));

  // When SpellcheckWordIterator uses an inconsistent ICU ruleset, the following
  // iterator.GetNextWord() call gets stuck in an infinite loop. Therefore, this
  // test succeeds if this call returns without timeouts.
  string16 actual_word;
  int actual_start, actual_end;
  EXPECT_FALSE(iterator.GetNextWord(&actual_word, &actual_start, &actual_end));
  EXPECT_EQ(0, actual_start);
  EXPECT_EQ(0, actual_end);
}
