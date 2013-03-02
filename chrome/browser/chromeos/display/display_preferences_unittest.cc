// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/display_preferences.h"

#include "ash/display/display_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"

namespace chromeos {
namespace {

class DisplayPreferencesTest : public ash::test::AshTestBase {
 protected:
  DisplayPreferencesTest() : ash::test::AshTestBase() {}
  virtual ~DisplayPreferencesTest() {}

  virtual void SetUp() OVERRIDE {
    ash::test::AshTestBase::SetUp();
    RegisterDisplayLocalStatePrefs(local_state_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
  }

  virtual void TearDown() OVERRIDE {
    TestingBrowserProcess::GetGlobal()->SetLocalState(NULL);
    ash::test::AshTestBase::TearDown();
  }

  void LoggedInAsUser() {
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsUserLoggedIn())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsLoggedInAsDemoUser())
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsLoggedInAsGuest())
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsLoggedInAsStub())
        .WillRepeatedly(testing::Return(false));
  }

  void LoggedInAsGuest() {
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsUserLoggedIn())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsLoggedInAsDemoUser())
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsLoggedInAsGuest())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsLoggedInAsStub())
        .WillRepeatedly(testing::Return(false));
  }

  // Do not use the implementation of display_preferences.cc directly to avoid
  // notifying the update to the system.
  void StoreDisplayLayoutPrefForName(const std::string& name,
                                     ash::DisplayLayout::Position layout,
                                     int offset) {
    DictionaryPrefUpdate update(&local_state_, prefs::kSecondaryDisplays);
    ash::DisplayLayout display_layout(layout, offset);

    DCHECK(!name.empty());

    base::DictionaryValue* pref_data = update.Get();
    scoped_ptr<base::Value>layout_value(new base::DictionaryValue());
    if (pref_data->HasKey(name)) {
      base::Value* value = NULL;
      if (pref_data->Get(name, &value) && value != NULL)
        layout_value.reset(value->DeepCopy());
    }
    if (ash::DisplayLayout::ConvertToValue(display_layout, layout_value.get()))
      pref_data->Set(name, layout_value.release());
  }

  void StoreDisplayLayoutPrefForPair(int64 id1,
                                     int64 id2,
                                     ash::DisplayLayout::Position layout,
                                     int offset) {
    StoreDisplayLayoutPrefForName(
        base::Int64ToString(id1) + "," + base::Int64ToString(id2),
        layout, offset);
  }

  void StoreDisplayLayoutPrefForSecondary(int64 id,
                                          ash::DisplayLayout::Position layout,
                                          int offset) {
    StoreDisplayLayoutPrefForName(base::Int64ToString(id), layout, offset);
  }

  void StoreDefaultLayoutPref(ash::DisplayLayout::Position layout,
                              int offset) {
    local_state_.SetInteger(
        prefs::kSecondaryDisplayLayout, static_cast<int>(layout));
    local_state_.SetInteger(prefs::kSecondaryDisplayOffset, offset);
  }

  void StorePrimaryDisplayId(int64 display_id) {
    local_state_.SetInt64(prefs::kPrimaryDisplayID, display_id);
  }

  void StoreDisplayOverscan(int64 id, const gfx::Insets& insets) {
    DictionaryPrefUpdate update(&local_state_, prefs::kDisplayOverscans);
    const std::string name = base::Int64ToString(id);

    base::DictionaryValue* pref_data = update.Get();
    base::DictionaryValue* insets_value = new base::DictionaryValue();
    insets_value->SetInteger("top", insets.top());
    insets_value->SetInteger("left", insets.left());
    insets_value->SetInteger("bottom", insets.bottom());
    insets_value->SetInteger("right", insets.right());
    pref_data->Set(name, insets_value);
  }

  std::string GetRegisteredDisplayLayoutStr(int64 id1, int64 id2) {
    ash::DisplayIdPair pair;
    pair.first = id1;
    pair.second = id2;
    return ash::Shell::GetInstance()->display_controller()->
        GetRegisteredDisplayLayout(pair).ToString();
  }

  const PrefService* local_state() const { return &local_state_; }

 private:
  ScopedMockUserManagerEnabler mock_user_manager_;
  TestingPrefServiceSimple local_state_;

  DISALLOW_COPY_AND_ASSIGN(DisplayPreferencesTest);
};

