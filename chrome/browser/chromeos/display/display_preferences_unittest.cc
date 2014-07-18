// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/display_preferences.h"

#include <string>
#include <vector>

#include "ash/display/display_controller.h"
#include "ash/display/display_layout_store.h"
#include "ash/display/display_manager.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/wm/maximize_mode/maximize_mode_controller.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/display/display_configuration_observer.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/gfx/vector3d_f.h"
#include "ui/message_center/message_center.h"

using ash::ResolutionNotificationController;

namespace chromeos {
namespace {
const char kPrimaryIdKey[] = "primary-id";
const char kMirroredKey[] = "mirrored";
const char kPositionKey[] = "position";
const char kOffsetKey[] = "offset";

class DisplayPreferencesTest : public ash::test::AshTestBase {
 protected:
  DisplayPreferencesTest()
      : mock_user_manager_(new MockUserManager),
        user_manager_enabler_(mock_user_manager_) {
  }

  virtual ~DisplayPreferencesTest() {}

  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(*mock_user_manager_, IsUserLoggedIn())
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(*mock_user_manager_, Shutdown());
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
    EXPECT_CALL(*mock_user_manager_, IsUserLoggedIn())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_user_manager_, IsLoggedInAsRegularUser())
        .WillRepeatedly(testing::Return(true));
  }

  void LoggedInAsGuest() {
    EXPECT_CALL(*mock_user_manager_, IsUserLoggedIn())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_user_manager_, IsLoggedInAsRegularUser())
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(*mock_user_manager_, IsLoggedInAsSupervisedUser())
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

  void StoreColorProfile(int64 id, const std::string& profile) {
    DictionaryPrefUpdate update(&local_state_, prefs::kDisplayProperties);
    const std::string name = base::Int64ToString(id);

    base::DictionaryValue* pref_data = update.Get();
    base::DictionaryValue* property = new base::DictionaryValue();
    property->SetString("color_profile_name", profile);
    pref_data->Set(name, property);
  }

  std::string GetRegisteredDisplayLayoutStr(int64 id1, int64 id2) {
    ash::DisplayIdPair pair;
    pair.first = id1;
    pair.second = id2;
    return ash::Shell::GetInstance()->display_manager()->layout_store()->
        GetRegisteredDisplayLayout(pair).ToString();
  }

  PrefService* local_state() { return &local_state_; }

 private:
  MockUserManager* mock_user_manager_;  // Not owned.
  ScopedUserManagerEnabler user_manager_enabler_;
  TestingPrefServiceSimple local_state_;
  scoped_ptr<DisplayConfigurationObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(DisplayPreferencesTest);
};

}  // namespace

TEST_F(DisplayPreferencesTest, PairedLayoutOverrides) {
  UpdateDisplay("100x100,200x200");
  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();
  int64 id2 = ash::ScreenUtil::GetSecondaryDisplay().id();
  int64 dummy_id = id2 + 1;
  ASSERT_NE(id1, dummy_id);

  StoreDisplayLayoutPrefForPair(id1, id2, ash::DisplayLayout::TOP, 20);
  StoreDisplayLayoutPrefForPair(id1, dummy_id, ash::DisplayLayout::LEFT, 30);
  StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);

  ash::Shell* shell = ash::Shell::GetInstance();

  LoadDisplayPreferences(true);
  // DisplayPowerState should be ignored at boot.
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON,
            shell->display_configurator()->power_state());

  shell->display_manager()->UpdateDisplays();
  // Check if the layout settings are notified to the system properly.
  // The paired layout overrides old layout.
  // Inverted one of for specified pair (id1, id2).  Not used for the pair
  // (id1, dummy_id) since dummy_id is not connected right now.
  EXPECT_EQ("top, 20",
            shell->display_manager()->GetCurrentDisplayLayout().ToString());
  EXPECT_EQ("top, 20", GetRegisteredDisplayLayoutStr(id1, id2));
  EXPECT_EQ("left, 30", GetRegisteredDisplayLayoutStr(id1, dummy_id));
}

