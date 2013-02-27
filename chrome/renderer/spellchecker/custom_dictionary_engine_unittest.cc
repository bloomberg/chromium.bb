// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/renderer/spellchecker/custom_dictionary_engine.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(CustomDictionaryTest, HandlesEmptyWordWithInvalidSubstring) {
  CustomDictionaryEngine engine;
  std::vector<std::string> custom_words;

  engine.Init(custom_words);
  EXPECT_FALSE(engine.SpellCheckWord(string16().c_str(), 15, 23));
}