TEST_F(DisplayPreferencesTest, OldInitialization) {
  UpdateDisplay("100x100,200x200");
  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  int64 dummy_id = id2 + 1;
  ASSERT_NE(id1, dummy_id);

  StoreDisplayLayoutPrefForSecondary(id2, ash::DisplayLayout::BOTTOM, 20);
  StoreDisplayLayoutPrefForSecondary(dummy_id, ash::DisplayLayout::TOP, -10);
  StoreDefaultLayoutPref(ash::DisplayLayout::LEFT, 50);
  StorePrimaryDisplayId(id2);
  StoreDisplayOverscan(id1, gfx::Insets(10, 10, 10, 10));
  StoreDisplayOverscan(id2, gfx::Insets(20, 20, 20, 20));

  NotifyDisplayLocalStatePrefChanged();
  // Check if the layout settings are notified to the system properly.
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  EXPECT_EQ(id2, screen->GetPrimaryDisplay().id());
  // Display was swapped, so the layout was inverted.
  EXPECT_EQ("top, -20",
            display_controller->GetCurrentDisplayLayout().ToString());

  EXPECT_EQ("bottom, 20", GetRegisteredDisplayLayoutStr(id1, id2));
  EXPECT_EQ("top, -10", GetRegisteredDisplayLayoutStr(id1, dummy_id));
  EXPECT_EQ("left, 50",
            display_controller->default_display_layout().ToString());
  EXPECT_EQ("160x160", screen->GetPrimaryDisplay().bounds().size().ToString());
  EXPECT_EQ("80x80",
            ash::ScreenAsh::GetSecondaryDisplay().bounds().size().ToString());
}

TEST_F(DisplayPreferencesTest, PairedLayoutOverrides) {
  UpdateDisplay("100x100,200x200");
  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  int64 dummy_id = id2 + 1;
  ASSERT_NE(id1, dummy_id);

  StoreDisplayLayoutPrefForPair(id1, id2, ash::DisplayLayout::TOP, 20);
  StoreDisplayLayoutPrefForPair(id1, dummy_id, ash::DisplayLayout::LEFT, 30);
  StoreDefaultLayoutPref(ash::DisplayLayout::LEFT, 50);

  NotifyDisplayLocalStatePrefChanged();
  // Check if the layout settings are notified to the system properly.
  // The paired layout overrides old layout.
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  // Inverted one of for specified pair (id1, id2).  Not used for the pair
  // (id1, dummy_id) since dummy_id is not connected right now.
  EXPECT_EQ("top, 20",
            display_controller->GetCurrentDisplayLayout().ToString());
  EXPECT_EQ("top, 20", GetRegisteredDisplayLayoutStr(id1, id2));
  EXPECT_EQ("left, 30", GetRegisteredDisplayLayoutStr(id1, dummy_id));
}

TEST_F(DisplayPreferencesTest, BasicStores) {
  UpdateDisplay("100x100,200x200");
  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  int64 dummy_id = id2 + 1;
  ASSERT_NE(id1, dummy_id);

  LoggedInAsUser();
  ash::DisplayLayout layout(ash::DisplayLayout::TOP, 10);
  SetAndStoreDisplayLayoutPref(static_cast<int>(layout.position),
                               layout.offset);
  StoreDisplayLayoutPref(id1, dummy_id, ash::DisplayLayout::LEFT, 20);
  SetAndStorePrimaryDisplayIDPref(dummy_id);
  SetAndStoreDisplayOverscan(
      ash::ScreenAsh::GetNativeScreen()->GetPrimaryDisplay(),
      gfx::Insets(10, 11, 12, 13));

  scoped_ptr<base::DictionaryValue> serialized_value(
      new base::DictionaryValue());
  ASSERT_TRUE(ash::DisplayLayout::ConvertToValue(layout,
                                                 serialized_value.get()));

  const base::DictionaryValue* displays =
      local_state()->GetDictionary(prefs::kSecondaryDisplays);
  const base::DictionaryValue* display_layout = NULL;
  std::string key = base::Int64ToString(id1) + "," + base::Int64ToString(id2);
  EXPECT_TRUE(displays->GetDictionary(key, &display_layout));
  EXPECT_TRUE(serialized_value->Equals(display_layout));

  // The default value is set for the last call of
  // StoreDisplayLayoutPref()
  EXPECT_EQ(ash::DisplayLayout::LEFT,
            local_state()->GetInteger(prefs::kSecondaryDisplayLayout));
  EXPECT_EQ(20, local_state()->GetInteger(prefs::kSecondaryDisplayOffset));

  const base::DictionaryValue* overscans =
      local_state()->GetDictionary(prefs::kDisplayOverscans);
  const base::DictionaryValue* overscan = NULL;
  EXPECT_TRUE(overscans->GetDictionary(base::Int64ToString(id1), &overscan));
  int top = 0, left = 0, bottom = 0, right = 0;
  EXPECT_TRUE(overscan->GetInteger("top", &top));
  EXPECT_TRUE(overscan->GetInteger("left", &left));
  EXPECT_TRUE(overscan->GetInteger("bottom", &bottom));
  EXPECT_TRUE(overscan->GetInteger("right", &right));
  EXPECT_EQ(10, top);
  EXPECT_EQ(11, left);
  EXPECT_EQ(12, bottom);
  EXPECT_EQ(13, right);
  EXPECT_EQ(dummy_id, local_state()->GetInt64(prefs::kPrimaryDisplayID));
}

