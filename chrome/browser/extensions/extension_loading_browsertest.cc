// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains tests for extension loading, reloading, and
// unloading behavior.

#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/safe_numerics.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_creator.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace extensions {
namespace {

// Provides a temporary directory to build an extension into.  This lets all of
// an extension's code live inside the test instead of in a separate directory.
class TestExtensionDir {
 public:
  TestExtensionDir() {
    EXPECT_TRUE(dir_.CreateUniqueTempDir());
    EXPECT_TRUE(crx_dir_.CreateUniqueTempDir());
  }

  // Writes |contents| to path()/filename, overwriting anything that was already
  // there.
  void WriteFile(base::FilePath::StringType filename,
                 base::StringPiece contents) {
    EXPECT_EQ(
        base::checked_numeric_cast<int>(contents.size()),
        file_util::WriteFile(
            dir_.path().Append(filename), contents.data(), contents.size()));
  }

  // Converts |value| to JSON, and then writes it to path()/filename with
  // WriteFile().
  void WriteJson(base::FilePath::StringType filename, const Value& value) {
    std::string json;
    base::JSONWriter::Write(&value, &json);
    WriteFile(filename, json);
  }

  // This function packs the extension into a .crx, and returns the path to that
  // .crx. Multiple calls to Pack() will produce extensions with the same ID.
  base::FilePath Pack() {
    ExtensionCreator creator;
    base::FilePath crx_path =
        crx_dir_.path().Append(FILE_PATH_LITERAL("ext.crx"));
    base::FilePath pem_path =
        crx_dir_.path().Append(FILE_PATH_LITERAL("ext.pem"));
    base::FilePath pem_in_path, pem_out_path;
    if (file_util::PathExists(pem_path))
      pem_in_path = pem_path;
    else
      pem_out_path = pem_path;
    if (!creator.Run(dir_.path(),
                     crx_path,
                     pem_in_path,
                     pem_out_path,
                     ExtensionCreator::kOverwriteCRX)) {
      ADD_FAILURE()
          << "ExtensionCreator::Run() failed: " << creator.error_message();
      return base::FilePath();
    }
    if (!file_util::PathExists(crx_path)) {
      ADD_FAILURE() << crx_path.value() << " was not created.";
      return base::FilePath();
    }
    return crx_path;
  }

 private:
  // Stores files that make up the extension.
  base::ScopedTempDir dir_;
  // Stores the generated .crx and .pem.
  base::ScopedTempDir crx_dir_;
};

class ExtensionLoadingTest : public ExtensionBrowserTest {
};

// Check the fix for http://crbug.com/178542.
IN_PROC_BROWSER_TEST_F(ExtensionLoadingTest,
                       UpgradeAfterNavigatingFromOverriddenNewTabPage) {
  embedded_test_server()->ServeFilesFromDirectory(
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());

  TestExtensionDir extension_dir;
  scoped_ptr<base::Value> manifest =
      base::test::ParseJson("{"
                            "  \"name\": \"Overrides New Tab\","
                            "  \"version\": \"1\","
                            "  \"description\": \"Overrides New Tab\","
                            "  \"manifest_version\": 2,"
                            "  \"background\": {"
                            "    \"persistent\": false,"
                            "    \"scripts\": [\"event.js\"]"
                            "  },"
                            "  \"chrome_url_overrides\": {"
                            "    \"newtab\": \"newtab.html\""
                            "  }"
                            "}");
  extension_dir.WriteJson(FILE_PATH_LITERAL("manifest.json"), *manifest);
  extension_dir.WriteFile(FILE_PATH_LITERAL("event.js"), "");
  extension_dir.WriteFile(FILE_PATH_LITERAL("newtab.html"),
                          "<h1>Overridden New Tab Page</h1>");

  const Extension* new_tab_extension =
      InstallExtension(extension_dir.Pack(), 1 /*new install*/);
  ASSERT_TRUE(new_tab_extension);

  // Visit the New Tab Page to get a renderer using the extension into history.
  ui_test_utils::NavigateToURL(browser(), GURL("chrome://newtab"));

  // Navigate that tab to a non-extension URL to swap out the extension's
  // renderer.
  const GURL test_link_from_NTP =
      embedded_test_server()->GetURL("/README.chromium");
  EXPECT_THAT(test_link_from_NTP.spec(), testing::EndsWith("/README.chromium"))
      << "Check that the test server started.";
  NavigateInRenderer(browser()->tab_strip_model()->GetActiveWebContents(),
                     test_link_from_NTP);

  // Increase the extension's version.
  static_cast<base::DictionaryValue&>(*manifest).SetString("version", "2");
  extension_dir.WriteJson(FILE_PATH_LITERAL("manifest.json"), *manifest);

  // Upgrade the extension.
  new_tab_extension = UpdateExtension(
      new_tab_extension->id(), extension_dir.Pack(), 0 /*expected upgrade*/);
  EXPECT_THAT(new_tab_extension->version()->components(),
              testing::ElementsAre(2));

  // The extension takes a couple round-trips to the renderer in order
  // to crash, so open a new tab to wait long enough.
  AddTabAtIndex(browser()->tab_strip_model()->count(),
                GURL("http://www.google.com/"),
                content::PAGE_TRANSITION_TYPED);

  // Check that the extension hasn't crashed.
  ExtensionService* service =
      ExtensionSystem::Get(profile())->extension_service();
  EXPECT_EQ(0U, service->terminated_extensions()->size());
  EXPECT_TRUE(service->extensions()->Contains(new_tab_extension->id()));
}

}  // namespace
}  // namespace extensions
