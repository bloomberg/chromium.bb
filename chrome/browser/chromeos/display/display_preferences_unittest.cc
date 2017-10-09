// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/display_preferences.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "ash/display/display_util.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/display/screen_orientation_controller_chromeos.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/display/display_configuration_observer.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/chromeos/display_configurator.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_utilities.h"
#include "ui/display/manager/json_converter.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/message_center/message_center.h"

using ash::ResolutionNotificationController;

namespace chromeos {
namespace {
const char kPrimaryIdKey[] = "primary-id";
const char kMirroredKey[] = "mirrored";
const char kPositionKey[] = "position";
const char kOffsetKey[] = "offset";
const char kPlacementDisplayIdKey[] = "placement.display_id";
const char kPlacementParentDisplayIdKey[] = "placement.parent_display_id";
const char kTouchCalibrationMap[] = "touch_calibration_map";
const char kTouchCalibrationWidth[] = "touch_calibration_width";
const char kTouchCalibrationHeight[] = "touch_calibration_height";
const char kTouchCalibrationPointPairs[] = "touch_calibration_point_pairs";

// The mean acceleration due to gravity on Earth in m/s^2.
const float kMeanGravity = -9.80665f;

bool IsRotationLocked() {
  return ash::Shell::Get()->screen_orientation_controller()->rotation_locked();
}

class DisplayPreferencesTest : public ash::AshTestBase {
 protected:
  DisplayPreferencesTest()
      : mock_user_manager_(new MockUserManager),
        user_manager_enabler_(mock_user_manager_) {
  }

  ~DisplayPreferencesTest() override {}

  void SetUp() override {
    EXPECT_CALL(*mock_user_manager_, IsUserLoggedIn())
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(*mock_user_manager_, Shutdown());
    ash::AshTestBase::SetUp();
    RegisterDisplayLocalStatePrefs(local_state_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
    observer_ = std::make_unique<DisplayConfigurationObserver>();
    observer_->OnDisplaysInitialized();
  }

  void TearDown() override {
    observer_.reset();
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    ash::AshTestBase::TearDown();
  }

  void LoggedInAsUser() {
    EXPECT_CALL(*mock_user_manager_, IsUserLoggedIn())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_user_manager_, IsLoggedInAsUserWithGaiaAccount())
        .WillRepeatedly(testing::Return(true));
  }

  void LoggedInAsGuest() {
    EXPECT_CALL(*mock_user_manager_, IsUserLoggedIn())
        .WillRepeatedly(testing::Return(true));
    EXPECT_CALL(*mock_user_manager_, IsLoggedInAsUserWithGaiaAccount())
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(*mock_user_manager_, IsLoggedInAsSupervisedUser())
        .WillRepeatedly(testing::Return(false));
  }

  // Do not use the implementation of display_preferences.cc directly to avoid
  // notifying the update to the system.
  void StoreDisplayLayoutPrefForList(
      const display::DisplayIdList& list,
      display::DisplayPlacement::Position position,
      int offset,
      int64_t primary_id) {
    std::string name = display::DisplayIdListToString(list);
    DictionaryPrefUpdate update(&local_state_, prefs::kSecondaryDisplays);
    display::DisplayLayout display_layout;
    display_layout.placement_list.emplace_back(position, offset);
    display_layout.primary_id = primary_id;

    DCHECK(!name.empty());

    base::DictionaryValue* pref_data = update.Get();
    std::unique_ptr<base::Value> layout_value(new base::DictionaryValue());
    base::Value* value = nullptr;
    if (pref_data->Get(name, &value) && value != nullptr)
      layout_value.reset(value->DeepCopy());
    if (display::DisplayLayoutToJson(display_layout, layout_value.get()))
      pref_data->Set(name, std::move(layout_value));
  }

  bool GetDisplayPropertyFromList(const display::DisplayIdList& list,
                                  const std::string& key,
                                  base::Value** out_value) {
    std::string name = display::DisplayIdListToString(list);

    DictionaryPrefUpdate update(&local_state_, prefs::kSecondaryDisplays);
    base::DictionaryValue* pref_data = update.Get();

    base::Value* layout_value = pref_data->FindPath({name});
    if (layout_value) {
      return static_cast<base::DictionaryValue*>(layout_value)
          ->Get(key, out_value);
    }
    return false;
  }

  void StoreDisplayPropertyForList(const display::DisplayIdList& list,
                                   const std::string& key,
                                   std::unique_ptr<base::Value> value) {
    std::string name = display::DisplayIdListToString(list);

    DictionaryPrefUpdate update(&local_state_, prefs::kSecondaryDisplays);
    base::DictionaryValue* pref_data = update.Get();

    base::Value* layout_value = pref_data->FindPath({name});
    if (layout_value) {
      static_cast<base::DictionaryValue*>(layout_value)
          ->Set(key, std::move(value));
    } else {
      std::unique_ptr<base::DictionaryValue> layout_value(
          new base::DictionaryValue());
      layout_value->SetBoolean(key, value != nullptr);
      pref_data->Set(name, std::move(layout_value));
    }
  }

  void StoreDisplayBoolPropertyForList(const display::DisplayIdList& list,
                                       const std::string& key,
                                       bool value) {
    StoreDisplayPropertyForList(list, key,
                                base::MakeUnique<base::Value>(value));
  }

  void StoreDisplayLayoutPrefForList(const display::DisplayIdList& list,
                                     display::DisplayPlacement::Position layout,
                                     int offset) {
    StoreDisplayLayoutPrefForList(list, layout, offset, list[0]);
  }

