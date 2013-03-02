// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/display_preferences.h"

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
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
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/screen.h"

namespace chromeos {
namespace {

// This kind of boilerplates should be done by base::JSONValueConverter but it
// doesn't support classes like gfx::Insets for now.
// TODO(mukai): fix base::JSONValueConverter and use it here.
bool ValueToInsets(const base::DictionaryValue& value, gfx::Insets* insets) {
  DCHECK(insets);
  int top = 0;
  int left = 0;
  int bottom = 0;
  int right = 0;
  if (value.GetInteger("top", &top) &&
      value.GetInteger("left", &left) &&
      value.GetInteger("bottom", &bottom) &&
      value.GetInteger("right", &right)) {
    insets->Set(top, left, bottom, right);
    return true;
  }
  return false;
}

void InsetsToValue(const gfx::Insets& insets, base::DictionaryValue* value) {
  DCHECK(value);
  value->SetInteger("top", insets.top());
  value->SetInteger("left", insets.left());
  value->SetInteger("bottom", insets.bottom());
  value->SetInteger("right", insets.right());
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

void NotifyDisplayLayoutChanged() {
  PrefService* local_state = g_browser_process->local_state();
  ash::DisplayController* display_controller = GetDisplayController();

  ash::DisplayLayout default_layout(
      static_cast<ash::DisplayLayout::Position>(local_state->GetInteger(
          prefs::kSecondaryDisplayLayout)),
      local_state->GetInteger(prefs::kSecondaryDisplayOffset));
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

void NotifyDisplayOverscans() {
  PrefService* local_state = g_browser_process->local_state();

  const base::DictionaryValue* overscans = local_state->GetDictionary(
      prefs::kDisplayOverscans);
  for (DictionaryValue::Iterator it(*overscans);
       !it.IsAtEnd();
       it.Advance()) {
    int64 display_id = gfx::Display::kInvalidDisplayID;
    if (!base::StringToInt64(it.key(), &display_id)) {
      LOG(WARNING) << "Invalid key, cannot convert to display ID: " << it.key();
      continue;
    }

    const base::DictionaryValue* value = NULL;
    if (!it.value().GetAsDictionary(&value)) {
      LOG(WARNING) << "Can't find dictionary value for " << it.key();
      continue;
    }

    gfx::Insets insets;
    if (!ValueToInsets(*value, &insets)) {
      LOG(WARNING) << "Can't convert the data into insets for " << it.key();
      continue;
    }

    GetDisplayController()->SetOverscanInsets(display_id, insets);
  }
}

void StoreDisplayLayoutPref(int64 id1,
                            int64 id2,
                            const ash::DisplayLayout& display_layout) {
  std::string name = base::Int64ToString(id1) + "," + base::Int64ToString(id2);

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
  local_state->SetInteger(prefs::kSecondaryDisplayLayout,
                          static_cast<int>(display_layout.position));
  local_state->SetInteger(prefs::kSecondaryDisplayOffset,
                          display_layout.offset);
}

void StoreCurrentDisplayLayoutPrefs() {
  if (!IsValidUser() || gfx::Screen::GetNativeScreen()->GetNumDisplays() < 2)
    return;

  ash::DisplayController* display_controller = GetDisplayController();
  ash::DisplayLayout display_layout =
      display_controller->GetCurrentDisplayLayout();
  ash::DisplayIdPair pair = display_controller->GetCurrentDisplayIdPair();

  StoreDisplayLayoutPref(pair.first, pair.second, display_layout);
}


void StorePrimaryDisplayIDPref(int64 display_id) {
  if (!IsValidUser())
    return;

  PrefService* local_state = g_browser_process->local_state();
  if (GetDisplayManager()->IsInternalDisplayId(display_id))
    local_state->ClearPref(prefs::kPrimaryDisplayID);
  else
    local_state->SetInt64(prefs::kPrimaryDisplayID, display_id);
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

  // Primary output name.
  registry->RegisterInt64Pref(prefs::kPrimaryDisplayID,
                              gfx::Display::kInvalidDisplayID);

  // Display overscan preference.
  registry->RegisterDictionaryPref(prefs::kDisplayOverscans);
}

void StoreDisplayPrefs() {
  StorePrimaryDisplayIDPref(ash::Shell::GetScreen()->GetPrimaryDisplay().id());
  StoreCurrentDisplayLayoutPrefs();
}

void SetAndStoreDisplayLayoutPref(int layout, int offset) {
  ash::DisplayLayout display_layout(
      static_cast<ash::DisplayLayout::Position>(layout), offset);
  ash::Shell::GetInstance()->display_controller()->
      SetLayoutForCurrentDisplays(display_layout);
  StoreCurrentDisplayLayoutPrefs();
}

void StoreDisplayLayoutPref(int64 id1, int64 id2,
                            int layout, int offset) {
  ash::DisplayLayout display_layout(
      static_cast<ash::DisplayLayout::Position>(layout), offset);
  StoreDisplayLayoutPref(id1, id2, display_layout);
}

void SetAndStoreDisplayOverscan(const gfx::Display& display,
                                const gfx::Insets& insets) {
  if (IsValidUser()) {
    DictionaryPrefUpdate update(
        g_browser_process->local_state(), prefs::kDisplayOverscans);
    const std::string id = base::Int64ToString(display.id());

    base::DictionaryValue* pref_data = update.Get();
    base::DictionaryValue* insets_value = new base::DictionaryValue();
    InsetsToValue(insets, insets_value);
    pref_data->Set(id, insets_value);
  }

  ash::Shell::GetInstance()->display_controller()->SetOverscanInsets(
      display.id(), insets);
}

void SetAndStorePrimaryDisplayIDPref(int64 display_id) {
  StorePrimaryDisplayIDPref(display_id);
  ash::Shell::GetInstance()->display_controller()->SetPrimaryDisplayId(
      display_id);
}

void NotifyDisplayLocalStatePrefChanged() {
  PrefService* local_state = g_browser_process->local_state();
  ash::Shell::GetInstance()->display_controller()->SetPrimaryDisplayId(
      local_state->GetInt64(prefs::kPrimaryDisplayID));
  NotifyDisplayLayoutChanged();
  NotifyDisplayOverscans();
}

}  // namespace chromeos
