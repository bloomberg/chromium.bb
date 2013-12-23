// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/language_options_handler.h"

#include <string>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(OS_MACOSX)
TEST(LanguageOptionsHandlerTest, GetUILanguageCodeSet) {
  scoped_ptr<base::DictionaryValue> dictionary(
      options::LanguageOptionsHandler::GetUILanguageCodeSet());
  EXPECT_TRUE(dictionary->HasKey("en-US"));
  // Note that we don't test a false case, as such an expectation will
  // fail when we add support for the language.
  // EXPECT_FALSE(dictionary->HasKey("no"));
}
#endif  // !defined(OS_MACOSX)

TEST(LanguageOptionsHandlerTest, GetSpellCheckLanguageCodeSet) {
  scoped_ptr<base::DictionaryValue> dictionary(
      options::LanguageOptionsHandler::GetSpellCheckLanguageCodeSet());
  EXPECT_TRUE(dictionary->HasKey("en-US"));
}