  void StoreDisplayOverscan(int64_t id, const gfx::Insets& insets) {
    DictionaryPrefUpdate update(&local_state_, prefs::kDisplayProperties);
    const std::string name = base::Int64ToString(id);

    base::DictionaryValue* pref_data = update.Get();
    auto insets_value = base::MakeUnique<base::DictionaryValue>();
    insets_value->SetInteger("insets_top", insets.top());
    insets_value->SetInteger("insets_left", insets.left());
    insets_value->SetInteger("insets_bottom", insets.bottom());
    insets_value->SetInteger("insets_right", insets.right());
    pref_data->Set(name, std::move(insets_value));
  }

  void StoreDisplayRotationPrefsForTest(bool rotation_lock,
                                        display::Display::Rotation rotation) {
    DictionaryPrefUpdate update(local_state(), prefs::kDisplayRotationLock);
    base::DictionaryValue* pref_data = update.Get();
    pref_data->SetBoolean("lock", rotation_lock);
    pref_data->SetInteger("orientation", static_cast<int>(rotation));
  }

  std::string GetRegisteredDisplayPlacementStr(
      const display::DisplayIdList& list) {
    return ash::Shell::Get()
        ->display_manager()
        ->layout_store()
        ->GetRegisteredDisplayLayout(list)
        .placement_list[0]
        .ToString();
  }

  PrefService* local_state() { return &local_state_; }

 private:
  MockUserManager* mock_user_manager_;  // Not owned.
  ScopedUserManagerEnabler user_manager_enabler_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<ash::WindowTreeHostManager::Observer> observer_;

  DISALLOW_COPY_AND_ASSIGN(DisplayPreferencesTest);
};

}  // namespace

TEST_F(DisplayPreferencesTest, ListedLayoutOverrides) {
  UpdateDisplay("100x100,200x200");

  display::DisplayIdList list = display_manager()->GetCurrentDisplayIdList();
  display::DisplayIdList dummy_list =
      display::test::CreateDisplayIdList2(list[0], list[1] + 1);
  ASSERT_NE(list[0], dummy_list[1]);

  StoreDisplayLayoutPrefForList(list, display::DisplayPlacement::TOP, 20);
  StoreDisplayLayoutPrefForList(dummy_list, display::DisplayPlacement::LEFT,
                                30);
  StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);

  ash::Shell* shell = ash::Shell::Get();

  LoadDisplayPreferences(true);
  // DisplayPowerState should be ignored at boot.
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON,
            shell->display_configurator()->requested_power_state());

  shell->display_manager()->UpdateDisplays();
  // Check if the layout settings are notified to the system properly.
  // The new layout overrides old layout.
  // Inverted one of for specified pair (id1, id2).  Not used for the list
  // (id1, dummy_id) since dummy_id is not connected right now.
  EXPECT_EQ("id=2200000001, parent=2200000000, top, 20",
            shell->display_manager()
                ->GetCurrentDisplayLayout()
                .placement_list[0]
                .ToString());
  EXPECT_EQ("id=2200000001, parent=2200000000, top, 20",
            GetRegisteredDisplayPlacementStr(list));
  EXPECT_EQ("id=2200000002, parent=2200000000, left, 30",
            GetRegisteredDisplayPlacementStr(dummy_list));
}