TEST_F(DisplayPreferencesTest, StoreForSwappedDisplay) {
  UpdateDisplay("100x100,200x200");
  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();

  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  display_controller->SwapPrimaryDisplay();
  ASSERT_EQ(id1, ash::ScreenAsh::GetSecondaryDisplay().id());

  LoggedInAsUser();
  ash::DisplayLayout layout(ash::DisplayLayout::TOP, 10);
  SetAndStoreDisplayLayoutPref(static_cast<int>(layout.position),
                               layout.offset);

  scoped_ptr<base::DictionaryValue> layout_value(
      new base::DictionaryValue());
  ASSERT_TRUE(ash::DisplayLayout::ConvertToValue(layout, layout_value.get()));

  const base::DictionaryValue* displays =
      local_state()->GetDictionary(prefs::kSecondaryDisplays);
  const base::DictionaryValue* new_value = NULL;
  std::string key = base::Int64ToString(id1) + "," + base::Int64ToString(id2);
  EXPECT_TRUE(displays->GetDictionary(key, &new_value));
  EXPECT_TRUE(layout_value->Equals(new_value));

  display_controller->SwapPrimaryDisplay();
  EXPECT_TRUE(displays->GetDictionary(key, &new_value));
  EXPECT_TRUE(layout_value->Equals(new_value));
}

TEST_F(DisplayPreferencesTest, DontStoreInGuestMode) {
  UpdateDisplay("100x100,200x200");

  LoggedInAsGuest();
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  ash::DisplayLayout layout(ash::DisplayLayout::TOP, 10);
  SetAndStoreDisplayLayoutPref(static_cast<int>(layout.position),
                               layout.offset);
  SetAndStorePrimaryDisplayIDPref(id2);
  SetAndStoreDisplayOverscan(
      ash::ScreenAsh::GetNativeScreen()->GetPrimaryDisplay(),
      gfx::Insets(10, 11, 12, 13));

  // Does not store the preferences locally.
  EXPECT_FALSE(local_state()->FindPreference(
      prefs::kSecondaryDisplays)->HasUserSetting());
  EXPECT_FALSE(local_state()->FindPreference(
      prefs::kSecondaryDisplayLayout)->HasUserSetting());
  EXPECT_FALSE(local_state()->FindPreference(
      prefs::kSecondaryDisplayOffset)->HasUserSetting());
  EXPECT_FALSE(local_state()->FindPreference(
      prefs::kPrimaryDisplayID)->HasUserSetting());
  EXPECT_FALSE(local_state()->FindPreference(
      prefs::kDisplayOverscans)->HasUserSetting());

  // Settings are still notified to the system.
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  EXPECT_EQ(id2, screen->GetPrimaryDisplay().id());
  EXPECT_EQ(ash::DisplayLayout::BOTTOM,
            display_controller->GetCurrentDisplayLayout().position);
  EXPECT_EQ(-10, display_controller->GetCurrentDisplayLayout().offset);
  EXPECT_EQ("176x178", screen->GetPrimaryDisplay().bounds().size().ToString());
}

}  // namespace
}  // namespace chromeos
