// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ui/zoom/page_zoom.h"
#include "components/ui/zoom/test/zoom_test_utils.h"
#include "components/ui/zoom/zoom_controller.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "extensions/test/result_catcher.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#endif

const int kNumberLoadTestParts = 10;

bool GetGuestCallback(content::WebContents** guest_out,
                      content::WebContents* guest) {
  EXPECT_FALSE(*guest_out);
  *guest_out = guest;
  // Return false so that we iterate through all the guests and verify there is
  // only one.
  return false;
}

class PDFExtensionTest : public ExtensionApiTest,
                         public testing::WithParamInterface<int> {
 public:
  ~PDFExtensionTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kDisablePdfMaterialUI);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    ExtensionApiTest::TearDownOnMainThread();
  }

  bool PdfIsExpectedToFailLoad(const std::string& pdf_file) {
    const char* const kFailingPdfs[] = {
        // TODO(thestig): Investigate why this file doesn't fail when served by
        // EmbeddedTestServer or another webserver.
        // "pdf_private/cfuzz5.pdf",
        "pdf_private/cfuzz6.pdf", "pdf_private/crash-11-14-44.pdf",
        "pdf_private/js.pdf",     "pdf_private/segv-ecx.pdf",
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
  void RunTestsInFile(const std::string& filename,
                      const std::string& pdf_filename) {
    extensions::ResultCatcher catcher;

    GURL url(embedded_test_server()->GetURL("/pdf/" + pdf_filename));

    // It should be good enough to just navigate to the URL. But loading up the
    // BrowserPluginGuest seems to happen asynchronously as there was flakiness
    // being seen due to the BrowserPluginGuest not being available yet (see
    // crbug.com/498077). So instead use |LoadPdf| which ensures that the PDF is
    // loaded before continuing.
    content::WebContents* guest_contents = LoadPdfGetGuestContents(url);
    ASSERT_TRUE(guest_contents);

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
    return pdf_extension_test_util::EnsurePDFHasLoaded(web_contents);
  }

  // Same as |LoadPdf|, but also returns a pointer to the guest WebContents for
  // the loaded PDF. Returns nullptr if the load fails.
  content::WebContents* LoadPdfGetGuestContents(const GURL& url) {
    if (!LoadPdf(url))
      return nullptr;

    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::BrowserPluginGuestManager* guest_manager =
        contents->GetBrowserContext()->GetGuestManager();
    content::WebContents* guest_contents =
        guest_manager->GetFullPageGuest(contents);
    return guest_contents;
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

  void TestGetSelectedTextReply(GURL url, bool expect_success) {
    ui_test_utils::NavigateToURL(browser(), url);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));

    // Reach into the guest and hook into it such that it posts back a 'flush'
    // message after every getSelectedTextReply message sent.
    content::BrowserPluginGuestManager* guest_manager =
        web_contents->GetBrowserContext()->GetGuestManager();
    content::WebContents* guest_contents = nullptr;
    ASSERT_NO_FATAL_FAILURE(guest_manager->ForEachGuest(
        web_contents, base::Bind(&GetGuestCallback, &guest_contents)));
    ASSERT_TRUE(guest_contents);
    ASSERT_TRUE(content::ExecuteScript(
        guest_contents,
        "var oldSendScriptingMessage = "
        "    PDFViewer.prototype.sendScriptingMessage_;"
        "PDFViewer.prototype.sendScriptingMessage_ = function(message) {"
        "  oldSendScriptingMessage.bind(this)(message);"
        "  if (message.type == 'getSelectedTextReply')"
        "    this.parentWindow_.postMessage('flush', '*');"
        "}"));

    // Add an event listener for flush messages and request the selected text.
    // If we get a flush message without receiving getSelectedText we know that
    // the message didn't come through.
    bool success = false;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents,
        "window.addEventListener('message', function(event) {"
        "  if (event.data == 'flush')"
        "    window.domAutomationController.send(false);"
        "  if (event.data.type == 'getSelectedTextReply')"
        "    window.domAutomationController.send(true);"
        "});"
        "document.getElementsByTagName('embed')[0].postMessage("
        "    {type: 'getSelectedText'});",
        &success));
    ASSERT_EQ(expect_success, success);
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