TEST_F(DisplayPreferencesTest, BasicStores) {
  ash::WindowTreeHostManager* window_tree_host_manager =
      ash::Shell::Get()->window_tree_host_manager();

  UpdateDisplay("200x200*2, 400x300#400x400|300x200*1.25");
  int64_t id1 = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         id1);
  int64_t id2 = display_manager()->GetSecondaryDisplay().id();
  int64_t dummy_id = id2 + 1;
  ASSERT_NE(id1, dummy_id);

  LoggedInAsUser();

  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::TOP, 10));
  const display::DisplayLayout& layout =
      display_manager()->GetCurrentDisplayLayout();
  EXPECT_EQ(display::DisplayPlacement::TOP, layout.placement_list[0].position);
  EXPECT_EQ(10, layout.placement_list[0].offset);

  display::DisplayLayoutBuilder dummy_layout_builder(id1);
  dummy_layout_builder.SetSecondaryPlacement(
      dummy_id, display::DisplayPlacement::LEFT, 20);
  std::unique_ptr<display::DisplayLayout> dummy_layout(
      dummy_layout_builder.Build());
  display::DisplayIdList list =
      display::test::CreateDisplayIdList2(id1, dummy_id);
  StoreDisplayLayoutPrefForTest(list, *dummy_layout);

  // Can't switch to a display that does not exist.
  window_tree_host_manager->SetPrimaryDisplayId(dummy_id);
  EXPECT_NE(dummy_id, display::Screen::GetScreen()->GetPrimaryDisplay().id());

  window_tree_host_manager->SetOverscanInsets(id1, gfx::Insets(10, 11, 12, 13));
  display_manager()->SetDisplayRotation(id1, display::Display::ROTATE_90,
                                        display::Display::ROTATION_SOURCE_USER);
  EXPECT_TRUE(display::test::DisplayManagerTestApi(display_manager())
                  .SetDisplayUIScale(id1, 1.25f));
  EXPECT_FALSE(display::test::DisplayManagerTestApi(display_manager())
                   .SetDisplayUIScale(id2, 1.25f));

  // Set touch calibration data for display |id2|.
  constexpr uint32_t touch_device_identifier_1 = 1234;
  display::TouchCalibrationData::CalibrationPointPairQuad point_pair_quad_1 = {
      {std::make_pair(gfx::Point(10, 10), gfx::Point(11, 12)),
       std::make_pair(gfx::Point(190, 10), gfx::Point(195, 8)),
       std::make_pair(gfx::Point(10, 90), gfx::Point(12, 94)),
       std::make_pair(gfx::Point(190, 90), gfx::Point(189, 88))}};
  gfx::Size touch_size_1(200, 150);

  constexpr uint32_t touch_device_identifier_2 = 2345;
  display::TouchCalibrationData::CalibrationPointPairQuad point_pair_quad_2 = {
      {std::make_pair(gfx::Point(10, 10), gfx::Point(11, 12)),
       std::make_pair(gfx::Point(190, 10), gfx::Point(195, 8)),
       std::make_pair(gfx::Point(10, 90), gfx::Point(12, 94)),
       std::make_pair(gfx::Point(190, 90), gfx::Point(189, 88))}};
  gfx::Size touch_size_2(150, 150);

  display_manager()->SetTouchCalibrationData(
      id2, point_pair_quad_1, touch_size_1, touch_device_identifier_1);
  display_manager()->SetTouchCalibrationData(
      id2, point_pair_quad_2, touch_size_2, touch_device_identifier_2);

  const base::DictionaryValue* displays =
      local_state()->GetDictionary(prefs::kSecondaryDisplays);
  const base::DictionaryValue* layout_value = nullptr;
  std::string key = base::Int64ToString(id1) + "," + base::Int64ToString(id2);
  std::string dummy_key =
      base::Int64ToString(id1) + "," + base::Int64ToString(dummy_id);
  EXPECT_TRUE(displays->GetDictionary(dummy_key, &layout_value));

  display::DisplayLayout stored_layout;
  EXPECT_TRUE(display::JsonToDisplayLayout(*layout_value, &stored_layout));
  ASSERT_EQ(1u, stored_layout.placement_list.size());

  EXPECT_EQ(dummy_layout->placement_list[0].position,
            stored_layout.placement_list[0].position);
  EXPECT_EQ(dummy_layout->placement_list[0].offset,
            stored_layout.placement_list[0].offset);

  bool mirrored = true;
  EXPECT_TRUE(layout_value->GetBoolean(kMirroredKey, &mirrored));
  EXPECT_FALSE(mirrored);

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayProperties);
  const base::DictionaryValue* property = nullptr;
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

  std::string touch_str;
  const base::DictionaryValue* map_dictionary;
  const base::DictionaryValue* data_dict;
  EXPECT_FALSE(property->GetDictionary(kTouchCalibrationMap, &map_dictionary));

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

  display::TouchCalibrationData::CalibrationPointPairQuad stored_pair_quad;

  EXPECT_TRUE(property->GetDictionary(kTouchCalibrationMap, &map_dictionary));
  EXPECT_TRUE(map_dictionary->GetDictionary(
      base::UintToString(touch_device_identifier_1), &data_dict));
  EXPECT_TRUE(data_dict->GetString(kTouchCalibrationPointPairs, &touch_str));
  EXPECT_TRUE(ParseTouchCalibrationStringForTest(touch_str, &stored_pair_quad));

  for (std::size_t row = 0; row < point_pair_quad_1.size(); row++) {
    EXPECT_EQ(point_pair_quad_1[row].first.x(),
              stored_pair_quad[row].first.x());
    EXPECT_EQ(point_pair_quad_1[row].first.y(),
              stored_pair_quad[row].first.y());
    EXPECT_EQ(point_pair_quad_1[row].second.x(),
              stored_pair_quad[row].second.x());
    EXPECT_EQ(point_pair_quad_1[row].second.y(),
              stored_pair_quad[row].second.y());
  }
  width = height = 0;
  EXPECT_TRUE(data_dict->GetInteger(kTouchCalibrationWidth, &width));
  EXPECT_TRUE(data_dict->GetInteger(kTouchCalibrationHeight, &height));
  EXPECT_EQ(width, touch_size_1.width());
  EXPECT_EQ(height, touch_size_1.height());

  EXPECT_TRUE(map_dictionary->GetDictionary(
      base::UintToString(touch_device_identifier_2), &data_dict));
  EXPECT_TRUE(data_dict->GetString(kTouchCalibrationPointPairs, &touch_str));
  EXPECT_TRUE(ParseTouchCalibrationStringForTest(touch_str, &stored_pair_quad));

  for (std::size_t row = 0; row < point_pair_quad_2.size(); row++) {
    EXPECT_EQ(point_pair_quad_2[row].first.x(),
              stored_pair_quad[row].first.x());
    EXPECT_EQ(point_pair_quad_2[row].first.y(),
              stored_pair_quad[row].first.y());
    EXPECT_EQ(point_pair_quad_2[row].second.x(),
              stored_pair_quad[row].second.x());
    EXPECT_EQ(point_pair_quad_2[row].second.y(),
              stored_pair_quad[row].second.y());
  }
  width = height = 0;
  EXPECT_TRUE(data_dict->GetInteger(kTouchCalibrationWidth, &width));
  EXPECT_TRUE(data_dict->GetInteger(kTouchCalibrationHeight, &height));
  EXPECT_EQ(width, touch_size_2.width());
  EXPECT_EQ(height, touch_size_2.height());

  // Resolution is saved only when the resolution is set
  // by DisplayManager::SetDisplayMode
  width = 0;
  height = 0;
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  scoped_refptr<display::ManagedDisplayMode> mode(
      new display::ManagedDisplayMode(gfx::Size(300, 200), 60.0f, false, true,
                                      1.0 /* ui_scale */,
                                      1.25f /* device_scale_factor */));
  display_manager()->SetDisplayMode(id2, mode);

  window_tree_host_manager->SetPrimaryDisplayId(id2);

  EXPECT_EQ(id2, display::Screen::GetScreen()->GetPrimaryDisplay().id());

  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id1), &property));
  width = 0;
  height = 0;
  // Internal display shouldn't store its resolution.
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  // External display's resolution must be stored this time because
  // it's not best.
  int device_scale_factor = 0;
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id2), &property));
  EXPECT_TRUE(property->GetInteger("width", &width));
  EXPECT_TRUE(property->GetInteger("height", &height));
  EXPECT_TRUE(property->GetInteger(
      "device-scale-factor", &device_scale_factor));
  EXPECT_EQ(300, width);
  EXPECT_EQ(200, height);
  EXPECT_EQ(1250, device_scale_factor);

  // The layout is swapped.
  EXPECT_TRUE(displays->GetDictionary(key, &layout_value));

  EXPECT_TRUE(display::JsonToDisplayLayout(*layout_value, &stored_layout));
  ASSERT_EQ(1u, stored_layout.placement_list.size());
  const display::DisplayPlacement& stored_placement =
      stored_layout.placement_list[0];
  EXPECT_EQ(display::DisplayPlacement::BOTTOM, stored_placement.position);
  EXPECT_EQ(-10, stored_placement.offset);
  EXPECT_EQ(id1, stored_placement.display_id);
  EXPECT_EQ(id2, stored_placement.parent_display_id);
  EXPECT_EQ(id2, stored_layout.primary_id);

  if (true)
    return;

  mirrored = true;
  EXPECT_TRUE(layout_value->GetBoolean(kMirroredKey, &mirrored));
  EXPECT_FALSE(mirrored);
  std::string primary_id_str;
  EXPECT_TRUE(layout_value->GetString(kPrimaryIdKey, &primary_id_str));
  EXPECT_EQ(base::Int64ToString(id2), primary_id_str);

  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(ash::Shell::Get()->display_manager(),
                                         display::DisplayPlacement::BOTTOM,
                                         20));

  UpdateDisplay("1+0-200x200*2,1+0-200x200");
  // Mirrored.
  int offset = 0;
  std::string position;
  EXPECT_TRUE(displays->GetDictionary(key, &layout_value));
  EXPECT_TRUE(layout_value->GetString(kPositionKey, &position));
  EXPECT_EQ("bottom", position);
  EXPECT_TRUE(layout_value->GetInteger(kOffsetKey, &offset));
  EXPECT_EQ(20, offset);
  std::string id;
  EXPECT_TRUE(layout_value->GetString(kPlacementDisplayIdKey, &id));
  EXPECT_EQ(base::Int64ToString(id1), id);
  EXPECT_TRUE(layout_value->GetString(kPlacementParentDisplayIdKey, &id));
  EXPECT_EQ(base::Int64ToString(id2), id);

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
  display_manager()->RegisterDisplayProperty(
      id2 + 1, display::Display::ROTATE_0, 1.0f, nullptr, gfx::Size(500, 400),
      1.0f, nullptr /* touch_calibration_data_map */);

  UpdateDisplay("200x200*2, 600x500#600x500|500x400");

  // Update key as the 2nd display gets new id.
  id2 = display_manager()->GetSecondaryDisplay().id();
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
  display_manager()->RegisterDisplayProperty(
      id2 + 1, display::Display::ROTATE_0, 1.0f, nullptr, gfx::Size(500, 400),
      1.0f, nullptr /* touch_calibration_data_map */);
  // Disconnect 2nd display first to generate new id for external display.
  UpdateDisplay("200x200*2");
  UpdateDisplay("200x200*2, 500x400#600x500|500x400%60.0f");
  // Update key as the 2nd display gets new id.
  id2 = display_manager()->GetSecondaryDisplay().id();
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
  int64_t id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  // Set display's resolution in single display. It creates the notification and
  // display preferences should not stored meanwhile.
  ash::Shell* shell = ash::Shell::Get();

  scoped_refptr<display::ManagedDisplayMode> old_mode(
      new display::ManagedDisplayMode(gfx::Size(400, 300)));
  scoped_refptr<display::ManagedDisplayMode> new_mode(
      new display::ManagedDisplayMode(gfx::Size(500, 400)));
  EXPECT_TRUE(shell->resolution_notification_controller()
                  ->PrepareNotificationAndSetDisplayMode(id, old_mode, new_mode,
                                                         base::Closure()));
  UpdateDisplay("500x400#500x400|400x300|300x200");

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayProperties);
  const base::DictionaryValue* property = nullptr;
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
  // by SetDisplayMode.
  ash::Shell::Get()->display_manager()->SetDisplayMode(
      id, base::MakeRefCounted<display::ManagedDisplayMode>(
              gfx::Size(300, 200), 60.0f, false, true));
  UpdateDisplay("300x200#500x400|400x300|300x200");

  property = nullptr;
  EXPECT_TRUE(properties->GetDictionary(base::Int64ToString(id), &property));
  EXPECT_TRUE(property->GetInteger("width", &width));
  EXPECT_TRUE(property->GetInteger("height", &height));
  EXPECT_EQ(300, width);
  EXPECT_EQ(200, height);
}

