// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_caps_lock.h"

#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_test_api.h"
#include "ash/test/ash_test_base.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/chromeos/events/pref_names.h"

namespace ash {
namespace {

using TrayCapsLockTest = AshTestBase;

// Tests that the icon becomes visible when the tray controller toggles it.
TEST_F(TrayCapsLockTest, Visibility) {
  // prefs::kLanguageRemapSearchKeyTo is owned by chrome and shared with ash
  // when it connects. In unit tests, a TestingPrefServiceSimple is used
  // instead so only ash-owned prefs are registered by default. Manually
  // register prefs::kLanguageRemapSearchKeyTo so TrayCapsLock can be tested.
  static_cast<PrefRegistrySimple*>(Shell::Get()
                                       ->session_controller()
                                       ->GetLastActiveUserPrefService()
                                       ->DeprecatedGetPrefRegistry())
      ->RegisterIntegerPref(prefs::kLanguageRemapSearchKeyTo,
                            chromeos::input_method::kSearchKey);

  SystemTray* tray = GetPrimarySystemTray();
  TrayCapsLock* caps_lock = SystemTrayTestApi(tray).tray_caps_lock();

  // By default the icon isn't visible.
  EXPECT_FALSE(caps_lock->tray_view()->visible());

  // Simulate turning on caps lock.
  caps_lock->OnCapsLockChanged(true);
  EXPECT_TRUE(caps_lock->tray_view()->visible());

  // Simulate turning off caps lock.
  caps_lock->OnCapsLockChanged(false);
  EXPECT_FALSE(caps_lock->tray_view()->visible());
}

}  // namespace
}  // namespace ash
