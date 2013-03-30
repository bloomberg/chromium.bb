// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/display_preferences.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/display/display_configuration_observer.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/display/output_configurator.h"

namespace chromeos {
namespace {
const char kPrimaryIdKey[] = "primary-id";
const char kMirroredKey[] = "mirrored";
const char kPositionKey[] = "position";
const char kOffsetKey[] = "offset";

class DisplayPreferencesTest : public ash::test::AshTestBase {
 protected:
  DisplayPreferencesTest() : ash::test::AshTestBase() {}
  virtual ~DisplayPreferencesTest() {}

  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(*mock_user_manager_.user_manager(), IsUserLoggedIn())
        .WillRepeatedly(testing::Return(false));
    ash::test::AshTestBase::SetUp();
    RegisterDisplayLocalStatePrefs(local_state_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
    observer_.reset(new DisplayConfigurationObserver());
  }

  virtual void TearDown() OVERRIDE {
    observer_.reset();
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
                                     int offset,
                                     int64 primary_id) {
    DictionaryPrefUpdate update(&local_state_, prefs::kSecondaryDisplays);
    ash::DisplayLayout display_layout(layout, offset);
    display_layout.primary_id = primary_id;

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
        layout, offset, id1);
  }

  void StoreDisplayLayoutPrefForSecondary(int64 id,
                                          ash::DisplayLayout::Position layout,
                                          int offset,
                                          int64 primary_id) {
    StoreDisplayLayoutPrefForName(
        base::Int64ToString(id), layout, offset, primary_id);
  }

  void StoreDefaultLayoutPref(ash::DisplayLayout::Position layout,
                              int offset,
                              int64 primary_id) {
    local_state_.SetInteger(prefs::kSecondaryDisplayLayout,
                            static_cast<int>(layout));
    local_state_.SetInteger(prefs::kSecondaryDisplayOffset, offset);
    local_state_.SetInt64(prefs::kPrimaryDisplayID, primary_id);
  }

  void StoreDisplayOverscan(int64 id, const gfx::Insets& insets) {
    DictionaryPrefUpdate update(&local_state_, prefs::kDisplayProperties);
    const std::string name = base::Int64ToString(id);

    base::DictionaryValue* pref_data = update.Get();
    base::DictionaryValue* insets_value = new base::DictionaryValue();
    insets_value->SetInteger("insets_top", insets.top());
    insets_value->SetInteger("insets_left", insets.left());
    insets_value->SetInteger("insets_bottom", insets.bottom());
    insets_value->SetInteger("insets_right", insets.right());
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
  scoped_ptr<DisplayConfigurationObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(DisplayPreferencesTest);
};

TEST_F(DisplayPreferencesTest, DefaultLayout) {
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();

  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();
  int64 id2 = id1 + 1;
  StoreDefaultLayoutPref(ash::DisplayLayout::LEFT, 50, id2);
  LoadDisplayPreferences(true);
  EXPECT_EQ("left, 50",
            display_controller->default_display_layout().ToString());

  UpdateDisplay("100x100,200x200");
  // Displays are swapped, so does the layout.
  EXPECT_EQ("right, -50",
            display_controller->GetCurrentDisplayLayout().ToString());
  EXPECT_EQ(id2,
            display_controller->GetCurrentDisplayLayout().primary_id);
}

TEST_F(DisplayPreferencesTest, OldInitialization) {
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();

  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();
  int64 id2 = id1 + 1;
  int64 dummy_id = id2 + 1;

  StoreDisplayLayoutPrefForSecondary(id2, ash::DisplayLayout::BOTTOM, 20, id2);
  StoreDisplayLayoutPrefForSecondary(
      dummy_id, ash::DisplayLayout::TOP, -10, id1);
  StoreDisplayOverscan(id1, gfx::Insets(10, 10, 10, 10));
  StoreDisplayOverscan(id2, gfx::Insets(20, 20, 20, 20));

  LoadDisplayPreferences(true);

  UpdateDisplay("100x100,200x200");
  EXPECT_EQ(id1, ash::ScreenAsh::GetSecondaryDisplay().id());

  // Check if the layout settings are notified to the system properly.
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  EXPECT_EQ(id2, screen->GetPrimaryDisplay().id());
  // Display was swapped, so the layout was inverted.
  EXPECT_EQ("top, -20",
            display_controller->GetCurrentDisplayLayout().ToString());

  EXPECT_EQ("bottom, 20", GetRegisteredDisplayLayoutStr(id1, id2));
  EXPECT_EQ("top, -10", GetRegisteredDisplayLayoutStr(id1, dummy_id));
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
  StoreDefaultLayoutPref(ash::DisplayLayout::LEFT, 50, id2);
  StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);

