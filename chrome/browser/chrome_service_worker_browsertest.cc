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
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ppapi/shared_impl/ppapi_switches.h"

namespace {

class ChromeServiceWorkerTest : public InProcessBrowserTest {
 protected:
  ChromeServiceWorkerTest() {
    EXPECT_TRUE(service_worker_dir_.CreateUniqueTempDir());
  }
  ~ChromeServiceWorkerTest() override {}

  void WriteFile(const base::FilePath::StringType& filename,
                 base::StringPiece contents) {
    EXPECT_EQ(base::checked_cast<int>(contents.size()),
              base::WriteFile(service_worker_dir_.path().Append(filename),
                              contents.data(),
                              contents.size()));
  }

  base::ScopedTempDir service_worker_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeServiceWorkerTest);
};

static void ExpectResultAndRun(bool expected,
                               const base::Closure& continuation,
                               bool actual) {
  EXPECT_EQ(expected, actual);
  continuation.Run();
}

// http://crbug.com/368570
IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerTest,
                       CanShutDownWithRegisteredServiceWorker) {
  WriteFile(FILE_PATH_LITERAL("service_worker.js"), "");
  WriteFile(FILE_PATH_LITERAL("service_worker.js.mock-http-headers"),
            "HTTP/1.1 200 OK\nContent-Type: text/javascript");

  embedded_test_server()->ServeFilesFromDirectory(service_worker_dir_.path());
  ASSERT_TRUE(embedded_test_server()->Start());

  content::ServiceWorkerContext* sw_context =
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
          ->GetServiceWorkerContext();

  base::RunLoop run_loop;
  sw_context->RegisterServiceWorker(
      embedded_test_server()->GetURL("/"),
      embedded_test_server()->GetURL("/service_worker.js"),
      base::Bind(&ExpectResultAndRun, true, run_loop.QuitClosure()));
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

  embedded_test_server()->ServeFilesFromDirectory(service_worker_dir_.path());
  ASSERT_TRUE(embedded_test_server()->Start());

  Browser* incognito = CreateIncognitoBrowser();
  content::ServiceWorkerContext* sw_context =
      content::BrowserContext::GetDefaultStoragePartition(incognito->profile())
          ->GetServiceWorkerContext();

  base::RunLoop run_loop;
  sw_context->RegisterServiceWorker(
      embedded_test_server()->GetURL("/"),
      embedded_test_server()->GetURL("/service_worker.js"),
      base::Bind(&ExpectResultAndRun, true, run_loop.QuitClosure()));
  run_loop.Run();

  ui_test_utils::NavigateToURL(incognito,
                               embedded_test_server()->GetURL("/test.html"));

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_BROWSER_CLOSED, content::Source<Browser>(incognito));
  incognito->window()->Close();
  observer.Wait();

  // Test passes if we don't crash.
}

class ChromeServiceWorkerFetchTest : public ChromeServiceWorkerTest {
 protected:
  ChromeServiceWorkerFetchTest() {}
  ~ChromeServiceWorkerFetchTest() override {}

