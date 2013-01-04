// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/display_preferences.h"

#include "ash/display/display_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"

namespace chromeos {
namespace {

class DisplayPreferencesTest : public ash::test::AshTestBase {
 protected:
  DisplayPreferencesTest() : ash::test::AshTestBase() {}
  virtual ~DisplayPreferencesTest() {}

  virtual void SetUp() OVERRIDE {
    ash::test::AshTestBase::SetUp();
    RegisterDisplayLocalStatePrefs(&local_state_);
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
  void SetupDisplayLayoutPref(int64 id,
                              ash::DisplayLayout::Position layout,
                              int offset) {
    DictionaryPrefUpdate update(&local_state_, prefs::kSecondaryDisplays);
    ash::DisplayLayout display_layout(layout, offset);

    std::string name = base::Int64ToString(id);
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

  void SetupDefaultLayoutPref(ash::DisplayLayout::Position layout,
                              int offset) {
    local_state_.SetInteger(
        prefs::kSecondaryDisplayLayout, static_cast<int>(layout));
    local_state_.SetInteger(prefs::kSecondaryDisplayOffset, offset);
  }

  void SetupPrimaryDisplayId(int64 display_id) {
    local_state_.SetInt64(prefs::kPrimaryDisplayID, display_id);
  }

  void SetupDisplayOverscan(int64 id, const gfx::Insets& insets) {
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

  const PrefService* local_state() const { return &local_state_; }

 private:
  ScopedMockUserManagerEnabler mock_user_manager_;
  TestingPrefServiceSimple local_state_;

  DISALLOW_COPY_AND_ASSIGN(DisplayPreferencesTest);
};

TEST_F(DisplayPreferencesTest, Initialization) {
  UpdateDisplay("100x100,200x200");
  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  int64 dummy_id = id2 + 1;
  ASSERT_NE(id1, dummy_id);

  SetupDisplayLayoutPref(id1, ash::DisplayLayout::BOTTOM, 20);
  SetupDisplayLayoutPref(id2, ash::DisplayLayout::TOP, -10);
  SetupDisplayLayoutPref(dummy_id, ash::DisplayLayout::RIGHT, -30);
  SetupDefaultLayoutPref(ash::DisplayLayout::LEFT, 50);
  SetupPrimaryDisplayId(id2);
  SetupDisplayOverscan(id1, gfx::Insets(10, 10, 10, 10));
  SetupDisplayOverscan(id2, gfx::Insets(20, 20, 20, 20));

  NotifyDisplayLocalStatePrefChanged();
  // Check if the layout settings are notified to the system properly.
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  EXPECT_EQ(id2, screen->GetPrimaryDisplay().id());
  EXPECT_EQ(ash::DisplayLayout::BOTTOM,
            display_controller->GetCurrentDisplayLayout().position);
  EXPECT_EQ(20, display_controller->GetCurrentDisplayLayout().offset);
  EXPECT_EQ("160x160", screen->GetPrimaryDisplay().bounds().size().ToString());
  EXPECT_EQ("80x80",
            ash::ScreenAsh::GetSecondaryDisplay().bounds().size().ToString());
}

TEST_F(DisplayPreferencesTest, BasicSaves) {
  UpdateDisplay("100x100,200x200");
  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  int64 dummy_id = id2 + 1;
  ASSERT_NE(id1, dummy_id);

  LoggedInAsUser();
  ash::DisplayLayout layout(ash::DisplayLayout::TOP, 10);
  SetDisplayLayoutPref(ash::ScreenAsh::GetSecondaryDisplay(),
                       static_cast<int>(layout.position), layout.offset);
  SetDisplayLayoutPref(gfx::Screen::GetNativeScreen()->GetPrimaryDisplay(),
                       ash::DisplayLayout::LEFT, 20);
  SetPrimaryDisplayIDPref(dummy_id);
  SetDisplayOverscan(ash::ScreenAsh::GetNativeScreen()->GetPrimaryDisplay(),
                     gfx::Insets(10, 11, 12, 13));

  scoped_ptr<base::DictionaryValue> serialized_value(
      new base::DictionaryValue());
  ASSERT_TRUE(ash::DisplayLayout::ConvertToValue(layout,
                                                 serialized_value.get()));

  const base::DictionaryValue* displays =
      local_state()->GetDictionary(prefs::kSecondaryDisplays);
  const base::DictionaryValue* display_layout = NULL;
  EXPECT_TRUE(
      displays->GetDictionary(base::Int64ToString(id2), &display_layout));
  EXPECT_TRUE(serialized_value->Equals(display_layout));

  // The default value is set for the last call of SetDisplayLayoutPref()
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

TEST_F(DisplayPreferencesTest, DontSaveInGuestMode) {
  UpdateDisplay("100x100,200x200");

  LoggedInAsGuest();
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  ash::DisplayLayout layout(ash::DisplayLayout::TOP, 10);
  SetDisplayLayoutPref(ash::ScreenAsh::GetSecondaryDisplay(),
                       static_cast<int>(layout.position), layout.offset);
  SetPrimaryDisplayIDPref(id2);
  SetDisplayOverscan(ash::ScreenAsh::GetNativeScreen()->GetPrimaryDisplay(),
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
