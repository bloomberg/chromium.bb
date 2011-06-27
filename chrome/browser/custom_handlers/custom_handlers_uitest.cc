// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/test/test_file_util.h"
#include "build/build_config.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"
#include "net/test/test_server.h"

namespace {

class CustomHandlerTest : public UITest {
 public:
  CustomHandlerTest()
      : test_server_(net::TestServer::TYPE_HTTP,
                     FilePath(FILE_PATH_LITERAL("chrome/test/data"))) {
    // Stop Chrome from removing custom protocol handlers for protocols that
    // Chrome is not registered with the OS as the default application.
    // In the test environment Chrome will not be registered with the OS as the
    // default application to handle any protocols.
    launch_arguments_.AppendSwitch(switches::kDisableCustomProtocolOSCheck);

    FilePath template_dir;
    EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &template_dir));
    template_dir = template_dir.AppendASCII("profiles").
        AppendASCII("custom_handlers");
    set_template_user_data(template_dir);
  }

  virtual void SetUpProfile() {
    UITest::SetUpProfile();
    FilePath preferences_path = user_data_dir().AppendASCII("Default").
        Append(chrome::kPreferencesFilename);

    std::string preferences;
    ASSERT_TRUE(file_util::ReadFileToString(preferences_path, &preferences));

    // Update the placeholders for our mailto and webcal handlers with
    // their proper URLs.
    preferences = ReplacePlaceholderHandler(preferences,
                                            "MAILTOHANDLER",
                                            "files/custom_handler_mailto.html");
    preferences = ReplacePlaceholderHandler(preferences,
                                            "WEBCALHANDLER",
                                            "files/custom_handler_webcal.html");

    // Write the updated preference file to our temporary profile.
    ASSERT_TRUE(file_util::WriteFile(preferences_path,
                                     preferences.c_str(),
                                     preferences.size()));
  }

  virtual void SetUp() {
    UITest::SetUp();

    window_ = automation()->GetBrowserWindow(0);
    ASSERT_TRUE(window_.get());

    tab_ = window_->GetActiveTab();
    ASSERT_TRUE(tab_.get());
  }

 protected:
  net::TestServer test_server_;

  GURL GetTabURL() {
    GURL url;
    EXPECT_TRUE(tab_->GetCurrentURL(&url));
    return url;
  }

 private:
  std::string ReplacePlaceholderHandler(const std::string& preferences,
                                        const std::string& placeholder,
                                        const std::string& handler_path) {
    GURL handler_url = test_server_.GetURL(handler_path);
    std::string result = preferences;
    size_t found = result.find(placeholder);
    EXPECT_FALSE(found == std::string::npos);
    if (found != std::string::npos)
      result.replace(found, placeholder.length(), handler_url.spec());
    return result;
  }

  scoped_refptr<BrowserProxy> window_;
  scoped_refptr<TabProxy> tab_;
};

class CustomHandlerMailStartupTest : public CustomHandlerTest {
 public:
  CustomHandlerMailStartupTest() {
    launch_arguments_.AppendArg("mailto:test");
  }
};

class CustomHandlerWebcalStartupTest : public CustomHandlerTest {
 public:
  CustomHandlerWebcalStartupTest() {
    launch_arguments_.AppendArg("webcal:test");
  }
};

#if defined(OS_LINUX)
// See http://crbug.com/83557
#define MAYBE_CustomHandlerMailStartup FAILS_CustomHandlerMailStartup
#define MAYBE_CustomHandlerWebcalStartup FAILS_CustomHandlerWebcalStartup
#else
#define MAYBE_CustomHandlerMailStartup CustomHandlerMailStartup
#define MAYBE_CustomHandlerWebcalStartup CustomHandlerWebcalStartup
#endif

TEST_F(CustomHandlerMailStartupTest, MAYBE_CustomHandlerMailStartup) {
  ASSERT_EQ(test_server_.GetURL("files/custom_handler_mailto.html"),
            GetTabURL());
}

TEST_F(CustomHandlerWebcalStartupTest, MAYBE_CustomHandlerWebcalStartup) {
  ASSERT_EQ(test_server_.GetURL("files/custom_handler_webcal.html"),
            GetTabURL());
}

}  // namespace