TEST_F(DisplayPreferencesTest, StoreForSwappedDisplay) {
  UpdateDisplay("100x100,200x200");
  int64_t id1 = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  int64_t id2 = display_manager()->GetSecondaryDisplay().id();

  LoggedInAsUser();

  SwapPrimaryDisplay();
  ASSERT_EQ(id1, display_manager()->GetSecondaryDisplay().id());

  std::string key = base::Int64ToString(id1) + "," + base::Int64ToString(id2);
  const base::DictionaryValue* displays =
      local_state()->GetDictionary(prefs::kSecondaryDisplays);
  // Initial saved value is swapped.
  {
    const base::DictionaryValue* new_value = nullptr;
    EXPECT_TRUE(displays->GetDictionary(key, &new_value));
    display::DisplayLayout stored_layout;
    EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
    ASSERT_EQ(1u, stored_layout.placement_list.size());
    const display::DisplayPlacement& stored_placement =
        stored_layout.placement_list[0];
    EXPECT_EQ(display::DisplayPlacement::LEFT, stored_placement.position);
    EXPECT_EQ(0, stored_placement.offset);
    EXPECT_EQ(id1, stored_placement.display_id);
    EXPECT_EQ(id2, stored_placement.parent_display_id);
    EXPECT_EQ(id2, stored_layout.primary_id);
  }

  // Updating layout with primary swapped should save the correct value.
  {
    display_manager()->SetLayoutForCurrentDisplays(
        display::test::CreateDisplayLayout(display_manager(),
                                           display::DisplayPlacement::TOP, 10));
    const base::DictionaryValue* new_value = nullptr;
    EXPECT_TRUE(displays->GetDictionary(key, &new_value));
    display::DisplayLayout stored_layout;
    EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
    ASSERT_EQ(1u, stored_layout.placement_list.size());
    const display::DisplayPlacement& stored_placement =
        stored_layout.placement_list[0];
    EXPECT_EQ(display::DisplayPlacement::TOP, stored_placement.position);
    EXPECT_EQ(10, stored_placement.offset);
    EXPECT_EQ(id1, stored_placement.display_id);
    EXPECT_EQ(id2, stored_placement.parent_display_id);
    EXPECT_EQ(id2, stored_layout.primary_id);
  }

  // Swapping primary will save the swapped value.
  {
    SwapPrimaryDisplay();
    const base::DictionaryValue* new_value = nullptr;
    EXPECT_TRUE(displays->GetDictionary(key, &new_value));
    display::DisplayLayout stored_layout;

    EXPECT_TRUE(displays->GetDictionary(key, &new_value));
    EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
    ASSERT_EQ(1u, stored_layout.placement_list.size());
    const display::DisplayPlacement& stored_placement =
        stored_layout.placement_list[0];
    EXPECT_EQ(display::DisplayPlacement::BOTTOM, stored_placement.position);
    EXPECT_EQ(-10, stored_placement.offset);
    EXPECT_EQ(id2, stored_placement.display_id);
    EXPECT_EQ(id1, stored_placement.parent_display_id);
    EXPECT_EQ(id1, stored_layout.primary_id);
  }
}

