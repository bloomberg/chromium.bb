// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <vector>

#include "base/base_paths.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/pdf/pdf_extension_util.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/test/zoom_test_utils.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/manifest_handlers/mime_types_handler.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

#if defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#endif

const int kNumberLoadTestParts = 10;

#if defined(OS_MACOSX)
const int kDefaultKeyModifier = blink::WebInputEvent::MetaKey;
#else
const int kDefaultKeyModifier = blink::WebInputEvent::ControlKey;
#endif

// Using ASSERT_TRUE deliberately instead of ASSERT_EQ or ASSERT_STREQ
// in order to print a more readable message if the strings differ.
#define ASSERT_MULTILINE_STREQ(expected, actual) \
    ASSERT_TRUE(expected == actual) \
        << "Expected:\n" << expected \
        << "\n\nActual:\n" << actual

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
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    content::SetupCrossSiteRedirector(embedded_test_server());
    embedded_test_server()->StartAcceptingConnections();
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

  void ConvertPageCoordToScreenCoord(content::WebContents* contents,
                                     gfx::Point* point) {
    ASSERT_TRUE(contents);
    ASSERT_TRUE(content::ExecuteScript(contents,
        "var visiblePage = viewer.viewport.getMostVisiblePage();"
        "var visiblePageDimensions ="
        "    viewer.viewport.getPageScreenRect(visiblePage);"
        "var viewportPosition = viewer.viewport.position;"
        "var screenOffsetX = visiblePageDimensions.x - viewportPosition.x;"
        "var screenOffsetY = visiblePageDimensions.y - viewportPosition.y;"
        "var linkScreenPositionX ="
        "    Math.floor(" + base::IntToString(point->x()) + " + screenOffsetX);"
        "var linkScreenPositionY ="
        "    Math.floor(" + base::IntToString(point->y()) + " +"
        "    screenOffsetY);"));

    int x;
    ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
        contents,
        "window.domAutomationController.send(linkScreenPositionX);",
        &x));

    int y;
    ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
        contents,
        "window.domAutomationController.send(linkScreenPositionY);",
        &y));

    point->SetPoint(x, y);
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

class DisablePluginHelper : public content::DownloadManager::Observer {
 public:
  DisablePluginHelper() {}
  ~DisablePluginHelper() override {}

  void DisablePlugin(Profile* profile) {
    profile->GetPrefs()->SetBoolean(
        prefs::kPluginsAlwaysOpenPdfExternally, true);
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

 private:
  content::NotificationRegistrar registrar_;
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

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, GestureDetector) {
  RunTestsInFile("gesture_detector_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, Elements) {
  // Although this test file does not require a PDF to be loaded, loading the
  // elements without loading a PDF is difficult.
  RunTestsInFile("material_elements_test.js", "test.pdf");
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, ToolbarManager) {
  RunTestsInFile("toolbar_manager_test.js", "test.pdf");
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

// Ensure that the PDF component extension cannot be loaded directly.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, BlockDirectAccess) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::unique_ptr<content::ConsoleObserverDelegate> console_delegate(
      new content::ConsoleObserverDelegate(
          web_contents,
          "*Streams are only available from a mime handler view guest.*"));
  web_contents->SetDelegate(console_delegate.get());
  GURL forbiddenUrl(
      "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html?"
      "https://example.com/notrequested.pdf");
  ui_test_utils::NavigateToURL(browser(), forbiddenUrl);

  console_delegate->Wait();
}

// This test ensures that PDF can be loaded from local file
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, EnsurePDFFromLocalFileLoads) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  test_data_dir = test_data_dir.Append(FILE_PATH_LITERAL("pdf"));
  base::FilePath test_data_file = test_data_dir.AppendASCII("test.pdf");
  ASSERT_TRUE(PathExists(test_data_file));
  GURL test_pdf_url("file://" + test_data_file.MaybeAsASCII());
  content::WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);
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
  using namespace zoom;

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

  auto* zoom_controller = ZoomController::FromWebContents(web_contents);
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

