// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/shown_sections_handler.h"

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class ShownSectionsHandlerTest : public testing::Test {
};

TEST_F(ShownSectionsHandlerTest, MigrateUserPrefs) {
  PrefService pref((FilePath()));

  // Set an *old* value
  pref.RegisterIntegerPref(prefs::kNTPShownSections, 0);
  pref.SetInteger(prefs::kNTPShownSections, THUMB);

  ShownSectionsHandler::MigrateUserPrefs(&pref, 0, 1);

  int shown_sections = pref.GetInteger(prefs::kNTPShownSections);

  EXPECT_TRUE(shown_sections & THUMB);
  EXPECT_FALSE(shown_sections & LIST);
  EXPECT_FALSE(shown_sections & RECENT);
  EXPECT_TRUE(shown_sections & TIPS);
  EXPECT_TRUE(shown_sections & SYNC);
}
