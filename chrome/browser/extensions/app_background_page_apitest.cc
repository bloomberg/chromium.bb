// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "chrome/browser/background_contents_service.h"
#include "chrome/browser/background_contents_service_factory.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"

class AppBackgroundPageApiTest : public ExtensionApiTest {
 public:
  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kDisablePopupBlocking);
    command_line->AppendSwitch(switches::kAllowHTTPBackgroundPage);
  }

  bool CreateApp(const std::string& app_manifest,
                 FilePath* app_dir) {
    if (!app_dir_.CreateUniqueTempDir()) {
      LOG(ERROR) << "Unable to create a temporary directory.";
      return false;
    }
    FilePath manifest_path = app_dir_.path().AppendASCII("manifest.json");
    int bytes_written = file_util::WriteFile(manifest_path,
                                             app_manifest.data(),
                                             app_manifest.size());
    if (bytes_written != static_cast<int>(app_manifest.size())) {
      LOG(ERROR) << "Unable to write complete manifest to file. Return code="
                 << bytes_written;
      return false;
    }
    *app_dir = app_dir_.path();
    return true;
  }

 private:
  ScopedTempDir app_dir_;
};

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, Basic) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());

  std::string app_manifest = StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%d/\""
      "    }"
      "  },"
      "  \"permissions\": [\"background\"]"
      "}",
      test_server()->host_port_pair().port());

  FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));
  ASSERT_TRUE(RunExtensionTest("app_background_page/basic")) << message_;
}

// Crashy, http://crbug.com/49215.
IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, DISABLED_LacksPermission) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());

  std::string app_manifest = StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%d/\""
      "    }"
      "  }"
      "}",
      test_server()->host_port_pair().port());

  FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));
  ASSERT_TRUE(RunExtensionTest("app_background_page/lacks_permission"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AppBackgroundPageApiTest, ManifestBackgroundPage) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  ASSERT_TRUE(StartTestServer());

  std::string app_manifest = StringPrintf(
      "{"
      "  \"name\": \"App\","
      "  \"version\": \"0.1\","
      "  \"app\": {"
      "    \"urls\": ["
      "      \"http://a.com/\""
      "    ],"
      "    \"launch\": {"
      "      \"web_url\": \"http://a.com:%d/\""
      "    }"
      "  },"
      "  \"permissions\": [\"background\"],"
      "  \"background_page\": \"http://a.com:%d/test.html\""
      "}",
      test_server()->host_port_pair().port(),
      test_server()->host_port_pair().port());

  FilePath app_dir;
  ASSERT_TRUE(CreateApp(app_manifest, &app_dir));
  ASSERT_TRUE(LoadExtension(app_dir));

  const Extension* extension = GetSingleLoadedExtension();
  ASSERT_TRUE(
      BackgroundContentsServiceFactory::GetForProfile(browser()->profile())->
          GetAppBackgroundContents(ASCIIToUTF16(extension->id())));
}