TEST_F(DisplayPreferencesTest, DontStoreInGuestMode) {
  ash::WindowTreeHostManager* window_tree_host_manager =
      ash::Shell::Get()->window_tree_host_manager();

  UpdateDisplay("200x200*2,200x200");

  LoggedInAsGuest();
  int64_t id1 = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::test::ScopedSetInternalDisplayId set_internal(
      ash::Shell::Get()->display_manager(), id1);
  int64_t id2 = display_manager()->GetSecondaryDisplay().id();
  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::TOP, 10));
  display::test::DisplayManagerTestApi(display_manager())
      .SetDisplayUIScale(id1, 1.25f);
  window_tree_host_manager->SetPrimaryDisplayId(id2);
  int64_t new_primary = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  window_tree_host_manager->SetOverscanInsets(new_primary,
                                              gfx::Insets(10, 11, 12, 13));
  display_manager()->SetDisplayRotation(new_primary,
                                        display::Display::ROTATE_90,
                                        display::Display::ROTATION_SOURCE_USER);

  // Does not store the preferences locally.
  EXPECT_FALSE(local_state()->FindPreference(
      prefs::kSecondaryDisplays)->HasUserSetting());
  EXPECT_FALSE(local_state()->FindPreference(
      prefs::kDisplayProperties)->HasUserSetting());

  // Settings are still notified to the system.
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(id2, screen->GetPrimaryDisplay().id());
  const display::DisplayPlacement& placement =
      display_manager()->GetCurrentDisplayLayout().placement_list[0];
  EXPECT_EQ(display::DisplayPlacement::BOTTOM, placement.position);
  EXPECT_EQ(-10, placement.offset);
  const display::Display& primary_display = screen->GetPrimaryDisplay();
  EXPECT_EQ("178x176", primary_display.bounds().size().ToString());
  EXPECT_EQ(display::Display::ROTATE_90, primary_display.rotation());

  const display::ManagedDisplayInfo& info1 =
      display_manager()->GetDisplayInfo(id1);
  EXPECT_EQ(1.25f, info1.configured_ui_scale());

  const display::ManagedDisplayInfo& info_primary =
      display_manager()->GetDisplayInfo(new_primary);
  EXPECT_EQ(display::Display::ROTATE_90, info_primary.GetActiveRotation());
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
            ash::Shell::Get()->display_configurator()->requested_power_state());
}