  ash::Shell* shell = ash::Shell::GetInstance();

  LoadDisplayPreferences(true);
  // DisplayPowerState should be ignored at boot.
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON,
            shell->output_configurator()->display_power_state());

  shell->display_manager()->UpdateDisplays();
  // Check if the layout settings are notified to the system properly.
  // The paired layout overrides old layout.
  ash::DisplayController* display_controller = shell->display_controller();
  // Inverted one of for specified pair (id1, id2).  Not used for the pair
  // (id1, dummy_id) since dummy_id is not connected right now.
  EXPECT_EQ("top, 20",
            display_controller->GetCurrentDisplayLayout().ToString());
  EXPECT_EQ("top, 20", GetRegisteredDisplayLayoutStr(id1, id2));
  EXPECT_EQ("left, 30", GetRegisteredDisplayLayoutStr(id1, dummy_id));
}

TEST_F(DisplayPreferencesTest, BasicStores) {
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  ash::internal::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();

  UpdateDisplay("200x200*2,200x200");
  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  int64 dummy_id = id2 + 1;
  ASSERT_NE(id1, dummy_id);

  LoggedInAsUser();
  ash::DisplayLayout layout(ash::DisplayLayout::TOP, 10);
  SetCurrentAndDefaultDisplayLayout(layout);
  StoreDisplayLayoutPrefForTest(
      id1, dummy_id, ash::DisplayLayout(ash::DisplayLayout::LEFT, 20));
  // Can't switch to a display that does not exist.
  display_controller->SetPrimaryDisplayId(dummy_id);
  EXPECT_NE(dummy_id, display_controller->GetPrimaryDisplay().id());

  display_controller->SetOverscanInsets(id1, gfx::Insets(10, 11, 12, 13));
  display_manager->SetDisplayRotation(id1, gfx::Display::ROTATE_90);
  display_manager->SetDisplayUIScale(id1, 1.25f);
  display_manager->SetDisplayUIScale(id2, 1.25f);

  const base::DictionaryValue* displays =
      local_state()->GetDictionary(prefs::kSecondaryDisplays);
  const base::DictionaryValue* layout_value = NULL;
  std::string key = base::Int64ToString(id1) + "," + base::Int64ToString(id2);
  EXPECT_TRUE(displays->GetDictionary(key, &layout_value));

  ash::DisplayLayout stored_layout;
  EXPECT_TRUE(ash::DisplayLayout::ConvertFromValue(*layout_value,
                                                   &stored_layout));
  EXPECT_EQ(layout.position, stored_layout.position);
  EXPECT_EQ(layout.offset, stored_layout.offset);

  bool mirrored = true;
  EXPECT_TRUE(layout_value->GetBoolean(kMirroredKey, &mirrored));
  EXPECT_FALSE(mirrored);

  // The default value is set for the last call of
  // SetCurrentAndDefaultDisplayLayout
  EXPECT_EQ(ash::DisplayLayout::TOP,
            local_state()->GetInteger(prefs::kSecondaryDisplayLayout));
  EXPECT_EQ(10, local_state()->GetInteger(prefs::kSecondaryDisplayOffset));

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayProperties);
  const base::DictionaryValue* property = NULL;
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id1), &property));
  int ui_scale = 0;
  int rotation = 0;
  EXPECT_TRUE(property->GetInteger("rotation", &rotation));
  EXPECT_TRUE(property->GetInteger("ui-scale", &ui_scale));
  EXPECT_EQ(1, rotation);
  EXPECT_EQ(1250, ui_scale);

  int top = 0, left = 0, bottom = 0, right = 0;
  EXPECT_TRUE(property->GetInteger("insets_top", &top));
  EXPECT_TRUE(property->GetInteger("insets_left", &left));
  EXPECT_TRUE(property->GetInteger("insets_bottom", &bottom));
  EXPECT_TRUE(property->GetInteger("insets_right", &right));
  EXPECT_EQ(10, top);
  EXPECT_EQ(11, left);
  EXPECT_EQ(12, bottom);
  EXPECT_EQ(13, right);

  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id2), &property));
  EXPECT_TRUE(property->GetInteger("rotation", &rotation));
  EXPECT_TRUE(property->GetInteger("ui-scale", &ui_scale));
  EXPECT_EQ(0, rotation);
  // ui_scale works only on 2x scale factor/1st display.
  EXPECT_EQ(1000, ui_scale);
  EXPECT_FALSE(property->GetInteger("insets_top", &top));
  EXPECT_FALSE(property->GetInteger("insets_left", &left));
  EXPECT_FALSE(property->GetInteger("insets_bottom", &bottom));
  EXPECT_FALSE(property->GetInteger("insets_right", &right));

  display_controller->SetPrimaryDisplayId(id2);

  // The layout remains the same.
  EXPECT_TRUE(displays->GetDictionary(key, &layout_value));
  EXPECT_TRUE(ash::DisplayLayout::ConvertFromValue(*layout_value,
                                                   &stored_layout));
  EXPECT_EQ(layout.position, stored_layout.position);
  EXPECT_EQ(layout.offset, stored_layout.offset);
  EXPECT_EQ(id2, stored_layout.primary_id);

  mirrored = true;
  EXPECT_TRUE(layout_value->GetBoolean(kMirroredKey, &mirrored));
  EXPECT_FALSE(mirrored);
  std::string primary_id_str;
  EXPECT_TRUE(layout_value->GetString(kPrimaryIdKey, &primary_id_str));
  EXPECT_EQ(base::Int64ToString(id2), primary_id_str);

  // Default value should changte.
  EXPECT_EQ(ash::DisplayLayout::TOP,
            local_state()->GetInteger(prefs::kSecondaryDisplayLayout));
  EXPECT_EQ(10, local_state()->GetInteger(prefs::kSecondaryDisplayOffset));

  SetCurrentAndDefaultDisplayLayout(
      ash::DisplayLayout(ash::DisplayLayout::BOTTOM, 20));
  // Displays are swapped, so does the default layout.
  EXPECT_EQ(ash::DisplayLayout::TOP,
            local_state()->GetInteger(prefs::kSecondaryDisplayLayout));
  EXPECT_EQ(-20, local_state()->GetInteger(prefs::kSecondaryDisplayOffset));

  UpdateDisplay("200x200*2,1+0-200x200");
  // Mirrored.
  int offset = 0;
  std::string position;
  EXPECT_TRUE(displays->GetDictionary(key, &layout_value));
  EXPECT_TRUE(layout_value->GetString(kPositionKey, &position));
  EXPECT_EQ("top", position);
  EXPECT_TRUE(layout_value->GetInteger(kOffsetKey, &offset));
  EXPECT_EQ(-20, offset);
  mirrored = false;
  EXPECT_TRUE(layout_value->GetBoolean(kMirroredKey, &mirrored));
  EXPECT_TRUE(mirrored);
  EXPECT_TRUE(layout_value->GetString(kPrimaryIdKey, &primary_id_str));
  EXPECT_EQ(base::Int64ToString(id2), primary_id_str);

  UpdateDisplay("200x200*2,200x200");
  // Update key as the 2nd display gets new id.
  id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  key = base::Int64ToString(id1) + "," + base::Int64ToString(id2);
  EXPECT_TRUE(displays->GetDictionary(key, &layout_value));
  EXPECT_TRUE(layout_value->GetString(kPositionKey, &position));
  EXPECT_EQ("right", position);
  EXPECT_TRUE(layout_value->GetInteger(kOffsetKey, &offset));
  EXPECT_EQ(0, offset);
  mirrored = true;
  EXPECT_TRUE(layout_value->GetBoolean(kMirroredKey, &mirrored));
  EXPECT_FALSE(mirrored);
  EXPECT_TRUE(layout_value->GetString(kPrimaryIdKey, &primary_id_str));
  EXPECT_EQ(base::Int64ToString(id1), primary_id_str);
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
  SetCurrentAndDefaultDisplayLayout(layout);
  layout = layout.Invert();

  const base::DictionaryValue* displays =
      local_state()->GetDictionary(prefs::kSecondaryDisplays);
  const base::DictionaryValue* new_value = NULL;
  std::string key = base::Int64ToString(id1) + "," + base::Int64ToString(id2);
  EXPECT_TRUE(displays->GetDictionary(key, &new_value));

  ash::DisplayLayout stored_layout;
  EXPECT_TRUE(ash::DisplayLayout::ConvertFromValue(*new_value, &stored_layout));
  EXPECT_EQ(layout.position, stored_layout.position);
  EXPECT_EQ(layout.offset, stored_layout.offset);
  EXPECT_EQ(id2, stored_layout.primary_id);

  display_controller->SwapPrimaryDisplay();
  EXPECT_TRUE(displays->GetDictionary(key, &new_value));
  EXPECT_TRUE(ash::DisplayLayout::ConvertFromValue(*new_value, &stored_layout));
  EXPECT_EQ(layout.position, stored_layout.position);
  EXPECT_EQ(layout.offset, stored_layout.offset);
  EXPECT_EQ(id1, stored_layout.primary_id);
}