class DisablePluginHelper : public content::DownloadManager::Observer,
                            public content::NotificationObserver {
 public:
  DisablePluginHelper() {}

  virtual ~DisablePluginHelper() {}

  void DisablePlugin(Profile* profile) {
    registrar_.Add(this, chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED,
                   content::Source<Profile>(profile));
    scoped_refptr<PluginPrefs> prefs(PluginPrefs::GetForProfile(profile));
    DCHECK(prefs.get());
    prefs->EnablePluginGroup(
        false, base::UTF8ToUTF16(ChromeContentClient::kPDFPluginName));
    // Wait until the plugin has been disabled.
    disable_run_loop_.Run();
  }

  const GURL& GetLastUrl() {
    // Wait until the download has been created.
    download_run_loop_.Run();
    return last_url_;
  }

  // content::DownloadManager::Observer implementation.
  void OnDownloadCreated(content::DownloadManager* manager,
                         content::DownloadItem* item) override {
    last_url_ = item->GetURL();
    download_run_loop_.Quit();
  }

  // content::NotificationObserver implementation.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    DCHECK_EQ(chrome::NOTIFICATION_PLUGIN_ENABLE_STATUS_CHANGED, type);
    disable_run_loop_.Quit();
  }

 private:
  content::NotificationRegistrar registrar_;
  base::RunLoop disable_run_loop_;
  base::RunLoop download_run_loop_;
  GURL last_url_;
};

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, DisablePlugin) {
  // Disable the PDF plugin.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  DisablePluginHelper helper;
  helper.DisablePlugin(profile);

  // Register a download observer.
  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(browser_context);
  download_manager->AddObserver(&helper);

  // Navigate to a PDF and test that it is downloaded.
  GURL url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  ui_test_utils::NavigateToURL(browser(), url);
  ASSERT_EQ(url, helper.GetLastUrl());

  // Cancel the download to shutdown cleanly.
  download_manager->RemoveObserver(&helper);
  std::vector<content::DownloadItem*> downloads;
  download_manager->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  downloads[0]->Cancel(false);
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

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Title) {
  RunTestsInFile("title_test.js", "test-title.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, WhitespaceTitle) {
  RunTestsInFile("whitespace_title_test.js", "test-whitespace-title.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PageChange) {
  RunTestsInFile("page_change_test.js", "test-bookmarks.pdf");
}

// Ensure that the internal PDF plugin application/x-google-chrome-pdf won't be
// loaded if it's not loaded in the chrome extension page.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, EnsureInternalPluginDisabled) {
  std::string url = embedded_test_server()->GetURL("/pdf/test.pdf").spec();
  std::string data_url =
      "data:text/html,"
      "<html><body>"
      "<embed type=\"application/x-google-chrome-pdf\" src=\"" +
      url +
      "\">"
      "</body></html>";
  ui_test_utils::NavigateToURL(browser(), GURL(data_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  bool plugin_loaded = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "var plugin_loaded = "
      "    document.getElementsByTagName('embed')[0].postMessage !== undefined;"
      "window.domAutomationController.send(plugin_loaded);",
      &plugin_loaded));
  ASSERT_FALSE(plugin_loaded);
}

// Ensure cross-origin replies won't work for getSelectedText.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, EnsureCrossOriginRepliesBlocked) {
  std::string url = embedded_test_server()->GetURL("/pdf/test.pdf").spec();
  std::string data_url =
      "data:text/html,"
      "<html><body>"
      "<embed type=\"application/pdf\" src=\"" +
      url +
      "\">"
      "</body></html>";
  TestGetSelectedTextReply(GURL(data_url), false);
}

// Ensure same-origin replies do work for getSelectedText.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, EnsureSameOriginRepliesAllowed) {
  TestGetSelectedTextReply(embedded_test_server()->GetURL("/pdf/test.pdf"),
                           true);
}

// This test ensures that link permissions are enforced properly in PDFs.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, LinkPermissions) {
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  // chrome://favicon links should be allowed for PDFs, while chrome://settings
  // links should not.
  GURL valid_link_url("chrome://favicon/https://www.google.ca/");
  GURL invalid_link_url("chrome://settings");

  GURL unfiltered_valid_link_url(valid_link_url);
  content::RenderProcessHost* rph = guest_contents->GetRenderProcessHost();
  rph->FilterURL(true, &valid_link_url);
  rph->FilterURL(true, &invalid_link_url);

  // Invalid link URLs should be changed to "about:blank" when filtered.
  EXPECT_EQ(unfiltered_valid_link_url, valid_link_url);
  EXPECT_EQ(GURL("about:blank"), invalid_link_url);
}