static std::string DumpPdfAccessibilityTree(const ui::AXTreeUpdate& ax_tree) {
  // Create a string representation of the tree starting with the embedded
  // object.
  std::string ax_tree_dump;
  base::hash_map<int32_t, int> id_to_indentation;
  bool found_embedded_object = false;
  for (auto& node : ax_tree.nodes) {
    if (node.role == ui::AX_ROLE_EMBEDDED_OBJECT)
      found_embedded_object = true;
    if (!found_embedded_object)
      continue;

    int indent = id_to_indentation[node.id];
    ax_tree_dump += std::string(2 * indent, ' ');
    ax_tree_dump += ui::ToString(node.role);

    std::string name = node.GetStringAttribute(ui::AX_ATTR_NAME);
    base::ReplaceChars(name, "\r", "\\r", &name);
    base::ReplaceChars(name, "\n", "\\n", &name);
    if (!name.empty())
      ax_tree_dump += " '" + name + "'";
    ax_tree_dump += "\n";
    for (size_t j = 0; j < node.child_ids.size(); ++j)
      id_to_indentation[node.child_ids[j]] = indent + 1;
  }

  return ax_tree_dump;
}

static const char kExpectedPDFAXTree[] =
    "embeddedObject\n"
    "  group\n"
    "    region 'Page 1'\n"
    "      paragraph\n"
    "        staticText '1 First Section\\r\\n'\n"
    "          inlineTextBox '1 '\n"
    "          inlineTextBox 'First Section\\r\\n'\n"
    "      paragraph\n"
    "        staticText 'This is the first section.\\r\\n1'\n"
    "          inlineTextBox 'This is the first section.\\r\\n'\n"
    "          inlineTextBox '1'\n"
    "    region 'Page 2'\n"
    "      paragraph\n"
    "        staticText '1.1 First Subsection\\r\\n'\n"
    "          inlineTextBox '1.1 '\n"
    "          inlineTextBox 'First Subsection\\r\\n'\n"
    "      paragraph\n"
    "        staticText 'This is the first subsection.\\r\\n2'\n"
    "          inlineTextBox 'This is the first subsection.\\r\\n'\n"
    "          inlineTextBox '2'\n"
    "    region 'Page 3'\n"
    "      paragraph\n"
    "        staticText '2 Second Section\\r\\n'\n"
    "          inlineTextBox '2 '\n"
    "          inlineTextBox 'Second Section\\r\\n'\n"
    "      paragraph\n"
    "        staticText '3'\n"
    "          inlineTextBox '3'\n";

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfAccessibility) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();

  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  content::WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  WaitForAccessibilityTreeToContainNodeWithName(guest_contents,
                                                "1 First Section\r\n");
  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshot(guest_contents);
  std::string ax_tree_dump = DumpPdfAccessibilityTree(ax_tree);
  ASSERT_MULTILINE_STREQ(kExpectedPDFAXTree, ax_tree_dump);
}

#if defined(GOOGLE_CHROME_BUILD)
// Test a particular PDF encountered in the wild that triggered a crash
// when accessibility is enabled.  (http://crbug.com/648981)
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfAccessibilityCharCountCrash) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  GURL test_pdf_url(embedded_test_server()->GetURL(
      "/pdf_private/accessibility_crash_1.pdf"));

  content::WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  WaitForAccessibilityTreeToContainNodeWithName(guest_contents, "Page 1");
}
#endif

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfAccessibilityEnableLater) {
  // In this test, load the PDF file first, with accessibility off.
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test-bookmarks.pdf"));
  content::WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  // Now enable accessibility globally, and assert that the PDF accessibility
  // tree loads.
  EnableAccessibilityForWebContents(guest_contents);
  WaitForAccessibilityTreeToContainNodeWithName(guest_contents,
                                                "1 First Section\r\n");
  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshot(guest_contents);
  std::string ax_tree_dump = DumpPdfAccessibilityTree(ax_tree);
  ASSERT_MULTILINE_STREQ(kExpectedPDFAXTree, ax_tree_dump);
}

