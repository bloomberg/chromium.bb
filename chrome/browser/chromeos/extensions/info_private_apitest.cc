// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chromeos/settings/cros_settings_names.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_ChromeOSInfoPrivateTest) {
  // Set the initial timezone to a different one that JS function
  // timezoneSetTest() will attempt to set.
  base::StringValue initial_timezone("America/Los_Angeles");
  chromeos::CrosSettings::Get()->Set(chromeos::kSystemTimezone,
                                     initial_timezone);

  ASSERT_TRUE(RunComponentExtensionTest("chromeos_info_private")) << message_;
}