  void SetUpOnMainThread() override {
    WriteServiceWorkerFetchTestFiles();
    embedded_test_server()->ServeFilesFromDirectory(service_worker_dir_.path());
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

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchTest, EmbedPdfSameOrigin) {
  // <embed src="test.pdf">
  const std::string result(ExecuteScriptAndExtractString(
      "var embed = document.createElement('embed');"
      "embed.src = 'test.pdf';"
      "document.body.appendChild(embed);"));
  EXPECT_EQ(RequestString(GetURL("/test.pdf"), "no-cors", "include"), result);
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchTest, EmbedPdfOtherOrigin) {
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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeServiceWorkerFetchTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableAddToShelf);
  }

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
    return RequestAppBannerAndGetIssuedRequests();
  }

 private:
  void ExecuteJavaScriptForTests(const std::string& js) {
    browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetMainFrame()
        ->ExecuteJavaScriptForTests(base::ASCIIToUTF16(js));
  }

  std::string RequestAppBannerAndGetIssuedRequests() {
    browser()->RequestAppBannerFromDevTools(
        browser()->tab_strip_model()->GetActiveWebContents());
    return ExecuteScriptAndExtractString(
        "if (issuedRequests.length != 0) reportRequests();"
        "else reportOnFetch = true;");
  }
  DISALLOW_COPY_AND_ASSIGN(ChromeServiceWorkerManifestFetchTest);
};

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerManifestFetchTest, SameOrigin) {
  // <link rel="manifest" href="manifest.json">
  EXPECT_EQ(RequestString(GetURL("/manifest.json"), "cors", "same-origin"),
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
      RequestString("https://www.example.com/manifest.json", "cors",
                    "same-origin"),
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

  std::string GetRequestStringForPNACL() const {
    return RequestString(test_page_url_, "navigate", "include") +
           RequestString(GetURL("/pnacl_url_loader.nmf"), "same-origin",
                         "include") +
           RequestString(GetURL("/pnacl_url_loader_newlib_pnacl.pexe"),
                         "same-origin", "include");
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
  EXPECT_EQ(GetRequestStringForPNACL() +
                RequestString(GetURL("/echo"), "same-origin", "include"),
            ExecutePNACLUrlLoaderTest("Same"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPITest, SameOriginCORS) {
  // In pnacl_url_loader.cc:
  //   request.SetMethod("GET");
  //   request.SetURL("/echo");
  //   request.SetAllowCrossOriginRequests(true);
  EXPECT_EQ(GetRequestStringForPNACL() +
                RequestString(GetURL("/echo"), "cors", "same-origin"),
            ExecutePNACLUrlLoaderTest("SameCORS"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPITest,
                       SameOriginCredentials) {
  // In pnacl_url_loader.cc:
  //   request.SetMethod("GET");
  //   request.SetURL("/echo");
  //   request.SetAllowCredentials(true);
  EXPECT_EQ(GetRequestStringForPNACL() +
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
  EXPECT_EQ(GetRequestStringForPNACL() +
                RequestString(GetURL("/echo"), "cors", "include"),
            ExecutePNACLUrlLoaderTest("SameCORSCredentials"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPITest, OtherOrigin) {
  // In pnacl_url_loader.cc:
  //   request.SetMethod("GET");
  //   request.SetURL("https://www.example.com/echo");
  // This request fails because AllowCrossOriginRequests is not set.
  EXPECT_EQ(GetRequestStringForPNACL(), ExecutePNACLUrlLoaderTest("Other"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPITest, OtherOriginCORS) {
  // In pnacl_url_loader.cc:
  //   request.SetMethod("GET");
  //   request.SetURL("https://www.example.com/echo");
  //   request.SetAllowCrossOriginRequests(true);
  EXPECT_EQ(
      GetRequestStringForPNACL() +
          RequestString("https://www.example.com/echo", "cors", "same-origin"),
      ExecutePNACLUrlLoaderTest("OtherCORS"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPITest,
                       OtherOriginCredentials) {
  // In pnacl_url_loader.cc:
  //   request.SetMethod("GET");
  //   request.SetURL("https://www.example.com/echo");
  //   request.SetAllowCredentials(true);
  // This request fails because AllowCrossOriginRequests is not set.
  EXPECT_EQ(GetRequestStringForPNACL(),
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
      GetRequestStringForPNACL() +
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
  EXPECT_EQ(GetRequestStringForPNACL(), ExecutePNACLUrlLoaderTest("Same"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPIPrivateTest,
                       SameOriginCORS) {
  EXPECT_EQ(GetRequestStringForPNACL(), ExecutePNACLUrlLoaderTest("SameCORS"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPIPrivateTest,
                       SameOriginCredentials) {
  EXPECT_EQ(GetRequestStringForPNACL(),
            ExecutePNACLUrlLoaderTest("SameCredentials"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPIPrivateTest,
                       SameOriginCORSCredentials) {
  EXPECT_EQ(GetRequestStringForPNACL(),
            ExecutePNACLUrlLoaderTest("SameCORSCredentials"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPIPrivateTest, OtherOrigin) {
  EXPECT_EQ(GetRequestStringForPNACL(), ExecutePNACLUrlLoaderTest("Other"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPIPrivateTest,
                       OtherOriginCORS) {
  EXPECT_EQ(GetRequestStringForPNACL(), ExecutePNACLUrlLoaderTest("OtherCORS"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPIPrivateTest,
                       OtherOriginCredentials) {
  EXPECT_EQ(GetRequestStringForPNACL(),
            ExecutePNACLUrlLoaderTest("OtherCredentials"));
}

IN_PROC_BROWSER_TEST_F(ChromeServiceWorkerFetchPPAPIPrivateTest,
                       OtherOriginCORSCredentials) {
  EXPECT_EQ(GetRequestStringForPNACL(),
            ExecutePNACLUrlLoaderTest("OtherCORSCredentials"));
}

}  // namespace