bool RetrieveGuestContents(
    content::WebContents** out_guest_contents,
    content::WebContents* in_guest_contents) {
  *out_guest_contents = in_guest_contents;
  return true;
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfAccessibilityInIframe) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  GURL test_iframe_url(embedded_test_server()->GetURL("/pdf/test-iframe.html"));
  ui_test_utils::NavigateToURL(browser(), test_iframe_url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");

  content::WebContents* guest_contents = nullptr;
  content::BrowserPluginGuestManager* guest_manager =
        contents->GetBrowserContext()->GetGuestManager();
  guest_manager->ForEachGuest(contents,
                              base::Bind(&RetrieveGuestContents,
                                         &guest_contents));
  ASSERT_TRUE(guest_contents);

  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshot(guest_contents);
  std::string ax_tree_dump = DumpPdfAccessibilityTree(ax_tree);
  ASSERT_MULTILINE_STREQ(kExpectedPDFAXTree, ax_tree_dump);
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfAccessibilityInOOPIF) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  GURL test_iframe_url(embedded_test_server()->GetURL(
      "/pdf/test-cross-site-iframe.html"));
  ui_test_utils::NavigateToURL(browser(), test_iframe_url);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  WaitForAccessibilityTreeToContainNodeWithName(contents,
                                                "1 First Section\r\n");

  content::WebContents* guest_contents = nullptr;
  content::BrowserPluginGuestManager* guest_manager =
        contents->GetBrowserContext()->GetGuestManager();
  guest_manager->ForEachGuest(contents,
                              base::Bind(&RetrieveGuestContents,
                                         &guest_contents));
  ASSERT_TRUE(guest_contents);

  ui::AXTreeUpdate ax_tree = GetAccessibilityTreeSnapshot(guest_contents);
  std::string ax_tree_dump = DumpPdfAccessibilityTree(ax_tree);
  ASSERT_MULTILINE_STREQ(kExpectedPDFAXTree, ax_tree_dump);
}

#if defined(GOOGLE_CHROME_BUILD)
// Test a particular PDF encountered in the wild that triggered a crash
// when accessibility is enabled.  (http://crbug.com/668724)
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, PdfAccessibilityTextRunCrash) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  GURL test_pdf_url(embedded_test_server()->GetURL(
      "/pdf_private/accessibility_crash_2.pdf"));

  content::WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  WaitForAccessibilityTreeToContainNodeWithName(guest_contents, "Page 1");
}
#endif

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, LinkCtrlLeftClick) {
  host_resolver()->AddRule("www.example.com", "127.0.0.1");
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test-link.pdf"));
  content::WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  // The link position of the test-link.pdf in page coordinates is (110, 110).
  // Convert the link position from page coordinates to screen coordinates.
  gfx::Point link_position(110, 110);
  ConvertPageCoordToScreenCoord(guest_contents, &link_position);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  content::SimulateMouseClickAt(web_contents, kDefaultKeyModifier,
      blink::WebMouseEvent::Button::Left, link_position);
  observer.Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetURL();
  ASSERT_EQ(std::string("http://www.example.com/"), url.spec());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, LinkMiddleClick) {
  host_resolver()->AddRule("www.example.com", "127.0.0.1");
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test-link.pdf"));
  content::WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  // The link position of the test-link.pdf in page coordinates is (110, 110).
  // Convert the link position from page coordinates to screen coordinates.
  gfx::Point link_position(110, 110);
  ConvertPageCoordToScreenCoord(guest_contents, &link_position);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  content::SimulateMouseClickAt(web_contents, 0,
      blink::WebMouseEvent::Button::Middle, link_position);
  observer.Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetURL();
  ASSERT_EQ(std::string("http://www.example.com/"), url.spec());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, LinkCtrlShiftLeftClick) {
  host_resolver()->AddRule("www.example.com", "127.0.0.1");
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test-link.pdf"));
  content::WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  // The link position of the test-link.pdf in page coordinates is (110, 110).
  // Convert the link position from page coordinates to screen coordinates.
  gfx::Point link_position(110, 110);
  ConvertPageCoordToScreenCoord(guest_contents, &link_position);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  int modifiers = blink::WebInputEvent::ShiftKey | kDefaultKeyModifier;

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  content::SimulateMouseClickAt(web_contents, modifiers,
      blink::WebMouseEvent::Button::Left, link_position);
  observer.Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  const GURL& url = active_web_contents->GetURL();
  ASSERT_EQ(std::string("http://www.example.com/"), url.spec());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, LinkShiftMiddleClick) {
  host_resolver()->AddRule("www.example.com", "127.0.0.1");
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test-link.pdf"));
  content::WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);

  // The link position of the test-link.pdf in page coordinates is (110, 110).
  // Convert the link position from page coordinates to screen coordinates.
  gfx::Point link_position(110, 110);
  ConvertPageCoordToScreenCoord(guest_contents, &link_position);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  content::SimulateMouseClickAt(web_contents, blink::WebInputEvent::ShiftKey,
      blink::WebMouseEvent::Button::Middle, link_position);
  observer.Wait();

  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  const GURL& url = active_web_contents->GetURL();
  ASSERT_EQ(std::string("http://www.example.com/"), url.spec());
}