TEST_F(DisplayPreferencesTest, DontSaveAndRestoreAllOff) {
  ash::Shell* shell = ash::Shell::Get();
  StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);
  LoadDisplayPreferences(false);
  // DisplayPowerState should be ignored at boot.
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            shell->display_configurator()->requested_power_state());

  StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_ALL_OFF);
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            shell->display_configurator()->requested_power_state());
  EXPECT_EQ("internal_off_external_on",
            local_state()->GetString(prefs::kDisplayPowerState));

  // Don't try to load
  local_state()->SetString(prefs::kDisplayPowerState, "all_off");
  LoadDisplayPreferences(false);
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            shell->display_configurator()->requested_power_state());
}

// Tests that display configuration changes caused by TabletModeController
// are not saved.
TEST_F(DisplayPreferencesTest, DontSaveTabletModeControllerRotations) {
  ash::Shell* shell = ash::Shell::Get();
  display::Display::SetInternalDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  LoggedInAsUser();
  // Populate the properties.
  display_manager()->SetDisplayRotation(display::Display::InternalDisplayId(),
                                        display::Display::ROTATE_180,
                                        display::Display::ROTATION_SOURCE_USER);
  // Reset property to avoid rotation lock
  display_manager()->SetDisplayRotation(display::Display::InternalDisplayId(),
                                        display::Display::ROTATE_0,
                                        display::Display::ROTATION_SOURCE_USER);

  // Open up 270 degrees to trigger tablet mode
  scoped_refptr<chromeos::AccelerometerUpdate> update(
      new chromeos::AccelerometerUpdate());
  update->Set(chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD, 0.0f, 0.0f,
             kMeanGravity);
  update->Set(chromeos::ACCELEROMETER_SOURCE_SCREEN, 0.0f, -kMeanGravity, 0.0f);
  ash::TabletModeController* controller =
      ash::Shell::Get()->tablet_mode_controller();
  controller->OnAccelerometerUpdated(update);
  EXPECT_TRUE(controller->IsTabletModeWindowManagerEnabled());

  // Trigger 90 degree rotation
  update->Set(chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD, -kMeanGravity,
             0.0f, 0.0f);
  update->Set(chromeos::ACCELEROMETER_SOURCE_SCREEN, -kMeanGravity, 0.0f, 0.0f);
  controller->OnAccelerometerUpdated(update);
  shell->screen_orientation_controller()->OnAccelerometerUpdated(update);
  EXPECT_EQ(display::Display::ROTATE_90, GetCurrentInternalDisplayRotation());

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayProperties);
  const base::DictionaryValue* property = nullptr;
  EXPECT_TRUE(properties->GetDictionary(
      base::Int64ToString(display::Display::InternalDisplayId()), &property));
  int rotation = -1;
  EXPECT_TRUE(property->GetInteger("rotation", &rotation));
  EXPECT_EQ(display::Display::ROTATE_0, rotation);

  // Trigger a save, the acceleration rotation should not be saved as the user
  // rotation.
  StoreDisplayPrefs();
  properties = local_state()->GetDictionary(prefs::kDisplayProperties);
  property = nullptr;
  EXPECT_TRUE(properties->GetDictionary(
      base::Int64ToString(display::Display::InternalDisplayId()), &property));
  rotation = -1;
  EXPECT_TRUE(property->GetInteger("rotation", &rotation));
  EXPECT_EQ(display::Display::ROTATE_0, rotation);
}

// Tests that the rotation state is saved without a user being logged in.
TEST_F(DisplayPreferencesTest, StoreRotationStateNoLogin) {
  display::Display::SetInternalDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  bool current_rotation_lock = IsRotationLocked();
  StoreDisplayRotationPrefs(current_rotation_lock);
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayRotationLock);
  bool rotation_lock;
  EXPECT_TRUE(properties->GetBoolean("lock", &rotation_lock));
  EXPECT_EQ(current_rotation_lock, rotation_lock);

  int orientation;
  display::Display::Rotation current_rotation =
      GetCurrentInternalDisplayRotation();
  EXPECT_TRUE(properties->GetInteger("orientation", &orientation));
  EXPECT_EQ(current_rotation, orientation);
}

// Tests that the rotation state is saved when a guest is logged in.
TEST_F(DisplayPreferencesTest, StoreRotationStateGuest) {
  display::Display::SetInternalDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));
  LoggedInAsGuest();

  bool current_rotation_lock = IsRotationLocked();
  StoreDisplayRotationPrefs(current_rotation_lock);
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayRotationLock);
  bool rotation_lock;
  EXPECT_TRUE(properties->GetBoolean("lock", &rotation_lock));
  EXPECT_EQ(current_rotation_lock, rotation_lock);

  int orientation;
  display::Display::Rotation current_rotation =
      GetCurrentInternalDisplayRotation();
  EXPECT_TRUE(properties->GetInteger("orientation", &orientation));
  EXPECT_EQ(current_rotation, orientation);
}

// Tests that the rotation state is saved when a normal user is logged in.
TEST_F(DisplayPreferencesTest, StoreRotationStateNormalUser) {
  display::Display::SetInternalDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));
  LoggedInAsGuest();

  bool current_rotation_lock = IsRotationLocked();
  StoreDisplayRotationPrefs(current_rotation_lock);
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayRotationLock);
  bool rotation_lock;
  EXPECT_TRUE(properties->GetBoolean("lock", &rotation_lock));
  EXPECT_EQ(current_rotation_lock, rotation_lock);

  int orientation;
  display::Display::Rotation current_rotation =
      GetCurrentInternalDisplayRotation();
  EXPECT_TRUE(properties->GetInteger("orientation", &orientation));
  EXPECT_EQ(current_rotation, orientation);
}

