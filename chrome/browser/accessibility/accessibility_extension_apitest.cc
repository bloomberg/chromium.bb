// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/infobars/simple_alert_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/test_switches.h"
#include "extensions/common/switches.h"

// Times out on win syzyasan, http://crbug.com/166026
#if defined(SYZYASAN)
#define MAYBE_GetAlertsForTab DISABLED_GetAlertsForTab
#else
#define MAYBE_GetAlertsForTab GetAlertsForTab
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_GetAlertsForTab) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kAshBrowserTests))
    return;
#endif

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  ASSERT_TRUE(infobar_service);

  const char kAlertMessage[] = "Simple Alert Infobar.";
  SimpleAlertInfoBarDelegate::Create(infobar_service,
                                     infobars::InfoBarDelegate::kNoIconID,
                                     base::ASCIIToUTF16(kAlertMessage), false);
  CommandLine::ForCurrentProcess()->AppendSwitch(
      extensions::switches::kEnableExperimentalExtensionApis);
  ASSERT_TRUE(RunComponentExtensionTest("accessibility/get_alerts_for_tab"))
      << message_;
}