TEST_F(DisplayPreferencesTest, BasicStores) {
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();

  UpdateDisplay("200x200*2, 400x300#400x400|300x200");
  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();
  gfx::Display::SetInternalDisplayId(id1);
  int64 id2 = ash::ScreenUtil::GetSecondaryDisplay().id();
  int64 dummy_id = id2 + 1;
  ASSERT_NE(id1, dummy_id);
  std::vector<ui::ColorCalibrationProfile> profiles;
  profiles.push_back(ui::COLOR_PROFILE_STANDARD);
  profiles.push_back(ui::COLOR_PROFILE_DYNAMIC);
  profiles.push_back(ui::COLOR_PROFILE_MOVIE);
  profiles.push_back(ui::COLOR_PROFILE_READING);
  ash::test::DisplayManagerTestApi test_api(display_manager);
  // Allows only |id1|.
  test_api.SetAvailableColorProfiles(id1, profiles);
  display_manager->SetColorCalibrationProfile(id1, ui::COLOR_PROFILE_DYNAMIC);
  display_manager->SetColorCalibrationProfile(id2, ui::COLOR_PROFILE_DYNAMIC);

  LoggedInAsUser();
  ash::DisplayLayout layout(ash::DisplayLayout::TOP, 10);
  SetCurrentDisplayLayout(layout);
  StoreDisplayLayoutPrefForTest(
      id1, dummy_id, ash::DisplayLayout(ash::DisplayLayout::LEFT, 20));
  // Can't switch to a display that does not exist.
  display_controller->SetPrimaryDisplayId(dummy_id);
  EXPECT_NE(dummy_id, ash::Shell::GetScreen()->GetPrimaryDisplay().id());

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

  // Internal display never registered the resolution.
  int width = 0, height = 0;
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  int top = 0, left = 0, bottom = 0, right = 0;
  EXPECT_TRUE(property->GetInteger("insets_top", &top));
  EXPECT_TRUE(property->GetInteger("insets_left", &left));
  EXPECT_TRUE(property->GetInteger("insets_bottom", &bottom));
  EXPECT_TRUE(property->GetInteger("insets_right", &right));
  EXPECT_EQ(10, top);
  EXPECT_EQ(11, left);
  EXPECT_EQ(12, bottom);
  EXPECT_EQ(13, right);

  std::string color_profile;
  EXPECT_TRUE(property->GetString("color_profile_name", &color_profile));
  EXPECT_EQ("dynamic", color_profile);

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

  // |id2| doesn't have the color_profile because it doesn't have 'dynamic' in
  // its available list.
  EXPECT_FALSE(property->GetString("color_profile_name", &color_profile));

  // Resolution is saved only when the resolution is set
  // by DisplayManager::SetDisplayResolution
  width = 0;
  height = 0;
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  display_manager->SetDisplayResolution(id2, gfx::Size(300, 200));

  display_controller->SetPrimaryDisplayId(id2);

  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id1), &property));
  width = 0;
  height = 0;
  // Internal display shouldn't store its resolution.
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  // External display's resolution must be stored this time because
  // it's not best.
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id2), &property));
  EXPECT_TRUE(property->GetInteger("width", &width));
  EXPECT_TRUE(property->GetInteger("height", &height));
  EXPECT_EQ(300, width);
  EXPECT_EQ(200, height);

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

  SetCurrentDisplayLayout(
      ash::DisplayLayout(ash::DisplayLayout::BOTTOM, 20));

  UpdateDisplay("1+0-200x200*2,1+0-200x200");
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

  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id1), &property));
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  // External display's selected resolution must not change
  // by mirroring.
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id2), &property));
  EXPECT_TRUE(property->GetInteger("width", &width));
  EXPECT_TRUE(property->GetInteger("height", &height));
  EXPECT_EQ(300, width);
  EXPECT_EQ(200, height);

  // Set new display's selected resolution.
  display_manager->RegisterDisplayProperty(
      id2 + 1, gfx::Display::ROTATE_0, 1.0f, NULL, gfx::Size(500, 400),
      ui::COLOR_PROFILE_STANDARD);

  UpdateDisplay("200x200*2, 600x500#600x500|500x400");

  // Update key as the 2nd display gets new id.
  id2 = ash::ScreenUtil::GetSecondaryDisplay().id();
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

  // Best resolution should not be saved.
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id2), &property));
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  // Set yet another new display's selected resolution.
  display_manager->RegisterDisplayProperty(
      id2 + 1, gfx::Display::ROTATE_0, 1.0f, NULL, gfx::Size(500, 400),
      ui::COLOR_PROFILE_STANDARD);
  // Disconnect 2nd display first to generate new id for external display.
  UpdateDisplay("200x200*2");
  UpdateDisplay("200x200*2, 500x400#600x500|500x400%60.0f");
  // Update key as the 2nd display gets new id.
  id2 = ash::ScreenUtil::GetSecondaryDisplay().id();
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

  // External display's selected resolution must be updated.
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id2), &property));
  EXPECT_TRUE(property->GetInteger("width", &width));
  EXPECT_TRUE(property->GetInteger("height", &height));
  EXPECT_EQ(500, width);
  EXPECT_EQ(400, height);
}

