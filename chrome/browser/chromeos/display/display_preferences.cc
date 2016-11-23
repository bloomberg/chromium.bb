// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/display_preferences.h"

#include <stddef.h>

#include "ash/display/display_pref_util.h"
#include "ash/display/json_converter.h"
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

std::string ColorProfileToString(ui::ColorCalibrationProfile profile) {
  switch (profile) {
    case ui::COLOR_PROFILE_STANDARD:
      return "standard";
    case ui::COLOR_PROFILE_DYNAMIC:
      return "dynamic";
    case ui::COLOR_PROFILE_MOVIE:
      return "movie";
    case ui::COLOR_PROFILE_READING:
      return "reading";
    case ui::NUM_COLOR_PROFILES:
      break;
  }
  NOTREACHED();
  return "";
}

ui::ColorCalibrationProfile StringToColorProfile(const std::string& value) {
  if (value == "standard")
    return ui::COLOR_PROFILE_STANDARD;
  else if (value == "dynamic")
    return ui::COLOR_PROFILE_DYNAMIC;
  else if (value == "movie")
    return ui::COLOR_PROFILE_MOVIE;
  else if (value == "reading")
    return ui::COLOR_PROFILE_READING;
  NOTREACHED();
  return ui::COLOR_PROFILE_STANDARD;
}

display::DisplayManager* GetDisplayManager() {
  return ash::Shell::GetInstance()->display_manager();
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
    if (!ash::JsonToDisplayLayout(it.value(), layout.get())) {
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

    ui::ColorCalibrationProfile color_profile = ui::COLOR_PROFILE_STANDARD;
    std::string color_profile_name;
    if (dict_value->GetString("color_profile_name", &color_profile_name))
      color_profile = StringToColorProfile(color_profile_name);
    GetDisplayManager()->RegisterDisplayProperty(id,
                                                 rotation,
                                                 ui_scale,
                                                 insets_to_set,
                                                 resolution_in_pixels,
                                                 device_scale_factor,
                                                 color_profile);
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
  if (ash::DisplayLayoutToJson(display_layout, layout_value.get()))
    pref_data->Set(name, layout_value.release());
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

    scoped_refptr<display::ManagedDisplayMode> mode =
        display_manager->GetSelectedModeForDisplayId(id);
    if (!display.IsInternal() && mode && !mode->native()) {
      property_value->SetInteger("width", mode->size().width());
      property_value->SetInteger("height", mode->size().height());
      property_value->SetInteger(
          "device-scale-factor",
          static_cast<int>(mode->device_scale_factor() * 1000));
    }
    if (!info.overscan_insets_in_dip().IsEmpty())
      InsetsToValue(info.overscan_insets_in_dip(), property_value.get());
    if (info.color_profile() != ui::COLOR_PROFILE_STANDARD) {
      property_value->SetString(
          "color_profile_name", ColorProfileToString(info.color_profile()));
    }
    pref_data->Set(base::Int64ToString(id), property_value.release());
  }
}

typedef std::map<chromeos::DisplayPowerState, std::string>
    DisplayPowerStateToStringMap;

const DisplayPowerStateToStringMap* GetDisplayPowerStateToStringMap() {
  // Don't save or retore ALL_OFF state. crbug.com/318456.
  static const DisplayPowerStateToStringMap* map = ash::CreateToStringMap(
      chromeos::DISPLAY_POWER_ALL_ON, "all_on",
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
      "internal_off_external_on",
      chromeos::DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF,
      "internal_on_external_off");
  return map;
}

bool GetDisplayPowerStateFromString(const base::StringPiece& state,
                                    chromeos::DisplayPowerState* field) {
  if (ash::ReverseFind(GetDisplayPowerStateToStringMap(), state, field))
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
      ash::Shell::GetInstance()->display_configurator()->
          requested_power_state());
}

void StoreCurrentDisplayRotationLockPrefs() {
  bool rotation_lock = ash::Shell::GetInstance()->display_manager()->
      registered_internal_display_rotation_lock();
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
      !ash::Shell::GetInstance()->ShouldSaveDisplaySettings()) {
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
      ash::Shell::GetInstance()->display_configurator()->SetInitialDisplayPower(
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

}  // namespace chromeos