TEST_F(DisplayPreferencesTest, DontStoreInGuestMode) {
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  ash::internal::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();

  UpdateDisplay("200x200*2,200x200");

  LoggedInAsGuest();
  int64 id1 = ash::ScreenAsh::GetNativeScreen()->GetPrimaryDisplay().id();
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  ash::DisplayLayout layout(ash::DisplayLayout::TOP, 10);
  SetCurrentAndDefaultDisplayLayout(layout);
  display_manager->SetDisplayUIScale(id1, 1.25f);
  display_controller->SetPrimaryDisplayId(id2);
  int64 new_primary =
      ash::ScreenAsh::GetNativeScreen()->GetPrimaryDisplay().id();
  display_controller->SetOverscanInsets(
      new_primary,
      gfx::Insets(10, 11, 12, 13));
  display_manager->SetDisplayRotation(new_primary, gfx::Display::ROTATE_90);

  // Does not store the preferences locally.
  EXPECT_FALSE(local_state()->FindPreference(
      prefs::kSecondaryDisplays)->HasUserSetting());
  EXPECT_FALSE(local_state()->FindPreference(
      prefs::kSecondaryDisplayLayout)->HasUserSetting());
  EXPECT_FALSE(local_state()->FindPreference(
      prefs::kSecondaryDisplayOffset)->HasUserSetting());
  EXPECT_FALSE(local_state()->FindPreference(
      prefs::kDisplayProperties)->HasUserSetting());

  // Settings are still notified to the system.
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  EXPECT_EQ(id2, screen->GetPrimaryDisplay().id());
  EXPECT_EQ(ash::DisplayLayout::BOTTOM,
            display_controller->GetCurrentDisplayLayout().position);
  EXPECT_EQ(-10, display_controller->GetCurrentDisplayLayout().offset);
  const gfx::Display& primary_display = screen->GetPrimaryDisplay();
  EXPECT_EQ("178x176", primary_display.bounds().size().ToString());
  EXPECT_EQ(gfx::Display::ROTATE_90, primary_display.rotation());

  const ash::internal::DisplayInfo& info1 =
      display_manager->GetDisplayInfo(id1);
  EXPECT_EQ(1.25f, info1.ui_scale());

  const ash::internal::DisplayInfo& info_primary =
      display_manager->GetDisplayInfo(new_primary);
  EXPECT_EQ(gfx::Display::ROTATE_90, info_primary.rotation());
  EXPECT_EQ(1.0f, info_primary.ui_scale());
}

TEST_F(DisplayPreferencesTest, DisplayPowerStateAfterRestart) {
  StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);
  LoadDisplayPreferences(false);
  EXPECT_EQ(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
      ash::Shell::GetInstance()->output_configurator()->display_power_state());
}

}  // namespace
}  // namespace chromeos
