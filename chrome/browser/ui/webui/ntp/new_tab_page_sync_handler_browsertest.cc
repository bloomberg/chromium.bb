// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/new_tab_page_sync_handler.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using ::testing::_;

typedef InProcessBrowserTest NewTabPageSyncHandlerBrowserTest;

class MockNewTabPageSyncHandler : public NewTabPageSyncHandler {
 public:
  MOCK_METHOD3(SendSyncMessageToPage,
               void(MessageType type,
                    const std::string& msg,
                    const std::string& linktext));

  void SetWaitingForInitialPageLoad(bool waiting) {
    waiting_for_initial_page_load_ = waiting;
  }
};

// TODO(samarth): Delete along with the rest of the NTP4 code.
IN_PROC_BROWSER_TEST_F(NewTabPageSyncHandlerBrowserTest,
                       DISABLED_ChangeSigninAllowedToFalse) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  content::WebUI* web_ui =
      browser()->tab_strip_model()->GetActiveWebContents()->GetWebUI();
  MockNewTabPageSyncHandler* mock_handler = new MockNewTabPageSyncHandler();
  mock_handler->SetWaitingForInitialPageLoad(false);
  web_ui->AddMessageHandler(mock_handler);
  EXPECT_CALL(*mock_handler, SendSyncMessageToPage(mock_handler->HIDE, _, _));
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);
}
