// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/games/core/games_prefs.h"

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace games {
namespace prefs {

class GamesPrefsTest : public testing::Test {
 protected:
  void SetUp() override {
    test_pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    // Register Games prefs.
    prefs::RegisterProfilePrefs(test_pref_service_->registry());
  }

  std::unique_ptr<TestingPrefServiceSimple> test_pref_service_;
};

TEST_F(GamesPrefsTest, GetGamesCatalogPath_Empty) {
  base::FilePath path;
  ASSERT_FALSE(TryGetGamesCatalogPath(test_pref_service_.get(), &path));
  ASSERT_TRUE(path.empty());
}

TEST_F(GamesPrefsTest, SetGetGamesCatalogPath_Valid) {
  base::FilePath expected_path(FILE_PATH_LITERAL("some/long/path"));
  SetGamesCatalogPath(test_pref_service_.get(), expected_path);

  base::FilePath path;
  ASSERT_TRUE(TryGetGamesCatalogPath(test_pref_service_.get(), &path));
  ASSERT_EQ(expected_path, path);
}

TEST_F(GamesPrefsTest, GetHighlightedGamesPath_Empty) {
  base::FilePath path;
  ASSERT_FALSE(TryGetHighlightedGamesPath(test_pref_service_.get(), &path));
  ASSERT_TRUE(path.empty());
}

TEST_F(GamesPrefsTest, SetGetHighlightedGamesPath_Valid) {
  base::FilePath expected_path(FILE_PATH_LITERAL("some/long/path"));
  SetHighlightedGamesPath(test_pref_service_.get(), expected_path);

  base::FilePath path;
  ASSERT_TRUE(TryGetHighlightedGamesPath(test_pref_service_.get(), &path));
  ASSERT_EQ(expected_path, path);
}

}  // namespace prefs
}  // namespace games