// Tests that rotation state is loaded without a user being logged in, and that
// entering tablet mode applies the state.
TEST_F(DisplayPreferencesTest, LoadRotationNoLogin) {
  display::Display::SetInternalDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  ASSERT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  bool initial_rotation_lock = IsRotationLocked();
  ASSERT_FALSE(initial_rotation_lock);
  display::Display::Rotation initial_rotation =
      GetCurrentInternalDisplayRotation();
  ASSERT_EQ(display::Display::ROTATE_0, initial_rotation);

  StoreDisplayRotationPrefs(initial_rotation_lock);
  ASSERT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  StoreDisplayRotationPrefsForTest(true, display::Display::ROTATE_90);
  LoadDisplayPreferences(false);

  bool display_rotation_lock =
      display_manager()->registered_internal_display_rotation_lock();
  bool display_rotation =
      display_manager()->registered_internal_display_rotation();
  EXPECT_TRUE(display_rotation_lock);
  EXPECT_EQ(display::Display::ROTATE_90, display_rotation);

  bool rotation_lock = IsRotationLocked();
  display::Display::Rotation before_tablet_mode_rotation =
      GetCurrentInternalDisplayRotation();

  // Settings should not be applied until tablet mode activates
  EXPECT_FALSE(rotation_lock);
  EXPECT_EQ(display::Display::ROTATE_0, before_tablet_mode_rotation);

  // Open up 270 degrees to trigger tablet mode
  scoped_refptr<chromeos::AccelerometerUpdate> update(
      new chromeos::AccelerometerUpdate());
  update->Set(chromeos::ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD, 0.0f, 0.0f,
             kMeanGravity);
  update->Set(chromeos::ACCELEROMETER_SOURCE_SCREEN, 0.0f, -kMeanGravity, 0.0f);
  ash::TabletModeController* tablet_mode_controller =
      ash::Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->OnAccelerometerUpdated(update);
  EXPECT_TRUE(tablet_mode_controller->IsTabletModeWindowManagerEnabled());
  bool screen_orientation_rotation_lock = IsRotationLocked();
  display::Display::Rotation tablet_mode_rotation =
      GetCurrentInternalDisplayRotation();
  EXPECT_TRUE(screen_orientation_rotation_lock);
  EXPECT_EQ(display::Display::ROTATE_90, tablet_mode_rotation);
}

// Tests that rotation lock being set causes the rotation state to be saved.
TEST_F(DisplayPreferencesTest, RotationLockTriggersStore) {
  display::Display::SetInternalDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  ASSERT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  ash::Shell::Get()->screen_orientation_controller()->ToggleUserRotationLock();

  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayRotationLock);
  bool rotation_lock;
  EXPECT_TRUE(properties->GetBoolean("lock", &rotation_lock));
}

TEST_F(DisplayPreferencesTest, SaveUnifiedMode) {
  LoggedInAsUser();
  display_manager()->SetUnifiedDesktopEnabled(true);

  UpdateDisplay("200x200,100x100");
  display::DisplayIdList list = display_manager()->GetCurrentDisplayIdList();
  EXPECT_EQ(
      "400x200",
      display::Screen::GetScreen()->GetPrimaryDisplay().size().ToString());

  const base::DictionaryValue* secondary_displays =
      local_state()->GetDictionary(prefs::kSecondaryDisplays);
  const base::DictionaryValue* new_value = nullptr;
  EXPECT_TRUE(secondary_displays->GetDictionary(
      display::DisplayIdListToString(list), &new_value));

  display::DisplayLayout stored_layout;
  EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
  EXPECT_TRUE(stored_layout.default_unified);
  EXPECT_FALSE(stored_layout.mirrored);

  const base::DictionaryValue* displays =
      local_state()->GetDictionary(prefs::kDisplayProperties);
  int64_t unified_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  EXPECT_FALSE(
      displays->GetDictionary(base::Int64ToString(unified_id), &new_value));

  display::test::SetDisplayResolution(display_manager(), unified_id,
                                      gfx::Size(200, 100));
  EXPECT_EQ(
      "200x100",
      display::Screen::GetScreen()->GetPrimaryDisplay().size().ToString());
  EXPECT_FALSE(
      displays->GetDictionary(base::Int64ToString(unified_id), &new_value));

  // Mirror mode should remember if the default mode was unified.
  display_manager()->SetMirrorMode(true);
  ASSERT_TRUE(secondary_displays->GetDictionary(
      display::DisplayIdListToString(list), &new_value));
  EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
  EXPECT_TRUE(stored_layout.default_unified);
  EXPECT_TRUE(stored_layout.mirrored);

  display_manager()->SetMirrorMode(false);
  ASSERT_TRUE(secondary_displays->GetDictionary(
      display::DisplayIdListToString(list), &new_value));
  EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
  EXPECT_TRUE(stored_layout.default_unified);
  EXPECT_FALSE(stored_layout.mirrored);

  // Exit unified mode.
  display_manager()->SetDefaultMultiDisplayModeForCurrentDisplays(
      display::DisplayManager::EXTENDED);
  ASSERT_TRUE(secondary_displays->GetDictionary(
      display::DisplayIdListToString(list), &new_value));
  EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
  EXPECT_FALSE(stored_layout.default_unified);
  EXPECT_FALSE(stored_layout.mirrored);
}

