// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/tab_contents/simple_alert_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_switches.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, GetAlertsForTab) {
  TabContents* tab = chrome::GetActiveTabContents(browser());
  ASSERT_TRUE(tab);
  InfoBarTabHelper* infobar_helper = tab->infobar_tab_helper();

  const char kAlertMessage[] = "Simple Alert Infobar.";
  infobar_helper->AddInfoBar(
      new SimpleAlertInfoBarDelegate(infobar_helper,
                                     NULL,
                                     ASCIIToUTF16(kAlertMessage),
                                     false));
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  ASSERT_TRUE(RunExtensionTest("accessibility/get_alerts_for_tab")) << message_;
}
