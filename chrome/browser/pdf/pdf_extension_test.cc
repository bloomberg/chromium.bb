// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/hash.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "extensions/test/result_catcher.h"
#include "grit/component_extension_resources.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/resource/resource_bundle.h"

const int kNumberLoadTestParts = 10;

class PDFExtensionTest : public ExtensionApiTest,
                         public testing::WithParamInterface<int> {
 public:
  ~PDFExtensionTest() override {}

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    ExtensionApiTest::TearDownOnMainThread();
  }

  bool PdfIsExpectedToFailLoad(const std::string& pdf_file) {
    const char* kFailingPdfs[] = {
        "pdf_private/cfuzz5.pdf",
        "pdf_private/cfuzz6.pdf",
        "pdf_private/crash-11-14-44.pdf",
        "pdf_private/js.pdf",
        "pdf_private/segv-ecx.pdf",
        "pdf_private/tests.pdf",
    };
    for (size_t i = 0; i < arraysize(kFailingPdfs); ++i) {
      if (kFailingPdfs[i] == pdf_file)
        return true;
    }
    return false;
  }

  // Runs the extensions test at chrome/test/data/pdf/<filename> on the PDF file
  // at chrome/test/data/pdf/<pdf_filename>.
  void RunTestsInFile(std::string filename, std::string pdf_filename) {
    extensions::ResultCatcher catcher;

    GURL url(embedded_test_server()->GetURL("/pdf/" + pdf_filename));
    ui_test_utils::NavigateToURL(browser(), url);

    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::BrowserPluginGuestManager* guest_manager =
        contents->GetBrowserContext()->GetGuestManager();
    content::WebContents* guest_contents =
        guest_manager->GetFullPageGuest(contents);
    ASSERT_TRUE(guest_contents);
    EXPECT_TRUE(content::WaitForLoadStop(guest_contents));

    base::FilePath test_data_dir;
    PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    test_data_dir = test_data_dir.Append(FILE_PATH_LITERAL("pdf"));
    base::FilePath test_util_path = test_data_dir.AppendASCII("test_util.js");
    std::string test_util_js;
    ASSERT_TRUE(base::ReadFileToString(test_util_path, &test_util_js));

    base::FilePath test_file_path = test_data_dir.AppendASCII(filename);
    std::string test_js;
    ASSERT_TRUE(base::ReadFileToString(test_file_path, &test_js));

    test_util_js.append(test_js);
    ASSERT_TRUE(content::ExecuteScript(guest_contents, test_util_js));

    if (!catcher.GetNextResult())
      FAIL() << catcher.message();
  }

  // Load the PDF at the given URL and use the PDFScriptingAPI to ensure it has
  // finished loading. Return true if it loads successfully or false if it
  // fails. If it doesn't finish loading the test will hang. This is done from
  // outside of the BrowserPlugin guest to ensure the PDFScriptingAPI works
  // correctly from there.
  bool LoadPdf(const GURL& url) {
    ui_test_utils::NavigateToURL(browser(), url);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::string scripting_api_js =
        ResourceBundle::GetSharedInstance()
            .GetRawDataResource(IDR_PDF_PDF_SCRIPTING_API_JS)
            .as_string();
    CHECK(content::ExecuteScript(web_contents, scripting_api_js));

    bool load_success = false;
    CHECK(content::ExecuteScriptAndExtractBool(
        web_contents,
        "var scriptingAPI = new PDFScriptingAPI(window, "
        "    document.getElementsByTagName('embed')[0]);"
        "scriptingAPI.setLoadCallback(function(success) {"
        "  window.domAutomationController.send(success);"
        "});",
        &load_success));
    return load_success;
  }

  // Load all the PDFs contained in chrome/test/data/<dir_name>. This only runs
  // the test if base::Hash(filename) mod kNumberLoadTestParts == k in order
  // to shard the files evenly across values of k in [0, kNumberLoadTestParts).
  void LoadAllPdfsTest(const std::string& dir_name, int k) {
    base::FilePath test_data_dir;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
    base::FileEnumerator file_enumerator(test_data_dir.AppendASCII(dir_name),
                                         false, base::FileEnumerator::FILES,
                                         FILE_PATH_LITERAL("*.pdf"));

    size_t count = 0;
    for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
         file_path = file_enumerator.Next()) {
      std::string filename = file_path.BaseName().MaybeAsASCII();
      ASSERT_FALSE(filename.empty());

      std::string pdf_file = dir_name + "/" + filename;
      if (static_cast<int>(base::Hash(filename) % kNumberLoadTestParts) == k) {
        LOG(INFO) << "Loading: " << pdf_file;
        bool success = LoadPdf(embedded_test_server()->GetURL("/" + pdf_file));
        EXPECT_EQ(!PdfIsExpectedToFailLoad(pdf_file), success);
      }
      ++count;
    }
    // Assume that there is at least 1 pdf in the directory to guard against
    // someone deleting the directory and silently making the test pass.
    ASSERT_GE(count, 1u);
  }
};

IN_PROC_BROWSER_TEST_P(PDFExtensionTest, Load) {
#if defined(GOOGLE_CHROME_BUILD)
  // Load private PDFs.
  LoadAllPdfsTest("pdf_private", GetParam());
#endif
  // Load public PDFs.
  LoadAllPdfsTest("pdf", GetParam());
}

// We break PDFTest.Load up into kNumberLoadTestParts.
INSTANTIATE_TEST_CASE_P(PDFTestFiles,
                        PDFExtensionTest,
                        testing::Range(0, kNumberLoadTestParts));

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Basic) {
  RunTestsInFile("basic_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, BasicPlugin) {
  RunTestsInFile("basic_plugin_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Viewport) {
  RunTestsInFile("viewport_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Bookmark) {
  RunTestsInFile("bookmarks_test.js", "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Navigator) {
  RunTestsInFile("navigator_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, ParamsParser) {
  RunTestsInFile("params_parser_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, ZoomManager) {
  RunTestsInFile("zoom_manager_test.js", "test.pdf");
}
