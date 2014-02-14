// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/pref_names.h"
#include "chromeos/settings/cros_settings_names.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, ChromeOSInfoPrivateTest) {
  // Set the initial timezone to a different one that JS function
  // timezoneSetTest() will attempt to set.
  base::StringValue initial_timezone("America/Los_Angeles");
  chromeos::CrosSettings::Get()->Set(chromeos::kSystemTimezone,
                                     initial_timezone);

  // Check that accessability settings are set to default values.
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kLargeCursorEnabled));
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kStickyKeysEnabled));
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kSpokenFeedbackEnabled));
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(prefs::kHighContrastEnabled));
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kScreenMagnifierEnabled));
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kAutoclickEnabled));

  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kLanguageSendFunctionKeys));

  ASSERT_TRUE(RunComponentExtensionTest("chromeos_info_private")) << message_;

  // Check that accessability settings have been all flipped by the test.
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kLargeCursorEnabled));
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kStickyKeysEnabled));
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kSpokenFeedbackEnabled));
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kHighContrastEnabled));
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kScreenMagnifierEnabled));
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kAutoclickEnabled));

  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kLanguageSendFunctionKeys));
}
