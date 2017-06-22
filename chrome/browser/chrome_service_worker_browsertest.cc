// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file tests that Service Workers (a Content feature) work in the Chromium
// embedder.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ppapi/shared_impl/ppapi_switches.h"

namespace {

const std::string kInstallAndWaitForActivatedPage =
    "<script>"
    "navigator.serviceWorker.register('./sw.js', {scope: './scope/'})"
    "  .then(function(reg) {"
    "      reg.addEventListener('updatefound', function() {"
    "          var worker = reg.installing;"
    "          worker.addEventListener('statechange', function() {"
    "              if (worker.state == 'activated')"
    "                document.title = 'READY';"
    "            });"
    "        });"
    "    });"
    "</script>";

class ChromeServiceWorkerTest : public InProcessBrowserTest {
 protected:
  ChromeServiceWorkerTest() {
    EXPECT_TRUE(service_worker_dir_.CreateUniqueTempDir());
    EXPECT_TRUE(base::CreateDirectoryAndGetError(
        service_worker_dir_.GetPath().Append(
            FILE_PATH_LITERAL("scope")), nullptr));
  }
  ~ChromeServiceWorkerTest() override {}

  void WriteFile(const base::FilePath::StringType& filename,
                 base::StringPiece contents) {
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    EXPECT_EQ(base::checked_cast<int>(contents.size()),
              base::WriteFile(service_worker_dir_.GetPath().Append(filename),
                              contents.data(), contents.size()));
  }

  void NavigateToPageAndWaitForReadyTitle(const std::string path) {
    const base::string16 expected_title1 = base::ASCIIToUTF16("READY");
    content::TitleWatcher title_watcher1(
        browser()->tab_strip_model()->GetActiveWebContents(), expected_title1);
    ui_test_utils::NavigateToURL(browser(),
                                 embedded_test_server()->GetURL(path));
    EXPECT_EQ(expected_title1, title_watcher1.WaitAndGetTitle());
  }

  void InitializeServer() {
    embedded_test_server()->ServeFilesFromDirectory(
        service_worker_dir_.GetPath());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::ServiceWorkerContext* GetServiceWorkerContext() {
    return content::BrowserContext::GetDefaultStoragePartition(
               browser()->profile())
        ->GetServiceWorkerContext();
  }

  base::ScopedTempDir service_worker_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeServiceWorkerTest);
};

template <typename T>
static void ExpectResultAndRun(T expected,
                               const base::Closure& continuation,
                               T actual) {
  EXPECT_EQ(expected, actual);
  continuation.Run();
}

// http://crbug.com/368570
IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerTest,
                       CanShutDownWithRegisteredServiceWorker) {
  WriteFile(FILE_PATH_LITERAL("service_worker.js"), "");

  embedded_test_server()->ServeFilesFromDirectory(
      service_worker_dir_.GetPath());
  ASSERT_TRUE(embedded_test_server()->Start());

  base::RunLoop run_loop;
  GetServiceWorkerContext()->RegisterServiceWorker(
      embedded_test_server()->GetURL("/"),
      embedded_test_server()->GetURL("/service_worker.js"),
      base::Bind(&ExpectResultAndRun<bool>, true, run_loop.QuitClosure()));
  run_loop.Run();

  // Leave the Service Worker registered, and make sure that the browser can
  // shut down without DCHECK'ing. It'd be nice to check here that the SW is
  // actually occupying a process, but we don't yet have the public interface to
  // do that.
}

