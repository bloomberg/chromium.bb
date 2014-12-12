// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_handlers/content_capabilities_handler.h"
#include "extensions/common/switches.h"
#include "extensions/common/url_pattern.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using extensions::DictionaryBuilder;
using extensions::Extension;
using extensions::ExtensionBuilder;
using extensions::ListBuilder;

class ContentCapabilitiesTest : public ExtensionApiTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID,
        crx_file::id_util::GenerateIdForPath(
            base::MakeAbsoluteFilePath(test_extension_dir_.unpacked_path())));
  }

  // Builds an extension manifest with the given content_capabilities matches
  // and permissions. The extension always has the same (whitelisted) ID.
  scoped_refptr<const Extension> LoadExtensionWithCapabilities(
      const std::string& matches,
      const std::string& permissions) {
    std::string manifest = base::StringPrintf(
        "{\n"
        "  \"name\": \"content_capabilities test extensions\",\n"
        "  \"version\": \"1\",\n"
        "  \"manifest_version\": 2,\n"
        "  \"content_capabilities\": {\n"
        "    \"matches\": %s,\n"
        "    \"permissions\": %s\n"
        "  }\n"
        "}\n",
        matches.c_str(), permissions.c_str());
    test_extension_dir_.WriteManifest(manifest);
    return LoadExtension(test_extension_dir_.unpacked_path());
  }

  std::string MakeJSONList(const std::string& s0 = "",
                           const std::string& s1 = "",
                           const std::string& s2 = "") {
    std::vector<std::string> v;
    if (!s0.empty())
      v.push_back(s0);
    if (!s1.empty())
      v.push_back(s1);
    if (!s2.empty())
      v.push_back(s2);
    std::string list = JoinString(v, "\",\"");
    if (!list.empty())
      list = "\"" + list + "\"";
    return "[" + list + "]";
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  GURL GetTestURLFor(const std::string& host) {
    std::string port = base::IntToString(embedded_test_server()->port());
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    replacements.SetPortStr(port);
    return embedded_test_server()
        ->GetURL("/" + host + ".html")
        .ReplaceComponents(replacements);
  }

  void InitializeTestServer() {
    base::FilePath test_data;
    EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data.AppendASCII("extensions/content_capabilities"));
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
    host_resolver()->AddRule("*", embedded_test_server()->base_url().host());
  }

  // Run some script in the context of the given origin and in the presence of
  // the given extension. This is used to wrap calls into the JS test functions
  // defined by
  // $(DIR_TEST_DATA)/extensions/content_capabilities/capability_tests.js.
  bool TestContentCapability(const Extension* extension,
                             const char* origin,
                             const char* code) {
    ui_test_utils::NavigateToURL(browser(), GetTestURLFor(origin));
    bool result = false;
    CHECK(content::ExecuteScriptAndExtractBool(web_contents(), code, &result));
    return result;
  }

  bool CanReadClipboard(const Extension* extension, const char* origin) {
    return TestContentCapability(extension, origin, "tests.canReadClipboard()");
  }

  bool CanWriteClipboard(const Extension* extension, const char* origin) {
    return TestContentCapability(extension, origin,
                                 "tests.canWriteClipboard()");
  }

 private:
  extensions::TestExtensionDir test_extension_dir_;
};

IN_PROC_BROWSER_TEST_F(ContentCapabilitiesTest, NoCapabilities) {
  InitializeTestServer();
  scoped_refptr<const Extension> extension = LoadExtensionWithCapabilities(
      MakeJSONList("http://foo.example.com/*"), MakeJSONList());
  EXPECT_FALSE(CanReadClipboard(extension.get(), "foo.example.com"));
  EXPECT_FALSE(CanWriteClipboard(extension.get(), "foo.example.com"));
}

IN_PROC_BROWSER_TEST_F(ContentCapabilitiesTest, ClipboardRead) {
  InitializeTestServer();
  scoped_refptr<const Extension> extension = LoadExtensionWithCapabilities(
      MakeJSONList("http://foo.example.com/*"), MakeJSONList("clipboardRead"));
  EXPECT_TRUE(CanReadClipboard(extension.get(), "foo.example.com"));
  EXPECT_FALSE(CanReadClipboard(extension.get(), "bar.example.com"));
  EXPECT_FALSE(CanWriteClipboard(extension.get(), "foo.example.com"));
}

IN_PROC_BROWSER_TEST_F(ContentCapabilitiesTest, ClipboardWrite) {
  InitializeTestServer();
  scoped_refptr<const Extension> extension = LoadExtensionWithCapabilities(
      MakeJSONList("http://foo.example.com/*"), MakeJSONList("clipboardWrite"));
  EXPECT_TRUE(CanWriteClipboard(extension.get(), "foo.example.com"));
  EXPECT_FALSE(CanWriteClipboard(extension.get(), "bar.example.com"));
  EXPECT_FALSE(CanReadClipboard(extension.get(), "foo.example.com"));
}

IN_PROC_BROWSER_TEST_F(ContentCapabilitiesTest, ClipboardReadWrite) {
  InitializeTestServer();
  scoped_refptr<const Extension> extension = LoadExtensionWithCapabilities(
      MakeJSONList("http://foo.example.com/*"),
      MakeJSONList("clipboardRead", "clipboardWrite"));
  EXPECT_TRUE(CanReadClipboard(extension.get(), "foo.example.com"));
  EXPECT_TRUE(CanWriteClipboard(extension.get(), "foo.example.com"));
  EXPECT_FALSE(CanReadClipboard(extension.get(), "bar.example.com"));
  EXPECT_FALSE(CanWriteClipboard(extension.get(), "bar.example.com"));
}