TEST_F(DisplayPreferencesTest, PreventStore) {
  ResolutionNotificationController::SuppressTimerForTest();
  LoggedInAsUser();
  UpdateDisplay("400x300#500x400|400x300|300x200");
  int64 id = ash::Shell::GetScreen()->GetPrimaryDisplay().id();
  // Set display's resolution in single display. It creates the notification and
  // display preferences should not stored meanwhile.
  ash::Shell::GetInstance()->resolution_notification_controller()->
      SetDisplayResolutionAndNotify(
          id, gfx::Size(400, 300), gfx::Size(500, 400), base::Closure());
  UpdateDisplay("500x400#500x400|400x300|300x200");

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayProperties);
  const base::DictionaryValue* property = NULL;
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id), &property));
  int width = 0, height = 0;
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  // Revert the change. When timeout, 2nd button is revert.
  message_center::MessageCenter::Get()->ClickOnNotificationButton(
      ResolutionNotificationController::kNotificationId, 1);
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          ResolutionNotificationController::kNotificationId));

  // Once the notification is removed, the specified resolution will be stored
  // by SetDisplayResolution.
  ash::Shell::GetInstance()->display_manager()->SetDisplayResolution(
      id, gfx::Size(300, 200));
  UpdateDisplay("300x200#500x400|400x300|300x200");

  property = NULL;
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id), &property));
  EXPECT_TRUE(property->GetInteger("width", &width));
  EXPECT_TRUE(property->GetInteger("height", &height));
  EXPECT_EQ(300, width);
  EXPECT_EQ(200, height);
}

TEST_F(DisplayPreferencesTest, StoreForSwappedDisplay) {
  UpdateDisplay("100x100,200x200");
  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();
  int64 id2 = ash::ScreenUtil::GetSecondaryDisplay().id();

  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  display_controller->SwapPrimaryDisplay();
  ASSERT_EQ(id1, ash::ScreenUtil::GetSecondaryDisplay().id());

  LoggedInAsUser();
  ash::DisplayLayout layout(ash::DisplayLayout::TOP, 10);
  SetCurrentDisplayLayout(layout);
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

TEST_F(DisplayPreferencesTest, RestoreColorProfiles) {
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();

  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();

  StoreColorProfile(id1, "dynamic");

  LoggedInAsUser();
  LoadDisplayPreferences(false);

  // id1's available color profiles list is empty, means somehow the color
  // profile suport is temporary in trouble.
  EXPECT_NE(ui::COLOR_PROFILE_DYNAMIC,
            display_manager->GetDisplayInfo(id1).color_profile());

  // Once the profile is supported, the color profile should be restored.
  std::vector<ui::ColorCalibrationProfile> profiles;
  profiles.push_back(ui::COLOR_PROFILE_STANDARD);
  profiles.push_back(ui::COLOR_PROFILE_DYNAMIC);
  profiles.push_back(ui::COLOR_PROFILE_MOVIE);
  profiles.push_back(ui::COLOR_PROFILE_READING);
  ash::test::DisplayManagerTestApi test_api(display_manager);
  test_api.SetAvailableColorProfiles(id1, profiles);

  LoadDisplayPreferences(false);
  EXPECT_EQ(ui::COLOR_PROFILE_DYNAMIC,
            display_manager->GetDisplayInfo(id1).color_profile());
}

TEST_F(DisplayPreferencesTest, DontStoreInGuestMode) {
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  ash::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();

  UpdateDisplay("200x200*2,200x200");

  LoggedInAsGuest();
  int64 id1 = ash::Shell::GetScreen()->GetPrimaryDisplay().id();
  gfx::Display::SetInternalDisplayId(id1);
  int64 id2 = ash::ScreenUtil::GetSecondaryDisplay().id();
  ash::DisplayLayout layout(ash::DisplayLayout::TOP, 10);
  SetCurrentDisplayLayout(layout);
  display_manager->SetDisplayUIScale(id1, 1.25f);
  display_controller->SetPrimaryDisplayId(id2);
  int64 new_primary = ash::Shell::GetScreen()->GetPrimaryDisplay().id();
  display_controller->SetOverscanInsets(
      new_primary,
      gfx::Insets(10, 11, 12, 13));
  display_manager->SetDisplayRotation(new_primary, gfx::Display::ROTATE_90);

  // Does not store the preferences locally.
  EXPECT_FALSE(local_state()->FindPreference(
      prefs::kSecondaryDisplays)->HasUserSetting());
  EXPECT_FALSE(local_state()->FindPreference(
      prefs::kDisplayProperties)->HasUserSetting());

  // Settings are still notified to the system.
  gfx::Screen* screen = gfx::Screen::GetNativeScreen();
  EXPECT_EQ(id2, screen->GetPrimaryDisplay().id());
  EXPECT_EQ(ash::DisplayLayout::BOTTOM,
            display_manager->GetCurrentDisplayLayout().position);
  EXPECT_EQ(-10, display_manager->GetCurrentDisplayLayout().offset);
  const gfx::Display& primary_display = screen->GetPrimaryDisplay();
  EXPECT_EQ("178x176", primary_display.bounds().size().ToString());
  EXPECT_EQ(gfx::Display::ROTATE_90, primary_display.rotation());

  const ash::DisplayInfo& info1 = display_manager->GetDisplayInfo(id1);
  EXPECT_EQ(1.25f, info1.configured_ui_scale());

  const ash::DisplayInfo& info_primary =
      display_manager->GetDisplayInfo(new_primary);
  EXPECT_EQ(gfx::Display::ROTATE_90, info_primary.rotation());
  EXPECT_EQ(1.0f, info_primary.configured_ui_scale());
}

TEST_F(DisplayPreferencesTest, StorePowerStateNoLogin) {
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayPowerState));

  // Stores display prefs without login, which still stores the power state.
  StoreDisplayPrefs();
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayPowerState));
}

