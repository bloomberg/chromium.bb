// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/display_preferences.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/display/display_pref_util.h"
#include "ash/shell.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/common/pref_names.h"
#include "chromeos/display/output_configurator.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/gfx/display.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/screen.h"

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

ash::internal::DisplayManager* GetDisplayManager() {
  return ash::Shell::GetInstance()->display_manager();
}

// Returns true id the current user can write display preferences to
// Local State.
bool IsValidUser() {
  UserManager* user_manager = UserManager::Get();
  return (user_manager->IsUserLoggedIn() &&
          !user_manager->IsLoggedInAsDemoUser() &&
          !user_manager->IsLoggedInAsGuest() &&
          !user_manager->IsLoggedInAsStub());
}

ash::DisplayController* GetDisplayController() {
  return ash::Shell::GetInstance()->display_controller();
}

void LoadDisplayLayouts() {
  PrefService* local_state = g_browser_process->local_state();
  ash::DisplayController* display_controller = GetDisplayController();

  ash::DisplayLayout default_layout = ash::DisplayLayout::FromInts(
      local_state->GetInteger(prefs::kSecondaryDisplayLayout),
      local_state->GetInteger(prefs::kSecondaryDisplayOffset));
  default_layout.primary_id = local_state->GetInt64(prefs::kPrimaryDisplayID);
  display_controller->SetDefaultDisplayLayout(default_layout);

  const base::DictionaryValue* layouts = local_state->GetDictionary(
      prefs::kSecondaryDisplays);
  for (DictionaryValue::Iterator it(*layouts); !it.IsAtEnd(); it.Advance()) {
    ash::DisplayLayout layout;
    if (!ash::DisplayLayout::ConvertFromValue(it.value(), &layout)) {
      LOG(WARNING) << "Invalid preference value for " << it.key();
      continue;
    }

    if (it.key().find(",") != std::string::npos) {
      std::vector<std::string> ids;
      base::SplitString(it.key(), ',', &ids);
      int64 id1 = gfx::Display::kInvalidDisplayID;
      int64 id2 = gfx::Display::kInvalidDisplayID;
      if (!base::StringToInt64(ids[0], &id1) ||
          !base::StringToInt64(ids[1], &id2) ||
          id1 == gfx::Display::kInvalidDisplayID ||
          id2 == gfx::Display::kInvalidDisplayID) {
        continue;
      }
      display_controller->RegisterLayoutForDisplayIdPair(id1, id2, layout);
    } else {
      int64 id = gfx::Display::kInvalidDisplayID;
      if (!base::StringToInt64(it.key(), &id) ||
          id == gfx::Display::kInvalidDisplayID) {
        continue;
      }
      display_controller->RegisterLayoutForDisplayId(id, layout);
    }
  }
}

void LoadDisplayProperties() {
  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* properties = local_state->GetDictionary(
      prefs::kDisplayProperties);
  for (DictionaryValue::Iterator it(*properties); !it.IsAtEnd(); it.Advance()) {
    const base::DictionaryValue* dict_value = NULL;
    if (!it.value().GetAsDictionary(&dict_value) || dict_value == NULL)
      continue;
    int64 id = gfx::Display::kInvalidDisplayID;
    if (!base::StringToInt64(it.key(), &id) ||
        id == gfx::Display::kInvalidDisplayID) {
      continue;
    }
    gfx::Display::Rotation rotation = gfx::Display::ROTATE_0;
    float ui_scale = 1.0f;
    const gfx::Insets* insets_to_set = NULL;

    int rotation_value = 0;
    if (dict_value->GetInteger("rotation", &rotation_value)) {
      rotation = static_cast<gfx::Display::Rotation>(rotation_value);
    }
    int ui_scale_value = 0;
    if (dict_value->GetInteger("ui-scale", &ui_scale_value))
      ui_scale = static_cast<float>(ui_scale_value) / 1000.0f;
    gfx::Insets insets;
    if (ValueToInsets(*dict_value, &insets))
      insets_to_set = &insets;
    GetDisplayManager()->RegisterDisplayProperty(id,
                                                 rotation,
                                                 ui_scale,
                                                 insets_to_set);
  }
}

void StoreDisplayLayoutPref(const ash::DisplayIdPair& pair,
                            const ash::DisplayLayout& display_layout) {
  std::string name =
      base::Int64ToString(pair.first) + "," + base::Int64ToString(pair.second);

  PrefService* local_state = g_browser_process->local_state();
  DictionaryPrefUpdate update(local_state, prefs::kSecondaryDisplays);
  base::DictionaryValue* pref_data = update.Get();
  scoped_ptr<base::Value> layout_value(new base::DictionaryValue());
  if (pref_data->HasKey(name)) {
    base::Value* value = NULL;
    if (pref_data->Get(name, &value) && value != NULL)
      layout_value.reset(value->DeepCopy());
  }
  if (ash::DisplayLayout::ConvertToValue(display_layout, layout_value.get()))
    pref_data->Set(name, layout_value.release());
}

