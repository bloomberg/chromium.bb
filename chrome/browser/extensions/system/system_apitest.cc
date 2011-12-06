// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, SystemApi) {
  PrefService* pref_service = browser()->profile()->GetPrefs();
  pref_service->SetInteger(prefs::kIncognitoModeAvailability, 1);

  EXPECT_TRUE(RunComponentExtensionTest("system")) << message_;
}
