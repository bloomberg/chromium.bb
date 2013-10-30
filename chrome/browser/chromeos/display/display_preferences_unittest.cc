// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/display_preferences.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_layout_store.h"
#include "ash/display/display_manager.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/prefs/testing_pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/display/display_configuration_observer.h"
#include "chrome/browser/chromeos/login/mock_user_manager.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/display/output_configurator.h"
#include "ui/message_center/message_center.h"

using ash::internal::ResolutionNotificationController;

namespace chromeos {
namespace {
const char kPrimaryIdKey[] = "primary-id";
const char kMirroredKey[] = "mirrored";
const char kPositionKey[] = "position";
const char kOffsetKey[] = "offset";

class DisplayPreferencesTest : public ash::test::AshTestBase {
 protected:
  DisplayPreferencesTest() : ash::test::AshTestBase(),
                             mock_user_manager_(new MockUserManager),
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
    EXPECT_CALL(*mock_user_manager_, IsLoggedInAsLocallyManagedUser())
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

  std::string GetRegisteredDisplayLayoutStr(int64 id1, int64 id2) {
    ash::DisplayIdPair pair;
    pair.first = id1;
    pair.second = id2;
    return ash::Shell::GetInstance()->display_manager()->layout_store()->
        GetRegisteredDisplayLayout(pair).ToString();
  }

  const PrefService* local_state() const { return &local_state_; }

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
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
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
            shell->output_configurator()->power_state());

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
  ash::internal::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();

  UpdateDisplay("200x200*2, 400x300#400x400|300x200");
  int64 id1 = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().id();
  gfx::Display::SetInternalDisplayId(id1);
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  int64 dummy_id = id2 + 1;
  ASSERT_NE(id1, dummy_id);

  LoggedInAsUser();
  ash::DisplayLayout layout(ash::DisplayLayout::TOP, 10);
  SetCurrentDisplayLayout(layout);
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

  // Internal display never registere the resolution.
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
  // Internal dispaly shouldn't store its resolution.
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  // External dispaly's resolution must be stored this time because
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

  // External dispaly's selected resolution must not change
  // by mirroring.
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id2), &property));
  EXPECT_TRUE(property->GetInteger("width", &width));
  EXPECT_TRUE(property->GetInteger("height", &height));
  EXPECT_EQ(300, width);
  EXPECT_EQ(200, height);

  // Set new display's selected resolution.
  display_manager->RegisterDisplayProperty(id2 + 1,
                                           gfx::Display::ROTATE_0,
                                           1.0f,
                                           NULL,
                                           gfx::Size(500, 400));

  UpdateDisplay("200x200*2, 600x500#600x500|500x400");

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

  // External dispaly's selected resolution must be updated.
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
  EXPECT_FALSE(message_center::MessageCenter::Get()->HasNotification(
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
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();

  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  display_controller->SwapPrimaryDisplay();
  ASSERT_EQ(id1, ash::ScreenAsh::GetSecondaryDisplay().id());

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

TEST_F(DisplayPreferencesTest, DontStoreInGuestMode) {
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();
  ash::internal::DisplayManager* display_manager =
      ash::Shell::GetInstance()->display_manager();

  UpdateDisplay("200x200*2,200x200");

  LoggedInAsGuest();
  int64 id1 = ash::ScreenAsh::GetNativeScreen()->GetPrimaryDisplay().id();
  gfx::Display::SetInternalDisplayId(id1);
  int64 id2 = ash::ScreenAsh::GetSecondaryDisplay().id();
  ash::DisplayLayout layout(ash::DisplayLayout::TOP, 10);
  SetCurrentDisplayLayout(layout);
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

  const ash::internal::DisplayInfo& info1 =
      display_manager->GetDisplayInfo(id1);
  EXPECT_EQ(1.25f, info1.ui_scale());

  const ash::internal::DisplayInfo& info_primary =
      display_manager->GetDisplayInfo(new_primary);
  EXPECT_EQ(gfx::Display::ROTATE_90, info_primary.rotation());
  EXPECT_EQ(1.0f, info_primary.ui_scale());
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
  EXPECT_EQ(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
      ash::Shell::GetInstance()->output_configurator()->power_state());
}

}  // namespace chromeos