void StoreCurrentDisplayLayoutPrefs() {
  if (!IsValidUser() || GetDisplayManager()->num_connected_displays() < 2)
    return;

  ash::DisplayController* display_controller = GetDisplayController();
  ash::DisplayIdPair pair = display_controller->GetCurrentDisplayIdPair();
  ash::DisplayLayout display_layout =
      display_controller->GetRegisteredDisplayLayout(pair);
  StoreDisplayLayoutPref(pair, display_layout);
}

void StoreCurrentDisplayProperties() {
  ash::internal::DisplayManager* display_manager = GetDisplayManager();
  PrefService* local_state = g_browser_process->local_state();

  DictionaryPrefUpdate update(local_state, prefs::kDisplayProperties);
  base::DictionaryValue* pref_data = update.Get();

  size_t num = display_manager->GetNumDisplays();
  for (size_t i = 0; i < num; ++i) {
    int64 id = display_manager->GetDisplayAt(i)->id();
    ash::internal::DisplayInfo info = display_manager->GetDisplayInfo(id);

    scoped_ptr<base::DictionaryValue> property_value(
        new base::DictionaryValue());
    property_value->SetInteger("rotation", static_cast<int>(info.rotation()));
    property_value->SetInteger("ui-scale",
                               static_cast<int>(info.ui_scale() * 1000));
    if (info.has_custom_overscan_insets())
      InsetsToValue(info.overscan_insets_in_dip(), property_value.get());
    pref_data->Set(base::Int64ToString(id), property_value.release());
  }
}

typedef std::map<chromeos::DisplayPowerState, std::string>
    DisplayPowerStateToStringMap;

const DisplayPowerStateToStringMap* GetDisplayPowerStateToStringMap() {
  static const DisplayPowerStateToStringMap* map = ash::CreateToStringMap(
      chromeos::DISPLAY_POWER_ALL_ON, "all_on",
      chromeos::DISPLAY_POWER_ALL_OFF, "all_off",
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
  std::string value = iter != map->end() ? iter->second : std::string();
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetString(prefs::kDisplayPowerState, value);
}

void StoreCurrentDisplayPowerState() {
  StoreDisplayPowerState(
      ash::Shell::GetInstance()->output_configurator()->display_power_state());
}

}  // namespace

void RegisterDisplayLocalStatePrefs(PrefRegistrySimple* registry) {
  // The default secondary display layout.
  registry->RegisterIntegerPref(prefs::kSecondaryDisplayLayout,
                                static_cast<int>(ash::DisplayLayout::RIGHT));
  // The default offset of the secondary display position from the primary
  // display.
  registry->RegisterIntegerPref(prefs::kSecondaryDisplayOffset, 0);
  // Per-display preference.
  registry->RegisterDictionaryPref(prefs::kSecondaryDisplays);
  registry->RegisterDictionaryPref(prefs::kDisplayProperties);
  DisplayPowerStateToStringMap::const_iterator iter =
      GetDisplayPowerStateToStringMap()->find(chromeos::DISPLAY_POWER_ALL_ON);
  registry->RegisterStringPref(prefs::kDisplayPowerState, iter->second);
  registry->RegisterInt64Pref(prefs::kPrimaryDisplayID,
                              gfx::Display::kInvalidDisplayID);
}

void StoreDisplayPrefs() {
  if (!IsValidUser())
    return;
  StoreCurrentDisplayLayoutPrefs();
  StoreCurrentDisplayProperties();
  StoreCurrentDisplayPowerState();
}

void SetCurrentAndDefaultDisplayLayout(const ash::DisplayLayout& layout) {
  ash::DisplayController* display_controller = GetDisplayController();
  display_controller->SetLayoutForCurrentDisplays(layout);

  if (IsValidUser()) {
    PrefService* local_state = g_browser_process->local_state();
    ash::DisplayIdPair pair = display_controller->GetCurrentDisplayIdPair();
    // Use registered layout as the layout might have been inverted when
    // the displays are swapped.
    ash::DisplayLayout display_layout =
        display_controller->GetRegisteredDisplayLayout(pair);
    local_state->SetInteger(prefs::kSecondaryDisplayLayout,
                            static_cast<int>(display_layout.position));
    local_state->SetInteger(prefs::kSecondaryDisplayOffset,
                            display_layout.offset);
  }
}

void LoadDisplayPreferences(bool first_run_after_boot) {
  LoadDisplayLayouts();
  LoadDisplayProperties();
  if (!first_run_after_boot) {
    PrefService* local_state = g_browser_process->local_state();
    // Restore DisplayPowerState:
    std::string value = local_state->GetString(prefs::kDisplayPowerState);
    chromeos::DisplayPowerState power_state;
    if (GetDisplayPowerStateFromString(value, &power_state)) {
      ash::Shell::GetInstance()->output_configurator()->set_display_power_state(
          power_state);
    }
  }
}

// Stores the display layout for given display pairs.
void StoreDisplayLayoutPrefForTest(int64 id1,
                                   int64 id2,
                                   const ash::DisplayLayout& layout) {
  StoreDisplayLayoutPref(std::make_pair(id1, id2), layout);
}

// Stores the given |power_state|.
void StoreDisplayPowerStateForTest(DisplayPowerState power_state) {
  StoreDisplayPowerState(power_state);
}

}  // namespace chromeos