// http://crbug.com/419290
IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerTest,
                       CanCloseIncognitoWindowWithServiceWorkerController) {
  WriteFile(FILE_PATH_LITERAL("service_worker.js"), "");
  WriteFile(FILE_PATH_LITERAL("service_worker.js.mock-http-headers"),
            "HTTP/1.1 200 OK\nContent-Type: text/javascript");
  WriteFile(FILE_PATH_LITERAL("test.html"), "");
  InitializeServer();

  Browser* incognito = CreateIncognitoBrowser();

  base::RunLoop run_loop;
  GetServiceWorkerContext()->RegisterServiceWorker(
      embedded_test_server()->GetURL("/"),
      embedded_test_server()->GetURL("/service_worker.js"),
      base::Bind(&ExpectResultAndRun<bool>, true, run_loop.QuitClosure()));
  run_loop.Run();

  ui_test_utils::NavigateToURL(incognito,
                               embedded_test_server()->GetURL("/test.html"));

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, content::Source<Browser>(incognito));
  incognito->window()->Close();
  observer.Wait();

  // Test passes if we don't crash.
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerTest,
                       FailRegisterServiceWorkerWhenJSDisabled) {
  WriteFile(FILE_PATH_LITERAL("service_worker.js"), "");
  InitializeServer();

  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);

  base::RunLoop run_loop;
  GetServiceWorkerContext()->RegisterServiceWorker(
      embedded_test_server()->GetURL("/"),
      embedded_test_server()->GetURL("/service_worker.js"),
      base::Bind(&ExpectResultAndRun<bool>, false, run_loop.QuitClosure()));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerTest,
                       FallbackMainResourceRequestWhenJSDisabled) {
  WriteFile(
      FILE_PATH_LITERAL("sw.js"),
      "self.onfetch = function(e) {"
      "  e.respondWith(new Response('<title>Fail</title>',"
      "                             {headers: {'Content-Type': 'text/html'}}));"
      "};");
  WriteFile(FILE_PATH_LITERAL("scope/done.html"), "<title>Done</title>");
  WriteFile(FILE_PATH_LITERAL("test.html"), kInstallAndWaitForActivatedPage);
  InitializeServer();
  NavigateToPageAndWaitForReadyTitle("/test.html");

  GetServiceWorkerContext()->StopAllServiceWorkersForOrigin(
      embedded_test_server()->base_url());
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_JAVASCRIPT,
                                 CONTENT_SETTING_BLOCK);

  const base::string16 expected_title2 = base::ASCIIToUTF16("Done");
  content::TitleWatcher title_watcher2(
      browser()->tab_strip_model()->GetActiveWebContents(), expected_title2);
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/scope/done.html"));
  EXPECT_EQ(expected_title2, title_watcher2.WaitAndGetTitle());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(TabSpecificContentSettings::FromWebContents(web_contents)->
              IsContentBlocked(CONTENT_SETTINGS_TYPE_JAVASCRIPT));
}

class ChromeServiceWorkerFetchTest : public ChromeServiceWorkerTest {
 protected:
  ChromeServiceWorkerFetchTest() {}
  ~ChromeServiceWorkerFetchTest() override {}

  void SetUpOnMainThread() override {
    WriteServiceWorkerFetchTestFiles();
    embedded_test_server()->ServeFilesFromDirectory(
        service_worker_dir_.GetPath());
    ASSERT_TRUE(embedded_test_server()->Start());
    InitializeServiceWorkerFetchTestPage();
  }

  std::string ExecuteScriptAndExtractString(const std::string& js) {
    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(), js, &result));
    return result;
  }

  std::string RequestString(const std::string& url,
                            const std::string& mode,
                            const std::string& credentials) const {
    return base::StringPrintf("url:%s, mode:%s, credentials:%s\n", url.c_str(),
                              mode.c_str(), credentials.c_str());
  }

  std::string GetURL(const std::string& relative_url) const {
    return embedded_test_server()->GetURL(relative_url).spec();
  }

 private:
  void WriteServiceWorkerFetchTestFiles() {
    WriteFile(FILE_PATH_LITERAL("sw.js"),
              "this.onactivate = function(event) {"
              "  event.waitUntil(self.clients.claim());"
              "};"
              "this.onfetch = function(event) {"
              "  event.respondWith("
              "      self.clients.matchAll().then(function(clients) {"
              "          clients.forEach(function(client) {"
              "              client.postMessage("
              "                'url:' + event.request.url + ', ' +"
              "                'mode:' + event.request.mode + ', ' +"
              "                'credentials:' + event.request.credentials"
              "              );"
              "            });"
              "          return fetch(event.request);"
              "        }));"
              "};");
    WriteFile(FILE_PATH_LITERAL("test.html"),
              "<script>"
              "navigator.serviceWorker.register('./sw.js', {scope: './'})"
              "  .then(function(reg) {"
              "      reg.addEventListener('updatefound', function() {"
              "          var worker = reg.installing;"
              "          worker.addEventListener('statechange', function() {"
              "              if (worker.state == 'activated')"
              "                document.title = 'READY';"
              "            });"
              "        });"
              "    });"
              "var reportOnFetch = true;"
              "var issuedRequests = [];"
              "function reportRequests() {"
              "  var str = '';"
              "  issuedRequests.forEach(function(data) {"
              "      str += data + '\\n';"
              "    });"
              "  window.domAutomationController.setAutomationId(0);"
              "  window.domAutomationController.send(str);"
              "}"
              "navigator.serviceWorker.addEventListener("
              "    'message',"
              "    function(event) {"
              "      issuedRequests.push(event.data);"
              "      if (reportOnFetch) {"
              "        reportRequests();"
              "      }"
              "    }, false);"
              "</script>");
  }

  void InitializeServiceWorkerFetchTestPage() {
    // The message "READY" will be sent when the service worker is activated.
    const base::string16 expected_title = base::ASCIIToUTF16("READY");
    content::TitleWatcher title_watcher(
        browser()->tab_strip_model()->GetActiveWebContents(), expected_title);
    ui_test_utils::NavigateToURL(browser(),
                                 embedded_test_server()->GetURL("/test.html"));
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  }

  DISALLOW_COPY_AND_ASSIGN(ChromeServiceWorkerFetchTest);
};