IN_PROC_BROWSER_TEST_F(PDFExtensionTest, LinkShiftLeftClick) {
  host_resolver()->AddRule("www.example.com", "127.0.0.1");
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test-link.pdf"));
  content::WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);
  ASSERT_EQ(1U, chrome::GetTotalBrowserCount());

  // The link position of the test-link.pdf in page coordinates is (110, 110).
  // Convert the link position from page coordinates to screen coordinates.
  gfx::Point link_position(110, 110);
  ConvertPageCoordToScreenCoord(guest_contents, &link_position);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BROWSER_WINDOW_READY,
      content::NotificationService::AllSources());
  content::SimulateMouseClickAt(web_contents, blink::WebInputEvent::ShiftKey,
                                blink::WebMouseEvent::Button::Left,
                                link_position);
  observer.Wait();

  ASSERT_EQ(2U, chrome::GetTotalBrowserCount());

  content::WebContents* active_web_contents =
      chrome::FindLastActive()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  const GURL& url = active_web_contents->GetURL();
  ASSERT_EQ(std::string("http://www.example.com/"), url.spec());
}

// Test that if the plugin tries to load a URL that redirects then it will fail
// to load. This is to avoid the source origin of the document changing during
// the redirect, which can have security implications. https://crbug.com/653749.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, RedirectsFailInPlugin) {
  RunTestsInFile("redirects_fail_test.js", "test.pdf");
}

// Test that even if a different tab is selected when a navigation occurs,
// the correct tab still gets navigated (see crbug.com/672563).
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, NavigationOnCorrectTab) {
  GURL test_pdf_url(embedded_test_server()->GetURL("/pdf/test.pdf"));
  content::WebContents* guest_contents = LoadPdfGetGuestContents(test_pdf_url);
  ASSERT_TRUE(guest_contents);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
          ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(web_contents, active_web_contents);

  ASSERT_TRUE(content::ExecuteScript(
      guest_contents,
      "viewer.navigator_.navigate("
      "    'www.example.com', Navigator.WindowOpenDisposition.CURRENT_TAB);"));

  EXPECT_TRUE(web_contents->GetController().GetPendingEntry());
  EXPECT_FALSE(active_web_contents->GetController().GetPendingEntry());
}

// This test opens a PDF by clicking a link via javascript and verifies that
// the PDF is loaded and functional by clicking a link in the PDF. The link
// click in the PDF opens a new tab. The main page handles the pageShow event
// and updates the history state.
IN_PROC_BROWSER_TEST_F(PDFExtensionTest, OpenPDFOnLinkClickWithReplaceState) {
  host_resolver()->AddRule("www.example.com", "127.0.0.1");

  // Navigate to the main page.
  GURL test_url(
      embedded_test_server()->GetURL("/pdf/pdf_href_replace_state.html"));
  ui_test_utils::NavigateToURL(browser(), test_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Click on the link which opens the PDF via JS.
  content::TestNavigationObserver navigation_observer(web_contents);
  const char kPdfLinkClick[] = "document.getElementById('link').click();";
  ASSERT_TRUE(content::ExecuteScript(web_contents->GetRenderViewHost(),
                                     kPdfLinkClick));
  navigation_observer.Wait();
  const GURL& current_url = web_contents->GetURL();
  ASSERT_TRUE(current_url.path() == "/pdf/test-link.pdf");

  ASSERT_TRUE(pdf_extension_test_util::EnsurePDFHasLoaded(web_contents));

  // Now click on the link to example.com in the PDF. This should open up a new
  // tab.
  content::BrowserPluginGuestManager* guest_manager =
      web_contents->GetBrowserContext()->GetGuestManager();
  content::WebContents* guest_contents =
      guest_manager->GetFullPageGuest(web_contents);
  ASSERT_TRUE(guest_contents);
  // The link position of the test-link.pdf in page coordinates is (110, 110).
  // Convert the link position from page coordinates to screen coordinates.
  gfx::Point link_position(110, 110);
  ConvertPageCoordToScreenCoord(guest_contents, &link_position);

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());
  content::SimulateMouseClickAt(web_contents, kDefaultKeyModifier,
                                blink::WebMouseEvent::Button::Left,
                                link_position);
  observer.Wait();

  // We should have two tabs now. One with the PDF and the second for
  // example.com
  int tab_count = browser()->tab_strip_model()->count();
  ASSERT_EQ(2, tab_count);

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(web_contents, active_web_contents);

  content::WebContents* new_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(new_web_contents);
  ASSERT_NE(web_contents, new_web_contents);

  const GURL& url = new_web_contents->GetURL();
  ASSERT_EQ(GURL("http://www.example.com"), url);
}