TEST_F(DisplayPreferencesTest, RestoreUnifiedMode) {
  int64_t id1 = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayIdList list =
      display::test::CreateDisplayIdList2(id1, id1 + 1);
  StoreDisplayBoolPropertyForList(list, "default_unified", true);
  StoreDisplayPropertyForList(
      list, "primary-id",
      base::MakeUnique<base::Value>(base::Int64ToString(id1)));
  LoadDisplayPreferences(false);

  // Should not restore to unified unless unified desktop is enabled.
  UpdateDisplay("100x100,200x200");
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());

  // Restored to unified.
  display_manager()->SetUnifiedDesktopEnabled(true);
  StoreDisplayBoolPropertyForList(list, "default_unified", true);
  LoadDisplayPreferences(false);
  UpdateDisplay("100x100,200x200");
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());

  // Restored to mirror, then unified.
  StoreDisplayBoolPropertyForList(list, "mirrored", true);
  StoreDisplayBoolPropertyForList(list, "default_unified", true);
  LoadDisplayPreferences(false);
  UpdateDisplay("100x100,200x200");
  EXPECT_TRUE(display_manager()->IsInMirrorMode());

  display_manager()->SetMirrorMode(false);
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());

  // Sanity check. Restore to extended.
  StoreDisplayBoolPropertyForList(list, "default_unified", false);
  StoreDisplayBoolPropertyForList(list, "mirrored", false);
  LoadDisplayPreferences(false);
  UpdateDisplay("100x100,200x200");
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());
}

TEST_F(DisplayPreferencesTest, SaveThreeDisplays) {
  LoggedInAsUser();
  UpdateDisplay("200x200,200x200,300x300");

  display::DisplayIdList list = display_manager()->GetCurrentDisplayIdList();
  ASSERT_EQ(3u, list.size());

  display::DisplayLayoutBuilder builder(list[0]);
  builder.AddDisplayPlacement(list[1], list[0],
                              display::DisplayPlacement::RIGHT, 0);
  builder.AddDisplayPlacement(list[2], list[0],
                              display::DisplayPlacement::BOTTOM, 100);
  display_manager()->SetLayoutForCurrentDisplays(builder.Build());

  const base::DictionaryValue* secondary_displays =
      local_state()->GetDictionary(prefs::kSecondaryDisplays);
  const base::DictionaryValue* new_value = nullptr;
  EXPECT_TRUE(secondary_displays->GetDictionary(
      display::DisplayIdListToString(list), &new_value));
}

TEST_F(DisplayPreferencesTest, RestoreThreeDisplays) {
  LoggedInAsUser();
  int64_t id1 = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayIdList list =
      display::test::CreateDisplayIdListN(3, id1, id1 + 1, id1 + 2);

  display::DisplayLayoutBuilder builder(list[0]);
  builder.AddDisplayPlacement(list[1], list[0], display::DisplayPlacement::LEFT,
                              0);
  builder.AddDisplayPlacement(list[2], list[1],
                              display::DisplayPlacement::BOTTOM, 100);
  StoreDisplayLayoutPrefForTest(list, *builder.Build());
  LoadDisplayPreferences(false);

  UpdateDisplay("200x200,200x200,300x300");
  display::DisplayIdList new_list =
      display_manager()->GetCurrentDisplayIdList();
  ASSERT_EQ(3u, list.size());
  ASSERT_EQ(list[0], new_list[0]);
  ASSERT_EQ(list[1], new_list[1]);
  ASSERT_EQ(list[2], new_list[2]);

  EXPECT_EQ(gfx::Rect(0, 0, 200, 200),
            display_manager()->GetDisplayForId(list[0]).bounds());
  EXPECT_EQ(gfx::Rect(-200, 0, 200, 200),
            display_manager()->GetDisplayForId(list[1]).bounds());
  EXPECT_EQ(gfx::Rect(-100, 200, 300, 300),
            display_manager()->GetDisplayForId(list[2]).bounds());
}

TEST_F(DisplayPreferencesTest, MirrorWhenEnterTableMode) {
  display::Display::SetInternalDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  LoggedInAsUser();
  UpdateDisplay("800x600,1200x800");
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  ash::TabletModeController* controller =
      ash::Shell::Get()->tablet_mode_controller();
  controller->EnableTabletModeWindowManager(true);
  ASSERT_TRUE(controller->IsTabletModeWindowManagerEnabled());
  EXPECT_TRUE(display_manager()->IsInMirrorMode());

  // Make sure the mirror mode is not saved in the preference.
  display::DisplayIdList list = display_manager()->GetCurrentDisplayIdList();
  ASSERT_EQ(2u, list.size());
  base::Value* value;
  EXPECT_TRUE(GetDisplayPropertyFromList(list, "mirrored", &value));
  bool mirrored;
  EXPECT_TRUE(value->GetAsBoolean(&mirrored));
  EXPECT_FALSE(mirrored);

  // Exiting the tablet mode should exit mirror mode.
  controller->EnableTabletModeWindowManager(false);
  ASSERT_FALSE(controller->IsTabletModeWindowManagerEnabled());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
}

TEST_F(DisplayPreferencesTest, AlreadyMirrorWhenEnterTableMode) {
  display::Display::SetInternalDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  LoggedInAsUser();
  UpdateDisplay("800x600,1200x800");
  display_manager()->SetMirrorMode(true);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  ash::TabletModeController* controller =
      ash::Shell::Get()->tablet_mode_controller();
  controller->EnableTabletModeWindowManager(true);
  ASSERT_TRUE(controller->IsTabletModeWindowManagerEnabled());
  EXPECT_TRUE(display_manager()->IsInMirrorMode());

  // Exiting the tablet mode should stay in mirror mode.
  controller->EnableTabletModeWindowManager(false);
  ASSERT_FALSE(controller->IsTabletModeWindowManagerEnabled());
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
}

}  // namespace chromeos