// Flaky on Windows; https://crbug.com/628898.
#if defined(OS_WIN)
#define MAYBE_EmbedPdfSameOrigin DISABLED_EmbedPdfSameOrigin
#else
#define MAYBE_EmbedPdfSameOrigin EmbedPdfSameOrigin
#endif
IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchTest, MAYBE_EmbedPdfSameOrigin) {
  // <embed src="test.pdf">
  const std::string result(ExecuteScriptAndExtractString(
      "var embed = document.createElement('embed');"
      "embed.src = 'test.pdf';"
      "document.body.appendChild(embed);"));
  EXPECT_EQ(RequestString(GetURL("/test.pdf"), "no-cors", "include"), result);
}

// Flaky on Windows; https://crbug.com/628898.
#if defined(OS_WIN)
#define MAYBE_EmbedPdfOtherOrigin DISABLED_EmbedPdfOtherOrigin
#else
#define MAYBE_EmbedPdfOtherOrigin EmbedPdfOtherOrigin
#endif
IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchTest,
                       MAYBE_EmbedPdfOtherOrigin) {
  // <embed src="https://www.example.com/test.pdf">
  const std::string result(ExecuteScriptAndExtractString(
      "var embed = document.createElement('embed');"
      "embed.src = 'https://www.example.com/test.pdf';"
      "document.body.appendChild(embed);"));
  EXPECT_EQ(
      RequestString("https://www.example.com/test.pdf", "no-cors", "include"),
      result);
}

