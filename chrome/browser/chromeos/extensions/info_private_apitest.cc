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
  PrefService* prefs = profile()->GetPrefs();
  ASSERT_FALSE(prefs->GetBoolean(prefs::kAccessibilityLargeCursorEnabled));
  ASSERT_FALSE(prefs->GetBoolean(prefs::kAccessibilityStickyKeysEnabled));
  ASSERT_FALSE(prefs->GetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled));
  ASSERT_FALSE(prefs->GetBoolean(prefs::kAccessibilityHighContrastEnabled));
  ASSERT_FALSE(prefs->GetBoolean(prefs::kAccessibilityScreenMagnifierEnabled));
  ASSERT_FALSE(prefs->GetBoolean(prefs::kAccessibilityAutoclickEnabled));

  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kLanguageSendFunctionKeys));

  ASSERT_TRUE(RunComponentExtensionTest("chromeos_info_private")) << message_;

  // Check that accessability settings have been all flipped by the test.
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityLargeCursorEnabled));
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityStickyKeysEnabled));
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilitySpokenFeedbackEnabled));
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityHighContrastEnabled));
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityScreenMagnifierEnabled));
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAccessibilityAutoclickEnabled));

  ASSERT_TRUE(prefs->GetBoolean(prefs::kLanguageSendFunctionKeys));
}
