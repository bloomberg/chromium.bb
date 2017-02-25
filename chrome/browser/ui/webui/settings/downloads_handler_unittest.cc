// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/downloads_handler.h"

#include "chrome/browser/download/chrome_download_manager_delegate.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/mock_download_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace settings {

class DownloadsHandlerTest : public testing::Test {
 public:
  DownloadsHandlerTest()
      : download_manager_(new content::MockDownloadManager()),
        chrome_download_manager_delegate_(&profile_),
        handler_(&profile_) {
    content::BrowserContext::SetDownloadManagerForTesting(&profile_,
                                                          download_manager_);
    EXPECT_EQ(download_manager_,
              content::BrowserContext::GetDownloadManager(&profile_));

    EXPECT_CALL(*download_manager_, GetDelegate())
        .WillRepeatedly(testing::Return(&chrome_download_manager_delegate_));
    EXPECT_CALL(*download_manager_, Shutdown());

    handler_.set_web_ui(&test_web_ui_);
  }

  void SetUp() override {
    EXPECT_TRUE(test_web_ui_.call_data().empty());

    base::ListValue args;
    handler()->HandleInitialize(&args);

    EXPECT_TRUE(handler()->IsJavascriptAllowed());
    VerifyAutoOpenDownloadsChangedCallback();

    test_web_ui_.ClearTrackedCalls();
  }

  void TearDown() override {
    chrome_download_manager_delegate_.Shutdown();
    testing::Test::TearDown();
  }

  void VerifyAutoOpenDownloadsChangedCallback() {
    EXPECT_EQ(1u, test_web_ui_.call_data().size());

    auto& data = *(test_web_ui_.call_data().back());
    EXPECT_EQ("cr.webUIListenerCallback", data.function_name());
    std::string event;
    ASSERT_TRUE(data.arg1()->GetAsString(&event));
    EXPECT_EQ("auto-open-downloads-changed", event);
    bool auto_open_downloads = false;
    ASSERT_TRUE(data.arg2()->GetAsBoolean(&auto_open_downloads));
    EXPECT_FALSE(auto_open_downloads);
  }

  Profile* profile() { return &profile_; }
  DownloadsHandler* handler() { return &handler_; }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  content::TestWebUI test_web_ui_;
  TestingProfile profile_;

  // This is heap allocated because its ownership is transferred to |profile_|.
  content::MockDownloadManager* download_manager_;
  ChromeDownloadManagerDelegate chrome_download_manager_delegate_;

  DownloadsHandler handler_;
};

TEST_F(DownloadsHandlerTest, AutoOpenDownloads) {
  // Touch the pref.
  profile()->GetPrefs()->SetString(prefs::kDownloadExtensionsToOpen, "");
  VerifyAutoOpenDownloadsChangedCallback();
}

}  // namespace settings