class ChromeServiceWorkerManifestFetchTest
    : public ChromeServiceWorkerFetchTest {
 protected:
  ChromeServiceWorkerManifestFetchTest() {}
  ~ChromeServiceWorkerManifestFetchTest() override {}

  std::string ExecuteManifestFetchTest(const std::string& url,
                                       const std::string& cross_origin) {
    std::string js(
        base::StringPrintf("reportOnFetch = false;"
                           "var link = document.createElement('link');"
                           "link.rel = 'manifest';"
                           "link.href = '%s';",
                           url.c_str()));
    if (!cross_origin.empty()) {
      js +=
          base::StringPrintf("link.crossOrigin = '%s';", cross_origin.c_str());
    }
    js += "document.head.appendChild(link);";
    ExecuteJavaScriptForTests(js);
    return GetManifestAndIssuedRequests();
  }

 private:
  void ExecuteJavaScriptForTests(const std::string& js) {
    browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetMainFrame()
        ->ExecuteJavaScriptForTests(base::ASCIIToUTF16(js));
  }

  std::string GetManifestAndIssuedRequests() {
    base::RunLoop run_loop;
    browser()->tab_strip_model()->GetActiveWebContents()->GetManifest(
        base::Bind(&ManifestCallbackAndRun, run_loop.QuitClosure()));
    run_loop.Run();
    return ExecuteScriptAndExtractString(
        "if (issuedRequests.length != 0) reportRequests();"
        "else reportOnFetch = true;");
  }

  static void ManifestCallbackAndRun(const base::Closure& continuation,
                                     const GURL&,
                                     const content::Manifest&) {
    continuation.Run();
  }

  DISALLOW_COPY_AND_ASSIGN(ChromeServiceWorkerManifestFetchTest);
};

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerManifestFetchTest, SameOrigin) {
  // <link rel="manifest" href="manifest.json">
  EXPECT_EQ(RequestString(GetURL("/manifest.json"), "cors", "omit"),
            ExecuteManifestFetchTest("manifest.json", ""));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerManifestFetchTest,
                       SameOriginUseCredentials) {
  // <link rel="manifest" href="manifest.json" crossorigin="use-credentials">
  EXPECT_EQ(RequestString(GetURL("/manifest.json"), "cors", "include"),
            ExecuteManifestFetchTest("manifest.json", "use-credentials"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerManifestFetchTest, OtherOrigin) {
  // <link rel="manifest" href="https://www.example.com/manifest.json">
  EXPECT_EQ(
      RequestString("https://www.example.com/manifest.json", "cors", "omit"),
      ExecuteManifestFetchTest("https://www.example.com/manifest.json", ""));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerManifestFetchTest,
                       OtherOriginUseCredentials) {
  // <link rel="manifest" href="https://www.example.com/manifest.json"
  //  crossorigin="use-credentials">
  EXPECT_EQ(
      RequestString("https://www.example.com/manifest.json", "cors", "include"),
      ExecuteManifestFetchTest("https://www.example.com/manifest.json",
                               "use-credentials"));
}

class ChromeServiceWorkerFetchPPAPITest : public ChromeServiceWorkerFetchTest {
 protected:
  ChromeServiceWorkerFetchPPAPITest() {}
  ~ChromeServiceWorkerFetchPPAPITest() override {}

  void SetUpOnMainThread() override {
    base::FilePath document_root;
    ASSERT_TRUE(ui_test_utils::GetRelativeBuildDirectory(&document_root));
    embedded_test_server()->AddDefaultHandlers(
        document_root.Append(FILE_PATH_LITERAL("nacl_test_data"))
            .Append(FILE_PATH_LITERAL("pnacl")));
    ChromeServiceWorkerFetchTest::SetUpOnMainThread();
    test_page_url_ = GetURL("/pnacl_url_loader.html");
  }

  std::string GetRequestStringForPNACL(const std::string& fragment) const {
    return RequestString(test_page_url_ + fragment, "navigate", "include") +
           RequestString(GetURL("/pnacl_url_loader.nmf"), "same-origin",
                         "same-origin") +
           RequestString(GetURL("/pnacl_url_loader_newlib_pnacl.pexe"),
                         "same-origin", "same-origin");
  }

  std::string ExecutePNACLUrlLoaderTest(const std::string& mode) {
    std::string result(ExecuteScriptAndExtractString(
        base::StringPrintf("reportOnFetch = false;"
                           "var iframe = document.createElement('iframe');"
                           "iframe.src='%s#%s';"
                           "document.body.appendChild(iframe);",
                           test_page_url_.c_str(), mode.c_str())));
    EXPECT_EQ(base::StringPrintf("OnOpen%s", mode.c_str()), result);
    return ExecuteScriptAndExtractString("reportRequests();");
  }

 private:
  std::string test_page_url_;

  DISALLOW_COPY_AND_ASSIGN(ChromeServiceWorkerFetchPPAPITest);
};

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPITest, SameOrigin) {
  // In pnacl_url_loader.cc:
  //   request.SetMethod("GET");
  //   request.SetURL("/echo");
  EXPECT_EQ(GetRequestStringForPNACL("#Same") +
                RequestString(GetURL("/echo"), "same-origin", "include"),
            ExecutePNACLUrlLoaderTest("Same"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPITest, SameOriginCORS) {
  // In pnacl_url_loader.cc:
  //   request.SetMethod("GET");
  //   request.SetURL("/echo");
  //   request.SetAllowCrossOriginRequests(true);
  EXPECT_EQ(GetRequestStringForPNACL("#SameCORS") +
                RequestString(GetURL("/echo"), "cors", "omit"),
            ExecutePNACLUrlLoaderTest("SameCORS"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPITest,
                       SameOriginCredentials) {
  // In pnacl_url_loader.cc:
  //   request.SetMethod("GET");
  //   request.SetURL("/echo");
  //   request.SetAllowCredentials(true);
  EXPECT_EQ(GetRequestStringForPNACL("#SameCredentials") +
                RequestString(GetURL("/echo"), "same-origin", "include"),
            ExecutePNACLUrlLoaderTest("SameCredentials"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPITest,
                       SameOriginCORSCredentials) {
  // In pnacl_url_loader.cc:
  //   request.SetMethod("GET");
  //   request.SetURL("/echo");
  //   request.SetAllowCrossOriginRequests(true);
  //   request.SetAllowCredentials(true);
  EXPECT_EQ(GetRequestStringForPNACL("#SameCORSCredentials") +
                RequestString(GetURL("/echo"), "cors", "include"),
            ExecutePNACLUrlLoaderTest("SameCORSCredentials"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPITest, OtherOrigin) {
  // In pnacl_url_loader.cc:
  //   request.SetMethod("GET");
  //   request.SetURL("https://www.example.com/echo");
  // This request fails because AllowCrossOriginRequests is not set.
  EXPECT_EQ(GetRequestStringForPNACL("#Other"),
            ExecutePNACLUrlLoaderTest("Other"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPITest, OtherOriginCORS) {
  // In pnacl_url_loader.cc:
  //   request.SetMethod("GET");
  //   request.SetURL("https://www.example.com/echo");
  //   request.SetAllowCrossOriginRequests(true);
  EXPECT_EQ(GetRequestStringForPNACL("#OtherCORS") +
                RequestString("https://www.example.com/echo", "cors", "omit"),
            ExecutePNACLUrlLoaderTest("OtherCORS"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPITest,
                       OtherOriginCredentials) {
  // In pnacl_url_loader.cc:
  //   request.SetMethod("GET");
  //   request.SetURL("https://www.example.com/echo");
  //   request.SetAllowCredentials(true);
  // This request fails because AllowCrossOriginRequests is not set.
  EXPECT_EQ(GetRequestStringForPNACL("#OtherCredentials"),
            ExecutePNACLUrlLoaderTest("OtherCredentials"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPITest,
                       OtherOriginCORSCredentials) {
  // In pnacl_url_loader.cc:
  //   request.SetMethod("GET");
  //   request.SetURL("https://www.example.com/echo");
  //   request.SetAllowCrossOriginRequests(true);
  //   request.SetAllowCredentials(true);
  EXPECT_EQ(
      GetRequestStringForPNACL("#OtherCORSCredentials") +
          RequestString("https://www.example.com/echo", "cors", "include"),
      ExecutePNACLUrlLoaderTest("OtherCORSCredentials"));
}

class ChromeServiceWorkerFetchPPAPIPrivateTest
    : public ChromeServiceWorkerFetchPPAPITest {
 protected:
  ChromeServiceWorkerFetchPPAPIPrivateTest() {}
  ~ChromeServiceWorkerFetchPPAPIPrivateTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeServiceWorkerFetchPPAPITest::SetUpCommandLine(command_line);
    // Sets this flag to test that the fetch request from the plugins with
    // private permission (PERMISSION_PRIVATE) should not go to the service
    // worker.
    command_line->AppendSwitch(switches::kEnablePepperTesting);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeServiceWorkerFetchPPAPIPrivateTest);
};

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPIPrivateTest, SameOrigin) {
  EXPECT_EQ(GetRequestStringForPNACL("#Same"),
            ExecutePNACLUrlLoaderTest("Same"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPIPrivateTest,
                       SameOriginCORS) {
  EXPECT_EQ(GetRequestStringForPNACL("#SameCORS"),
            ExecutePNACLUrlLoaderTest("SameCORS"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPIPrivateTest,
                       SameOriginCredentials) {
  EXPECT_EQ(GetRequestStringForPNACL("#SameCredentials"),
            ExecutePNACLUrlLoaderTest("SameCredentials"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPIPrivateTest,
                       SameOriginCORSCredentials) {
  EXPECT_EQ(GetRequestStringForPNACL("#SameCORSCredentials"),
            ExecutePNACLUrlLoaderTest("SameCORSCredentials"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPIPrivateTest, OtherOrigin) {
  EXPECT_EQ(GetRequestStringForPNACL("#Other"),
            ExecutePNACLUrlLoaderTest("Other"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPIPrivateTest,
                       OtherOriginCORS) {
  EXPECT_EQ(GetRequestStringForPNACL("#OtherCORS"),
            ExecutePNACLUrlLoaderTest("OtherCORS"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPIPrivateTest,
                       OtherOriginCredentials) {
  EXPECT_EQ(GetRequestStringForPNACL("#OtherCredentials"),
            ExecutePNACLUrlLoaderTest("OtherCredentials"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPIPrivateTest,
                       OtherOriginCORSCredentials) {
  EXPECT_EQ(GetRequestStringForPNACL("#OtherCORSCredentials"),
            ExecutePNACLUrlLoaderTest("OtherCORSCredentials"));
}

class ChromeServiceWorkerNavigationHintTest : public ChromeServiceWorkerTest {
 protected:
  void RunNavigationHintTest(
      const char* scope,
      content::StartServiceWorkerForNavigationHintResult expeced_requst,
      bool expected_started) {
    base::RunLoop run_loop;
    GetServiceWorkerContext()->StartServiceWorkerForNavigationHint(
        embedded_test_server()->GetURL(scope),
        base::Bind(&ExpectResultAndRun<
                       content::StartServiceWorkerForNavigationHintResult>,
                   expeced_requst, run_loop.QuitClosure()));
    run_loop.Run();
    if (expected_started) {
      histogram_tester_.ExpectBucketCount(
          "ServiceWorker.StartWorker.Purpose",
          27 /* ServiceWorkerMetrics::EventType::NAVIGATION_HINT  */, 1);
      histogram_tester_.ExpectBucketCount(
          "ServiceWorker.StartWorker.StatusByPurpose_NAVIGATION_HINT",
          0 /* SERVICE_WORKER_OK  */, 1);
    } else {
      histogram_tester_.ExpectTotalCount("ServiceWorker.StartWorker.Purpose",
                                         0);
      histogram_tester_.ExpectTotalCount(
          "ServiceWorker.StartWorker.StatusByPurpose_NAVIGATION_HINT", 0);
    }
  }

  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerNavigationHintTest, Started) {
  WriteFile(FILE_PATH_LITERAL("sw.js"), "self.onfetch = function(e) {};");
  WriteFile(FILE_PATH_LITERAL("test.html"), kInstallAndWaitForActivatedPage);
  InitializeServer();
  NavigateToPageAndWaitForReadyTitle("/test.html");
  GetServiceWorkerContext()->StopAllServiceWorkersForOrigin(
      embedded_test_server()->base_url());
  RunNavigationHintTest(
      "/scope/", content::StartServiceWorkerForNavigationHintResult::STARTED,
      true);
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerNavigationHintTest, AlreadyRunning) {
  WriteFile(FILE_PATH_LITERAL("sw.js"), "self.onfetch = function(e) {};");
  WriteFile(FILE_PATH_LITERAL("test.html"), kInstallAndWaitForActivatedPage);
  InitializeServer();
  NavigateToPageAndWaitForReadyTitle("/test.html");
  RunNavigationHintTest(
      "/scope/",
      content::StartServiceWorkerForNavigationHintResult::ALREADY_RUNNING,
      false);
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerNavigationHintTest,
                       NoServiceWorkerRegistration) {
  InitializeServer();
  RunNavigationHintTest("/scope/",
                        content::StartServiceWorkerForNavigationHintResult::
                            NO_SERVICE_WORKER_REGISTRATION,
                        false);
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerNavigationHintTest,
                       NoActiveServiceWorkerVersion) {
  WriteFile(FILE_PATH_LITERAL("sw.js"),
            "self.oninstall = function(e) {\n"
            "    e.waitUntil(new Promise(r => { /* never resolve */ }));\n"
            "  };\n"
            "self.onfetch = function(e) {};");
  InitializeServer();
  base::RunLoop run_loop;
  GetServiceWorkerContext()->RegisterServiceWorker(
      embedded_test_server()->GetURL("/scope/"),
      embedded_test_server()->GetURL("/sw.js"),
      base::Bind(&ExpectResultAndRun<bool>, true, run_loop.QuitClosure()));
  run_loop.Run();
  RunNavigationHintTest("/scope/",
                        content::StartServiceWorkerForNavigationHintResult::
                            NO_ACTIVE_SERVICE_WORKER_VERSION,
                        false);
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerNavigationHintTest, NoFetchHandler) {
  WriteFile(FILE_PATH_LITERAL("sw.js"), "/* empty */");
  WriteFile(FILE_PATH_LITERAL("test.html"), kInstallAndWaitForActivatedPage);
  InitializeServer();
  NavigateToPageAndWaitForReadyTitle("/test.html");
  GetServiceWorkerContext()->StopAllServiceWorkersForOrigin(
      embedded_test_server()->base_url());
  RunNavigationHintTest(
      "/scope/",
      content::StartServiceWorkerForNavigationHintResult::NO_FETCH_HANDLER,
      false);
}

}  // namespace
