// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/shown_sections_handler.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/common/json_pref_store.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class ShownSectionsHandlerTest : public testing::Test {
};

namespace {

int MigratePrefValue(PrefService* prefs, int starting_value) {
  prefs->SetInteger(prefs::kNTPShownSections, starting_value);
  ShownSectionsHandler::MigrateUserPrefs(prefs, 1, 3);
  return prefs->GetInteger(prefs::kNTPShownSections);
}

}  // namespace

TEST_F(ShownSectionsHandlerTest, MigrateUserPrefs) {
  scoped_ptr<PrefService> pref(new TestingPrefService);

  pref->RegisterIntegerPref(prefs::kNTPShownSections, 0);

  EXPECT_EQ(APPS, MigratePrefValue(pref.get(), APPS));
  EXPECT_EQ(THUMB, MigratePrefValue(pref.get(), THUMB));
  EXPECT_EQ(APPS, MigratePrefValue(pref.get(), APPS | THUMB));

  // 2 is not currently used, but older state may contain it and we should do
  // something reasonable.
  EXPECT_EQ(THUMB, MigratePrefValue(pref.get(), 3));

  // 0 can't correspond to any section, but we should still do something
  // reasonable.
  EXPECT_EQ(THUMB, MigratePrefValue(pref.get(), 0));
}
