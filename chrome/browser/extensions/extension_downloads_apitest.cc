// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"

class DownloadsApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

  void SetUpTempDownloadsDir() {
    ASSERT_TRUE(tmpdir.CreateUniqueTempDir());
    browser()->profile()->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory, tmpdir.path());
  }

 private:
  ScopedTempDir tmpdir;
};

IN_PROC_BROWSER_TEST_F(DownloadsApiTest, DISABLED_Downloads) {
  SetUpTempDownloadsDir();
  ASSERT_TRUE(StartTestServer());
  ASSERT_TRUE(RunExtensionTest("downloads")) << message_;
}
