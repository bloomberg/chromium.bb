// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/launcher_application_menu_item_model.h"

#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/ui/ash/launcher/test/launcher_application_menu_item_model_test_api.h"
#include "chrome/browser/ui/ash/launcher/test/test_chrome_launcher_app_menu_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kNumItemsEnabledHistogramName[] =
    "Ash.Shelf.Menu.NumItemsEnabledUponSelection";

const char kSelectedMenuItemIndexHistogramName[] =
    "Ash.Shelf.Menu.SelectedMenuItemIndex";

}  // namespace

// Verifies LauncherApplicationMenuItemModel::GetNumMenuItemsEnabled() return
// value when there are zero menu items.
TEST(LauncherApplicationMenuItemModelTest,
     VerifyGetNumMenuItemsEnabledWithNoMenuItems) {
  ChromeLauncherAppMenuItems menu_items;
  LauncherApplicationMenuItemModel menu_model(menu_items.Pass());
  LauncherApplicationMenuItemModelTestAPI menu_model_test_api(&menu_model);

  EXPECT_EQ(0, menu_model_test_api.GetNumMenuItemsEnabled());
}

// Verifies LauncherApplicationMenuItemModel::GetNumMenuItemsEnabled() return
// value when there are a mix of enabled and disabled menu items.
TEST(LauncherApplicationMenuItemModelTest,
     VerifyGetNumMenuItemsEnabledWithMenuItems) {
  ChromeLauncherAppMenuItems menu_items;

  TestChromeLauncherAppMenuItem* enabled_menu_item_1 =
      new TestChromeLauncherAppMenuItem();
  enabled_menu_item_1->set_is_enabled(true);

  TestChromeLauncherAppMenuItem* enabled_menu_item_2 =
      new TestChromeLauncherAppMenuItem();
  enabled_menu_item_2->set_is_enabled(true);

  TestChromeLauncherAppMenuItem* enabled_menu_item_3 =
      new TestChromeLauncherAppMenuItem();
  enabled_menu_item_3->set_is_enabled(true);

  TestChromeLauncherAppMenuItem* disabled_menu_item_1 =
      new TestChromeLauncherAppMenuItem();
  disabled_menu_item_1->set_is_enabled(false);

  menu_items.push_back(enabled_menu_item_1);
  menu_items.push_back(disabled_menu_item_1);
  menu_items.push_back(enabled_menu_item_2);
  menu_items.push_back(enabled_menu_item_3);

  LauncherApplicationMenuItemModel menu_model(menu_items.Pass());
  LauncherApplicationMenuItemModelTestAPI menu_model_test_api(&menu_model);

  EXPECT_EQ(3, menu_model_test_api.GetNumMenuItemsEnabled());
}

// Verifies the correct histogram buckets are recorded for
// LauncherApplicationMenuItemModel::RecordMenuItemSelectedMetrics.
TEST(LauncherApplicationMenuItemModelTest,
     VerifyHistogramBucketsRecordedByRecordMenuItemSelectedMetrics) {
  const int kCommandId = 3;
  const int kNumMenuItemsEnabled = 7;

  base::HistogramTester histogram_tester;

  ChromeLauncherAppMenuItems menu_items;
  LauncherApplicationMenuItemModel menu_model(menu_items.Pass());
  LauncherApplicationMenuItemModelTestAPI menu_model_test_api(&menu_model);
  menu_model_test_api.RecordMenuItemSelectedMetrics(kCommandId,
                                                    kNumMenuItemsEnabled);

  histogram_tester.ExpectTotalCount(kNumItemsEnabledHistogramName, 1);
  histogram_tester.ExpectBucketCount(kNumItemsEnabledHistogramName,
                                     kNumMenuItemsEnabled, 1);

  histogram_tester.ExpectTotalCount(kSelectedMenuItemIndexHistogramName, 1);
  histogram_tester.ExpectBucketCount(kSelectedMenuItemIndexHistogramName,
                                     kCommandId, 1);
}

// Verify histogram data is recorded when
// LauncherApplicationMenuItemModel::ExecuteCommand is called.
TEST(LauncherApplicationMenuItemModelTest,
     VerifyHistogramDataIsRecordedWhenExecuteCommandIsCalled) {
  const int kCommandId = 0;
  const int kFlags = 0;

  ChromeLauncherAppMenuItems menu_items;
  TestChromeLauncherAppMenuItem* menu_item =
      new TestChromeLauncherAppMenuItem();
  menu_item->set_is_enabled(true);
  menu_items.push_back(menu_item);

  base::HistogramTester histogram_tester;

  LauncherApplicationMenuItemModel menu_model(menu_items.Pass());
  menu_model.ExecuteCommand(kCommandId, kFlags);

  histogram_tester.ExpectTotalCount(kNumItemsEnabledHistogramName, 1);
  histogram_tester.ExpectTotalCount(kSelectedMenuItemIndexHistogramName, 1);
}
