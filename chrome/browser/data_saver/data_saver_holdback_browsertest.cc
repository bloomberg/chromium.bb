// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// Tests if the save data header holdback works as expected.
class DataSaverHoldbackBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kDataSaverHoldback);
    InProcessBrowserTest::SetUp();
  }

  void EnableDataSaver(bool enabled) {
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kDataSaverEnabled, enabled);
    content::RunAllPendingInMessageLoop();
  }

  void VerifySaveDataHeader(const std::string& expected_header_value) {
    ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL("/echoheader?Save-Data"));
    std::string header_value;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send(document.body.textContent);",
        &header_value));
    EXPECT_EQ(expected_header_value, header_value);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DataSaverHoldbackBrowserTest,
                       DataSaverEnabledWithHoldbackEnabled) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EnableDataSaver(true);
  VerifySaveDataHeader("None");
}