// This test ensures that titles are set properly for PDFs without /Title.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, TabTitleWithNoTitle) {
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);
  EXPECT_EQ(base::ASCIIToUTF16("test.pdf"), guest_contents->GetTitle());
  EXPECT_EQ(base::ASCIIToUTF16("test.pdf"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
}

// This test ensures that titles are set properly for PDFs with /Title.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, TabTitleWithTitle) {
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test-title.pdf"));
  content::WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);
  EXPECT_EQ(base::ASCIIToUTF16("PDF title test"), guest_contents->GetTitle());
  EXPECT_EQ(base::ASCIIToUTF16("PDF title test"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetTitle());
}

// This test ensures that titles are set properly for embedded PDFs with /Title.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, TabTitleWithEmbeddedPdf) {
  std::string url =
      embedded_test_server()->GetURL("/pdf/test-title.pdf").spec();
  std::string data_url =
      "data:text/html,"
      "<html><head><title>TabTitleWithEmbeddedPdf</title></head><body>"
      "<embed type=\"application/pdf\" src=\"" +
      url +
      "\"></body></html>";
  ui_test_utils::NavigateToURL(browser(), GURL(data_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));
  EXPECT_EQ(base::ASCIIToUTF16("TabTitleWithEmbeddedPdf"),
            web_contents->GetTitle());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfZoomWithoutBubble) {
  using namespace ui_zoom;

  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // The PDF viewer always starts at default zoom, which for tests is 100% or
  // zoom level 0.0. Here we look at the presets to find the next zoom level
  // above 0. Ideally we should look at the zoom levels from the PDF viewer
  // javascript, but we assume they'll always match the browser presets, which
  // are easier to access.
  std::vector<double> preset_zoom_levels = PageZoom::PresetZoomLevels(0.0);
  std::vector<double>::iterator it =
      std::find(preset_zoom_levels.begin(), preset_zoom_levels.end(), 0.0);
  ASSERT_TRUE(it != preset_zoom_levels.end());
  it++;
  ASSERT_TRUE(it != preset_zoom_levels.end());
  double new_zoom_level = *it;

  auto zoom_controller = ZoomController::FromWebContents(web_contents);
  // We expect a ZoomChangedEvent with can_show_bubble == false if the PDF
  // extension behaviour is properly picked up. The test times out otherwise.
  ZoomChangedWatcher watcher(zoom_controller,
                             ZoomController::ZoomChangedEventData(
                                 web_contents, 0.f, new_zoom_level,
                                 ZoomController::ZOOM_MODE_MANUAL, false));

  // Zoom PDF via script.
#if defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
  EXPECT_EQ(nullptr, ZoomBubbleView::GetZoomBubble());
#endif
  ASSERT_TRUE(
      content::ExecuteScript(guest_contents, "viewer.viewport.zoomIn();"));

  watcher.Wait();
#if defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
  EXPECT_EQ(nullptr, ZoomBubbleView::GetZoomBubble());
#endif
}

class MaterialPDFExtensionTest : public PDFExtensionTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnablePdfMaterialUI);
  }
};

IN_PROC_BROWSER_TEST_F(MaterialPDFExtensionTest, Basic) {
  RunTestsInFile("basic_test_material.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(MaterialPDFExtensionTest, BasicPlugin) {
  RunTestsInFile("basic_plugin_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(MaterialPDFExtensionTest, Viewport) {
  RunTestsInFile("viewport_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(MaterialPDFExtensionTest, Bookmark) {
  RunTestsInFile("bookmarks_test.js", "test-bookmarks.pdf");
}

IN_PROC_BROWSER_TEST_F(MaterialPDFExtensionTest, Navigator) {
  RunTestsInFile("navigator_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(MaterialPDFExtensionTest, ParamsParser) {
  RunTestsInFile("params_parser_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(MaterialPDFExtensionTest, ZoomManager) {
  RunTestsInFile("zoom_manager_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(MaterialPDFExtensionTest, Elements) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInFile("material_elements_test.js", "test.pdf");
}

// TODO(tsergeant): Verify that flake is fixed. http://crbug.com/550015
IN_PROC_BROWSER_TEST_F(MaterialPDFExtensionTest, ToolbarManager) {
  RunTestsInFile("toolbar_manager_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(MaterialPDFExtensionTest, Title) {
  RunTestsInFile("title_test.js", "test-title.pdf");
}

IN_PROC_BROWSER_TEST_F(MaterialPDFExtensionTest, WhitespaceTitle) {
  RunTestsInFile("whitespace_title_test.js", "test-whitespace-title.pdf");
}

IN_PROC_BROWSER_TEST_F(MaterialPDFExtensionTest, PageChange) {
  RunTestsInFile("page_change_test.js", "test-bookmarks.pdf");
}