TEST_F(DisplayPreferencesTest, StorePowerStateGuest) {
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayPowerState));

  LoggedInAsGuest();
  StoreDisplayPrefs();
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayPowerState));
}

TEST_F(DisplayPreferencesTest, StorePowerStateNormalUser) {
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayPowerState));

  LoggedInAsUser();
  StoreDisplayPrefs();
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayPowerState));
}

TEST_F(DisplayPreferencesTest, DisplayPowerStateAfterRestart) {
  StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);
  LoadDisplayPreferences(false);
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            ash::Shell::GetInstance()->display_configurator()->power_state());
}

TEST_F(DisplayPreferencesTest, DontSaveAndRestoreAllOff) {
  ash::Shell* shell = ash::Shell::GetInstance();
  StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);
  LoadDisplayPreferences(false);
  // DisplayPowerState should be ignored at boot.
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            shell->display_configurator()->power_state());

  StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_ALL_OFF);
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            shell->display_configurator()->power_state());
  EXPECT_EQ("internal_off_external_on",
            local_state()->GetString(prefs::kDisplayPowerState));

  // Don't try to load
  local_state()->SetString(prefs::kDisplayPowerState, "all_off");
  LoadDisplayPreferences(false);
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            shell->display_configurator()->power_state());
}

// Tests that display configuration changes caused by MaximizeModeController
// are not saved.
TEST_F(DisplayPreferencesTest, DontSaveMaximizeModeControllerRotations) {
  ash::Shell* shell = ash::Shell::GetInstance();
  ash::MaximizeModeController* controller = shell->maximize_mode_controller();
  gfx::Display::SetInternalDisplayId(
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id());
  ash::DisplayManager* display_manager = shell->display_manager();
  LoggedInAsUser();
  // Populate the properties.
  display_manager->SetDisplayRotation(gfx::Display::InternalDisplayId(),
                                      gfx::Display::ROTATE_180);
  // Reset property to avoid rotation lock
  display_manager->SetDisplayRotation(gfx::Display::InternalDisplayId(),
                                      gfx::Display::ROTATE_0);

  // Open up 270 degrees to trigger maximize mode
  controller->OnAccelerometerUpdated(gfx::Vector3dF(0.0f, 0.0f, -1.0f),
                                     gfx::Vector3dF(-1.0f, 0.0f, 0.0f));
  EXPECT_TRUE(controller->IsMaximizeModeWindowManagerEnabled());

  // Trigger 90 degree rotation
  controller->OnAccelerometerUpdated(gfx::Vector3dF(0.0f, 1.0f, 0.0f),
                                     gfx::Vector3dF(0.0f, 1.0f, 0.0f));
  EXPECT_EQ(gfx::Display::ROTATE_90, display_manager->
                GetDisplayInfo(gfx::Display::InternalDisplayId()).rotation());

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayProperties);
  const base::DictionaryValue* property = NULL;
  EXPECT_TRUE(properties->GetDictionary(
      base::Int64ToString(gfx::Display::InternalDisplayId()), &property));
  int rotation = -1;
  EXPECT_TRUE(property->GetInteger("rotation", &rotation));
  EXPECT_EQ(gfx::Display::ROTATE_0, rotation);
}

}  // namespace chromeos
