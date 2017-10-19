// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/display_preferences.h"

#include <stddef.h>

#include "ash/shell.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_utilities.h"
#include "ui/display/manager/display_pref_util.h"
#include "ui/display/manager/json_converter.h"
#include "ui/display/types/display_constants.h"
#include "ui/gfx/geometry/insets.h"
#include "url/url_canon.h"
#include "url/url_util.h"

namespace chromeos {
namespace {

const char kInsetsTopKey[] = "insets_top";
const char kInsetsLeftKey[] = "insets_left";
const char kInsetsBottomKey[] = "insets_bottom";
const char kInsetsRightKey[] = "insets_right";

const char kTouchCalibrationMap[] = "touch_calibration_map";
const char kTouchCalibrationWidth[] = "touch_calibration_width";
const char kTouchCalibrationHeight[] = "touch_calibration_height";
const char kTouchCalibrationPointPairs[] = "touch_calibration_point_pairs";

// This kind of boilerplates should be done by base::JSONValueConverter but it
// doesn't support classes like gfx::Insets for now.
// TODO(mukai): fix base::JSONValueConverter and use it here.
bool ValueToInsets(const base::DictionaryValue& value, gfx::Insets* insets) {
  DCHECK(insets);
  int top = 0;
  int left = 0;
  int bottom = 0;
  int right = 0;
  if (value.GetInteger(kInsetsTopKey, &top) &&
      value.GetInteger(kInsetsLeftKey, &left) &&
      value.GetInteger(kInsetsBottomKey, &bottom) &&
      value.GetInteger(kInsetsRightKey, &right)) {
    insets->Set(top, left, bottom, right);
    return true;
  }
  return false;
}

void InsetsToValue(const gfx::Insets& insets, base::DictionaryValue* value) {
  DCHECK(value);
  value->SetInteger(kInsetsTopKey, insets.top());
  value->SetInteger(kInsetsLeftKey, insets.left());
  value->SetInteger(kInsetsBottomKey, insets.bottom());
  value->SetInteger(kInsetsRightKey, insets.right());
}

// Unmarshalls the string containing CalibrationPointPairQuad and populates
// |point_pair_quad| with the unmarshalled data.
bool ParseTouchCalibrationStringValue(
    const std::string& str,
    display::TouchCalibrationData::CalibrationPointPairQuad* point_pair_quad) {
  DCHECK(point_pair_quad);
  int x = 0, y = 0;
  std::vector<std::string> parts = base::SplitString(
      str, " ", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  size_t total = point_pair_quad->size();
  gfx::Point display_point, touch_point;
  for (std::size_t row = 0; row < total; row++) {
    if (!base::StringToInt(parts[row * total], &x) ||
        !base::StringToInt(parts[row * total + 1], &y)) {
      return false;
    }
    display_point.SetPoint(x, y);

    if (!base::StringToInt(parts[row * total + 2], &x) ||
        !base::StringToInt(parts[row * total + 3], &y)) {
      return false;
    }
    touch_point.SetPoint(x, y);

    (*point_pair_quad)[row] = std::make_pair(display_point, touch_point);
  }
  return true;
}

// Retrieves touch calibration associated data from the dictionary and stores
// it in an instance of TouchCalibrationData struct.
bool ValueToTouchData(const base::DictionaryValue& value,
                      display::TouchCalibrationData* touch_calibration_data) {
  display::TouchCalibrationData::CalibrationPointPairQuad* point_pair_quad =
      &(touch_calibration_data->point_pairs);

  std::string str;
  if (!value.GetString(kTouchCalibrationPointPairs, &str))
    return false;

  if (!ParseTouchCalibrationStringValue(str, point_pair_quad))
    return false;

  int width, height;
  if (!value.GetInteger(kTouchCalibrationWidth, &width) ||
      !value.GetInteger(kTouchCalibrationHeight, &height)) {
    return false;
  }
  touch_calibration_data->bounds = gfx::Size(width, height);
  return true;
}

bool ValueToTouchDataMap(
    const base::DictionaryValue& value,
    std::map<uint32_t, display::TouchCalibrationData>* touch_calibration_map) {
  const base::DictionaryValue* map_dictionary;

  if (!value.GetDictionary(kTouchCalibrationMap, &map_dictionary))
    return false;

  for (const auto& data_pair : *map_dictionary) {
    uint32_t touch_device_identifier;
    if (!base::StringToUint(data_pair.first, &touch_device_identifier))
      return false;
    const base::DictionaryValue* data_dict;
    if (!data_pair.second->GetAsDictionary(&data_dict))
      return false;
    display::TouchCalibrationData data;
    if (!ValueToTouchData(*data_dict, &data))
      return false;
    touch_calibration_map->emplace(touch_device_identifier, data);
  }
  return true;
}

// Stores the touch calibration data into the dictionary.
void TouchDataToValue(
    const display::TouchCalibrationData& touch_calibration_data,
    base::DictionaryValue* value) {
  DCHECK(value);
  std::string str;
  for (std::size_t row = 0; row < touch_calibration_data.point_pairs.size();
       row++) {
    str +=
        base::IntToString(touch_calibration_data.point_pairs[row].first.x()) +
        " ";
    str +=
        base::IntToString(touch_calibration_data.point_pairs[row].first.y()) +
        " ";
    str +=
        base::IntToString(touch_calibration_data.point_pairs[row].second.x()) +
        " ";
    str +=
        base::IntToString(touch_calibration_data.point_pairs[row].second.y());
    if (row != touch_calibration_data.point_pairs.size() - 1)
      str += " ";
  }
  value->SetString(kTouchCalibrationPointPairs, str);
  value->SetInteger(kTouchCalibrationWidth,
                    touch_calibration_data.bounds.width());
  value->SetInteger(kTouchCalibrationHeight,
                    touch_calibration_data.bounds.height());
}

// Stores the entire touch calibration data map to the dictionary.
void TouchDataMapToValue(
    const std::map<uint32_t, display::TouchCalibrationData>& touch_data_map,
    base::DictionaryValue* value) {
  std::unique_ptr<base::DictionaryValue> touch_data_map_dictionary =
      std::make_unique<base::DictionaryValue>();

  for (const auto& pair : touch_data_map) {
    std::unique_ptr<base::DictionaryValue> touch_data_dictionary =
        std::make_unique<base::DictionaryValue>();
    TouchDataToValue(pair.second, touch_data_dictionary.get());

    touch_data_map_dictionary->SetDictionary(base::UintToString(pair.first),
                                             std::move(touch_data_dictionary));
  }
  value->SetDictionary(kTouchCalibrationMap,
                       std::move(touch_data_map_dictionary));
}

display::DisplayManager* GetDisplayManager() {
  return ash::Shell::Get()->display_manager();
}

// Returns true id the current user can write display preferences to
// Local State.
bool UserCanSaveDisplayPreference() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  return user_manager->IsUserLoggedIn() &&
      (user_manager->IsLoggedInAsUserWithGaiaAccount() ||
       user_manager->IsLoggedInAsSupervisedUser() ||
       user_manager->IsLoggedInAsKioskApp());
}

void LoadDisplayLayouts() {
  PrefService* local_state = g_browser_process->local_state();
  display::DisplayLayoutStore* layout_store =
      GetDisplayManager()->layout_store();

  const base::DictionaryValue* layouts = local_state->GetDictionary(
      prefs::kSecondaryDisplays);
  for (base::DictionaryValue::Iterator it(*layouts);
       !it.IsAtEnd(); it.Advance()) {
    std::unique_ptr<display::DisplayLayout> layout(new display::DisplayLayout);
    if (!display::JsonToDisplayLayout(it.value(), layout.get())) {
      LOG(WARNING) << "Invalid preference value for " << it.key();
      continue;
    }

    if (it.key().find(",") != std::string::npos) {
      std::vector<std::string> ids_str = base::SplitString(
          it.key(), ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
      std::vector<int64_t> ids;
      for (std::string id_str : ids_str) {
        int64_t id;
        if (!base::StringToInt64(id_str, &id))
          continue;
        ids.push_back(id);
      }
      display::DisplayIdList list =
          display::GenerateDisplayIdList(ids.begin(), ids.end());
      layout_store->RegisterLayoutForDisplayIdList(list, std::move(layout));
    }
  }
}

void LoadDisplayProperties() {
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* properties = local_state->GetDictionary(
      prefs::kDisplayProperties);
  for (base::DictionaryValue::Iterator it(*properties);
       !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* dict_value = nullptr;
    if (!it.value().GetAsDictionary(&dict_value) || dict_value == nullptr)
      continue;
    int64_t id = display::kInvalidDisplayId;
    if (!base::StringToInt64(it.key(), &id) ||
        id == display::kInvalidDisplayId) {
      continue;
    }
    display::Display::Rotation rotation = display::Display::ROTATE_0;
    float ui_scale = 1.0f;
    const gfx::Insets* insets_to_set = nullptr;

    int rotation_value = 0;
    if (dict_value->GetInteger("rotation", &rotation_value)) {
      rotation = static_cast<display::Display::Rotation>(rotation_value);
    }
    int ui_scale_value = 0;
    if (dict_value->GetInteger("ui-scale", &ui_scale_value))
      ui_scale = static_cast<float>(ui_scale_value) / 1000.0f;

    int width = 0, height = 0;
    dict_value->GetInteger("width", &width);
    dict_value->GetInteger("height", &height);
    gfx::Size resolution_in_pixels(width, height);

    float device_scale_factor = 1.0;
    int dsf_value = 0;
    if (dict_value->GetInteger("device-scale-factor", &dsf_value))
      device_scale_factor = static_cast<float>(dsf_value) / 1000.0f;

    gfx::Insets insets;
    if (ValueToInsets(*dict_value, &insets))
      insets_to_set = &insets;

    // Retrive any legacy touch calibration data.
    display::TouchCalibrationData calibration_data;
    display::TouchCalibrationData* calibration_data_to_set = nullptr;
    if (ValueToTouchData(*dict_value, &calibration_data))
      calibration_data_to_set = &calibration_data;

    std::map<uint32_t, display::TouchCalibrationData> calibration_data_map;
    std::map<uint32_t, display::TouchCalibrationData>*
        calibration_data_map_to_set = nullptr;
    if (ValueToTouchDataMap(*dict_value, &calibration_data_map))
      calibration_data_map_to_set = &calibration_data_map;

    // Migrate legacy touch calibration data to updated model. Use it as a
    // fallback mechanism.
    if (calibration_data_to_set) {
      const uint32_t fallback_identifier =
          display::TouchCalibrationData::GetFallbackTouchDeviceIdentifier();
      // Store the legacy cailbration data in the calibration data map if we
      // have a non-null calibration data map with no previous fallback
      // calibration data stored in it.
      if (calibration_data_map_to_set &&
          calibration_data_map_to_set->count(fallback_identifier) == 0) {
        (*calibration_data_map_to_set)[fallback_identifier] =
            *calibration_data_to_set;
      } else {
        calibration_data_map[fallback_identifier] = *calibration_data_to_set;
        calibration_data_map_to_set = &calibration_data_map;
      }
    }

    GetDisplayManager()->RegisterDisplayProperty(
        id, rotation, ui_scale, insets_to_set, resolution_in_pixels,
        device_scale_factor, calibration_data_map_to_set);
  }
}

void LoadDisplayRotationState() {
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* properties =
      local_state->GetDictionary(prefs::kDisplayRotationLock);

  bool rotation_lock = false;
  if (!properties->GetBoolean("lock", &rotation_lock))
    return;

  int rotation = display::Display::ROTATE_0;
  if (!properties->GetInteger("orientation", &rotation))
    return;

  GetDisplayManager()->RegisterDisplayRotationProperties(
      rotation_lock, static_cast<display::Display::Rotation>(rotation));
}

void StoreDisplayLayoutPref(const display::DisplayIdList& list,
                            const display::DisplayLayout& display_layout) {
  DCHECK(display::DisplayLayout::Validate(list, display_layout));
  std::string name = display::DisplayIdListToString(list);

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate update(local_state, prefs::kSecondaryDisplays);
  base::DictionaryValue* pref_data = update.Get();
  std::unique_ptr<base::Value> layout_value(new base::DictionaryValue());
  if (pref_data->HasKey(name)) {
    base::Value* value = nullptr;
    if (pref_data->Get(name, &value) && value != nullptr)
      layout_value.reset(value->DeepCopy());
  }
  if (display::DisplayLayoutToJson(display_layout, layout_value.get()))
    pref_data->Set(name, std::move(layout_value));
}

void StoreCurrentDisplayLayoutPrefs() {
  display::DisplayManager* display_manager = GetDisplayManager();
  if (!UserCanSaveDisplayPreference() ||
      display_manager->num_connected_displays() < 2) {
    return;
  }

  display::DisplayIdList list = display_manager->GetCurrentDisplayIdList();
  const display::DisplayLayout& display_layout =
      display_manager->layout_store()->GetRegisteredDisplayLayout(list);

  if (!display::DisplayLayout::Validate(list, display_layout)) {
    // We should never apply an invalid layout, if we do, it persists and the
    // user has no way of fixing it except by deleting the local state.
    LOG(ERROR) << "Attempting to store an invalid display layout in the local"
               << " state. Skipping.";
    return;
  }

  StoreDisplayLayoutPref(list, display_layout);
}

void StoreCurrentDisplayProperties() {
  display::DisplayManager* display_manager = GetDisplayManager();
  PrefService* local_state = g_browser_process->local_state();

  DictionaryPrefUpdate update(local_state, prefs::kDisplayProperties);
  base::DictionaryValue* pref_data = update.Get();

  size_t num = display_manager->GetNumDisplays();
  for (size_t i = 0; i < num; ++i) {
    const display::Display& display = display_manager->GetDisplayAt(i);
    int64_t id = display.id();
    display::ManagedDisplayInfo info = display_manager->GetDisplayInfo(id);

    std::unique_ptr<base::DictionaryValue> property_value(
        new base::DictionaryValue());
    // Don't save the display preference in unified mode because its
    // size and modes can change depending on the combination of displays.
    if (display_manager->IsInUnifiedMode())
      continue;
    property_value->SetInteger("rotation",
                               static_cast<int>(info.GetRotation(
                                   display::Display::ROTATION_SOURCE_USER)));
    property_value->SetInteger(
        "ui-scale", static_cast<int>(info.configured_ui_scale() * 1000));

    display::ManagedDisplayMode mode;
    if (!display.IsInternal() &&
        display_manager->GetSelectedModeForDisplayId(id, &mode) &&
        !mode.native()) {
      property_value->SetInteger("width", mode.size().width());
      property_value->SetInteger("height", mode.size().height());
      property_value->SetInteger(
          "device-scale-factor",
          static_cast<int>(mode.device_scale_factor() * 1000));
    }
    if (!info.overscan_insets_in_dip().IsEmpty())
      InsetsToValue(info.overscan_insets_in_dip(), property_value.get());

    if (info.touch_calibration_data_map().size()) {
      TouchDataMapToValue(info.touch_calibration_data_map(),
                          property_value.get());

      // Ensure that the legacy data is still stored just in case.
      uint32_t fallback_identifier =
          display::TouchCalibrationData::GetFallbackTouchDeviceIdentifier();
      if (info.HasTouchCalibrationData(fallback_identifier)) {
        TouchDataToValue(info.GetTouchCalibrationData(fallback_identifier),
                         property_value.get());
      }
    }

    pref_data->Set(base::Int64ToString(id), std::move(property_value));
  }
}

typedef std::map<chromeos::DisplayPowerState, std::string>
    DisplayPowerStateToStringMap;

const DisplayPowerStateToStringMap* GetDisplayPowerStateToStringMap() {
  // Don't save or retore ALL_OFF state. crbug.com/318456.
  static const DisplayPowerStateToStringMap* map = display::CreateToStringMap(
      chromeos::DISPLAY_POWER_ALL_ON, "all_on",
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
      "internal_off_external_on",
      chromeos::DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF,
      "internal_on_external_off");
  return map;
}

bool GetDisplayPowerStateFromString(const base::StringPiece& state,
                                    chromeos::DisplayPowerState* field) {
  if (display::ReverseFind(GetDisplayPowerStateToStringMap(), state, field))
    return true;
  LOG(ERROR) << "Invalid display power state value:" << state;
  return false;
}

void StoreDisplayPowerState(DisplayPowerState power_state) {
  const DisplayPowerStateToStringMap* map = GetDisplayPowerStateToStringMap();
  DisplayPowerStateToStringMap::const_iterator iter = map->find(power_state);
  if (iter != map->end()) {
    PrefService* local_state = g_browser_process->local_state();
    local_state->SetString(prefs::kDisplayPowerState, iter->second);
  }
}

void StoreCurrentDisplayPowerState() {
  StoreDisplayPowerState(
      ash::Shell::Get()->display_configurator()->requested_power_state());
}

void StoreCurrentDisplayRotationLockPrefs() {
  bool rotation_lock = ash::Shell::Get()
                           ->display_manager()
                           ->registered_internal_display_rotation_lock();
  StoreDisplayRotationPrefs(rotation_lock);
}

}  // namespace

void RegisterDisplayLocalStatePrefs(PrefRegistrySimple* registry) {
  // Per-display preference.
  registry->RegisterDictionaryPref(prefs::kSecondaryDisplays);
  registry->RegisterDictionaryPref(prefs::kDisplayProperties);
  DisplayPowerStateToStringMap::const_iterator iter =
      GetDisplayPowerStateToStringMap()->find(chromeos::DISPLAY_POWER_ALL_ON);
  registry->RegisterStringPref(prefs::kDisplayPowerState, iter->second);
  registry->RegisterDictionaryPref(prefs::kDisplayRotationLock);
}

void StoreDisplayPrefs() {
  // Stores the power state regardless of the login status, because the power
  // state respects to the current status (close/open) of the lid which can be
  // changed in any situation. See crbug.com/285360
  StoreCurrentDisplayPowerState();
  StoreCurrentDisplayRotationLockPrefs();

  // Do not store prefs when the confirmation dialog is shown.
  if (!UserCanSaveDisplayPreference() ||
      !ash::Shell::Get()->ShouldSaveDisplaySettings()) {
    return;
  }

  StoreCurrentDisplayLayoutPrefs();
  StoreCurrentDisplayProperties();
}

void StoreDisplayRotationPrefs(bool rotation_lock) {
  if (!display::Display::HasInternalDisplay())
    return;

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate update(local_state, prefs::kDisplayRotationLock);
  base::DictionaryValue* pref_data = update.Get();
  pref_data->SetBoolean("lock", rotation_lock);
  display::Display::Rotation rotation =
      GetDisplayManager()
          ->GetDisplayInfo(display::Display::InternalDisplayId())
          .GetRotation(display::Display::ROTATION_SOURCE_ACCELEROMETER);
  pref_data->SetInteger("orientation", static_cast<int>(rotation));
}

void LoadDisplayPreferences(bool first_run_after_boot) {
  LoadDisplayLayouts();
  LoadDisplayProperties();
  LoadDisplayRotationState();
  if (!first_run_after_boot) {
    PrefService* local_state = g_browser_process->local_state();
    // Restore DisplayPowerState:
    std::string value = local_state->GetString(prefs::kDisplayPowerState);
    chromeos::DisplayPowerState power_state;
    if (GetDisplayPowerStateFromString(value, &power_state)) {
      ash::Shell::Get()->display_configurator()->SetInitialDisplayPower(
          power_state);
    }
  }
}

// Stores the display layout for given display pairs.
void StoreDisplayLayoutPrefForTest(const display::DisplayIdList& list,
                                   const display::DisplayLayout& layout) {
  StoreDisplayLayoutPref(list, layout);
}

// Stores the given |power_state|.
void StoreDisplayPowerStateForTest(DisplayPowerState power_state) {
  StoreDisplayPowerState(power_state);
}

bool ParseTouchCalibrationStringForTest(
    const std::string& str,
    display::TouchCalibrationData::CalibrationPointPairQuad* point_pair_quad) {
  return ParseTouchCalibrationStringValue(str, point_pair_quad);
}

}  // namespace chromeos
