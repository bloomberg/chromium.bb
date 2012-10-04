// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/display/display_preferences.h"

#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/url_canon.h"
#include "googleurl/src/url_util.h"
#include "ui/aura/display_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/env.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace chromeos {

namespace {
// Replaces dot "." by "%2E" since it's the path separater of base::Value.  Also
// replaces "%" by "%25" for unescaping.
void EscapeDisplayName(const std::string& name, std::string* escaped) {
  DCHECK(escaped);
  std::string middle;
  ReplaceChars(name, "%", "%25", &middle);
  ReplaceChars(middle, ".", "%2E", escaped);
}

// Unescape %-encoded characters.
std::string UnescapeDisplayName(const std::string& name) {
  url_canon::RawCanonOutputT<char16> decoded;
  url_util::DecodeURLEscapeSequences(name.data(), name.size(), &decoded);
  // Display names are ASCII-only.
  return UTF16ToASCII(string16(decoded.data(), decoded.length()));
}

void NotifyDisplayLayoutChanged(PrefService* pref_service) {
  ash::DisplayController* display_controller =
      ash::Shell::GetInstance()->display_controller();

  ash::DisplayLayout default_layout(
      static_cast<ash::DisplayLayout::Position>(pref_service->GetInteger(
          prefs::kSecondaryDisplayLayout)),
      pref_service->GetInteger(prefs::kSecondaryDisplayOffset));
  display_controller->SetDefaultDisplayLayout(default_layout);

  const base::DictionaryValue* layouts = pref_service->GetDictionary(
      prefs::kSecondaryDisplays);
  for (base::DictionaryValue::key_iterator it = layouts->begin_keys();
       it != layouts->end_keys(); ++it) {
    const base::Value* value = NULL;
    if (!layouts->Get(*it, &value) || value == NULL)
      continue;

    ash::DisplayLayout layout;
    if (!ash::DisplayLayout::ConvertFromValue(*value, &layout)) {
      LOG(WARNING) << "Invalid preference value for " << *it;
      continue;
    }

    display_controller->SetLayoutForDisplayName(
        UnescapeDisplayName(*it), layout);
  }
}

// Notification is currently sent when the pref is initialized and the pref is
// updated.  It means that the primary display isn't set when it's connected to
// the system later.
void NotifyPrimaryDisplayIDChanged(PrefService* pref_service) {
  int64 id = pref_service->GetInt64(prefs::kPrimaryDisplayID);
  if (id == gfx::Display::kInvalidDisplayID)
    return;

  aura::DisplayManager* display_manager =
      aura::Env::GetInstance()->display_manager();
  for (size_t i = 0; i < display_manager->GetNumDisplays(); ++i) {
    gfx::Display* display = display_manager->GetDisplayAt(i);
    if (display->id() == id) {
      ash::Shell::GetInstance()->display_controller()->
          SetPrimaryDisplay(*display);
      return;
    }
  }
}

// PrimaryDisplaySwitchObserver observes the change of the primary/secondary
// displays and store the current primary display ID into the local preference.
class PrimaryDisplaySwitchObserver : public aura::RootWindowObserver,
                                     public aura::WindowObserver {
 public:
  // Creates an instance of the observer and starts observing the status.  Since
  // it will automatically delete itself at window destroying, it is not
  // necessary to handle the ownership here.
  static void StartObserver() {
    new PrimaryDisplaySwitchObserver();
  }

 protected:
  // aura::RootWindow::Observer overrides:
  virtual void OnRootWindowMoved(const aura::RootWindow* root_window,
                                 const gfx::Point& new_origin) OVERRIDE {
    PrefService* pref_service = ProfileManager::GetDefaultProfile()->GetPrefs();
    pref_service->SetInt64(
        prefs::kPrimaryDisplayID, gfx::Screen::GetPrimaryDisplay().id());
  }

  // aura::Window::Observer overrides:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    delete this;
  }

 private:
  PrimaryDisplaySwitchObserver() : primary_root_(NULL) {
    // ash::Shell doesn't exist in unit_tests.
    if (ash::Shell::HasInstance()) {
      primary_root_ = ash::Shell::GetInstance()->GetPrimaryRootWindow();
      primary_root_->AddRootWindowObserver(this);
      primary_root_->AddObserver(this);
    }
  }

  virtual ~PrimaryDisplaySwitchObserver() {
    if (primary_root_) {
      primary_root_->RemoveRootWindowObserver(this);
      primary_root_->RemoveObserver(this);
    }
  }

  aura::RootWindow* primary_root_;

  DISALLOW_COPY_AND_ASSIGN(PrimaryDisplaySwitchObserver);
};

}  // namespace

void RegisterDisplayPrefs(PrefService* pref_service) {
  // The default secondary display layout.
  pref_service->RegisterIntegerPref(prefs::kSecondaryDisplayLayout,
                                    static_cast<int>(ash::DisplayLayout::RIGHT),
                                    PrefService::UNSYNCABLE_PREF);
  // The default offset of the secondary display position from the primary
  // display.
  pref_service->RegisterIntegerPref(prefs::kSecondaryDisplayOffset,
                                    0,
                                    PrefService::UNSYNCABLE_PREF);
  // Per-display preference.
  pref_service->RegisterDictionaryPref(prefs::kSecondaryDisplays,
                                       PrefService::UNSYNCABLE_PREF);

  // Primary output name.
  pref_service->RegisterInt64Pref(prefs::kPrimaryDisplayID,
                                  gfx::Display::kInvalidDisplayID,
                                  PrefService::UNSYNCABLE_PREF);
  PrimaryDisplaySwitchObserver::StartObserver();
}

void SetDisplayLayoutPref(PrefService* pref_service,
                          const gfx::Display& display,
                          int layout,
                          int offset) {
  {
    DictionaryPrefUpdate update(pref_service, prefs::kSecondaryDisplays);
    ash::DisplayLayout display_layout(
        static_cast<ash::DisplayLayout::Position>(layout), offset);

    aura::DisplayManager* display_manager =
        aura::Env::GetInstance()->display_manager();
    std::string name;
    EscapeDisplayName(display_manager->GetDisplayNameFor(display),
                      &name);
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

  pref_service->SetInteger(prefs::kSecondaryDisplayLayout, layout);
  pref_service->SetInteger(prefs::kSecondaryDisplayOffset, offset);

  NotifyDisplayLayoutChanged(pref_service);
}

void SetPrimaryDisplayIDPref(PrefService* pref_service, int64 display_id) {
  pref_service->SetInt64(prefs::kPrimaryDisplayID, display_id);
  NotifyPrimaryDisplayIDChanged(pref_service);
}

void NotifyDisplayPrefChanged(PrefService* pref_service) {
  NotifyDisplayLayoutChanged(pref_service);
  NotifyPrimaryDisplayIDChanged(pref_service);
}

}  // namespace chromeos
