// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/shown_sections_handler.h"

#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/pref_value_store.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class ShownSectionsHandlerTest : public testing::Test {
};

TEST_F(ShownSectionsHandlerTest, MigrateUserPrefs) {
  scoped_ptr<PrefService> pref(new TestingPrefService);

  // Set an *old* value
  pref->RegisterIntegerPref(prefs::kNTPShownSections, 0);
  pref->SetInteger(prefs::kNTPShownSections, THUMB);

  ShownSectionsHandler::MigrateUserPrefs(pref.get(), 0, 1);

  int shown_sections = pref->GetInteger(prefs::kNTPShownSections);

  EXPECT_TRUE(shown_sections & THUMB);
  EXPECT_FALSE(shown_sections & LIST);
  EXPECT_FALSE(shown_sections & RECENT);
  EXPECT_TRUE(shown_sections & TIPS);
  EXPECT_TRUE(shown_sections & SYNC);
}

TEST_F(ShownSectionsHandlerTest, MigrateUserPrefs1To2) {
  scoped_ptr<PrefService> pref(new TestingPrefService);

  // Set an *old* value
  pref->RegisterIntegerPref(prefs::kNTPShownSections, 0);
  pref->SetInteger(prefs::kNTPShownSections, LIST);

  ShownSectionsHandler::MigrateUserPrefs(pref.get(), 1, 2);

  int shown_sections = pref->GetInteger(prefs::kNTPShownSections);

  EXPECT_TRUE(shown_sections & THUMB);
  EXPECT_FALSE(shown_sections & LIST);
}
