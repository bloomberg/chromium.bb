// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/test_data_directory.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

namespace {

// PAC script that sends all requests to an invalid proxy server.
const base::FilePath::CharType kPACScript[] = FILE_PATH_LITERAL(
    "bad_server.pac");

// Verify kPACScript is installed as the PAC script.
void VerifyProxyScript(Browser* browser) {
  ui_test_utils::NavigateToURL(browser, GURL("http://google.com"));

  // Verify we get the ERR_PROXY_CONNECTION_FAILED screen.
  bool result = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser->tab_strip_model()->GetActiveWebContents(),
      "var textContent = document.body.textContent;"
      "var hasError = textContent.indexOf('ERR_PROXY_CONNECTION_FAILED') >= 0;"
      "domAutomationController.send(hasError);",
      &result));
  EXPECT_TRUE(result);
}

// This class observes chrome::NOTIFICATION_AUTH_NEEDED and supplies
// the credential which is required by the test proxy server.
// "foo:bar" is the required username and password for our test proxy server.
class LoginPromptObserver : public content::NotificationObserver {
 public:
  LoginPromptObserver() : auth_handled_(false) {}

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    if (type == chrome::NOTIFICATION_AUTH_NEEDED) {
      LoginNotificationDetails* login_details =
          content::Details<LoginNotificationDetails>(details).ptr();
      // |login_details->handler()| is the associated LoginHandler object.
      // SetAuth() will close the login dialog.
      login_details->handler()->SetAuth(base::ASCIIToUTF16("foo"),
                                        base::ASCIIToUTF16("bar"));
      auth_handled_ = true;
    }
  }

  bool auth_handled() const { return auth_handled_; }

 private:
  bool auth_handled_;

  DISALLOW_COPY_AND_ASSIGN(LoginPromptObserver);
};

class ProxyBrowserTest : public InProcessBrowserTest {
 public:
  ProxyBrowserTest()
      : proxy_server_(net::SpawnedTestServer::TYPE_BASIC_AUTH_PROXY,
                      net::SpawnedTestServer::kLocalhost,
                      base::FilePath()) {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(proxy_server_.Start());
    InProcessBrowserTest::SetUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kProxyServer,
                                    proxy_server_.host_port_pair().ToString());
  }

 protected:
  net::SpawnedTestServer proxy_server_;

 private:

  DISALLOW_COPY_AND_ASSIGN(ProxyBrowserTest);
};

#if defined(OS_CHROMEOS)
// We bypass manually installed proxy for localhost on chromeos.
#define MAYBE_BasicAuthWSConnect DISABLED_BasicAuthWSConnect
#else
#define MAYBE_BasicAuthWSConnect BasicAuthWSConnect
#endif
// Test that the browser can establish a WebSocket connection via a proxy
// that requires basic authentication.
IN_PROC_BROWSER_TEST_F(ProxyBrowserTest, MAYBE_BasicAuthWSConnect) {
  // Launch WebSocket server.
  net::SpawnedTestServer ws_server(net::SpawnedTestServer::TYPE_WS,
                                   net::SpawnedTestServer::kLocalhost,
                                   net::GetWebSocketTestDataDirectory());
  ASSERT_TRUE(ws_server.Start());

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController* controller = &tab->GetController();
  content::NotificationRegistrar registrar;
  // The proxy server will request basic authentication.
  // |observer| supplies the credential.
  LoginPromptObserver observer;
  registrar.Add(&observer, chrome::NOTIFICATION_AUTH_NEEDED,
                content::Source<content::NavigationController>(controller));

  content::TitleWatcher watcher(tab, base::ASCIIToUTF16("PASS"));
  watcher.AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));

  // Visit a page that tries to establish WebSocket connection. The title
  // of the page will be 'PASS' on success.
  std::string scheme("http");
  GURL::Replacements replacements;
  replacements.SetSchemeStr(scheme);
  ui_test_utils::NavigateToURL(
      browser(),
      ws_server.GetURL("connect_check.html").ReplaceComponents(replacements));

  const base::string16 result = watcher.WaitAndGetTitle();
  EXPECT_TRUE(EqualsASCII(result, "PASS"));
  EXPECT_TRUE(observer.auth_handled());
}

// Fetch PAC script via an http:// URL.
class HttpProxyScriptBrowserTest : public InProcessBrowserTest {
 public:
  HttpProxyScriptBrowserTest()
      : http_server_(net::SpawnedTestServer::TYPE_HTTP,
                     net::SpawnedTestServer::kLocalhost,
                     base::FilePath(FILE_PATH_LITERAL("chrome/test/data"))) {
  }
  virtual ~HttpProxyScriptBrowserTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(http_server_.Start());
    InProcessBrowserTest::SetUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    base::FilePath pac_script_path(FILE_PATH_LITERAL("files"));
    command_line->AppendSwitchASCII(switches::kProxyPacUrl, http_server_.GetURL(
        pac_script_path.Append(kPACScript).MaybeAsASCII()).spec());
  }

 private:
  net::SpawnedTestServer http_server_;

  DISALLOW_COPY_AND_ASSIGN(HttpProxyScriptBrowserTest);
};

IN_PROC_BROWSER_TEST_F(HttpProxyScriptBrowserTest, Verify) {
  VerifyProxyScript(browser());
}

// Fetch PAC script via a file:// URL.
class FileProxyScriptBrowserTest : public InProcessBrowserTest {
 public:
  FileProxyScriptBrowserTest() {}
  virtual ~FileProxyScriptBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitchASCII(switches::kProxyPacUrl,
        ui_test_utils::GetTestUrl(
            base::FilePath(base::FilePath::kCurrentDirectory),
            base::FilePath(kPACScript)).spec());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FileProxyScriptBrowserTest);
};

IN_PROC_BROWSER_TEST_F(FileProxyScriptBrowserTest, Verify) {
  VerifyProxyScript(browser());
}

// Fetch PAC script via an ftp:// URL.
class FtpProxyScriptBrowserTest : public InProcessBrowserTest {
 public:
  FtpProxyScriptBrowserTest()
      : ftp_server_(net::SpawnedTestServer::TYPE_FTP,
                    net::SpawnedTestServer::kLocalhost,
                    base::FilePath(FILE_PATH_LITERAL("chrome/test/data"))) {
  }
  virtual ~FtpProxyScriptBrowserTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(ftp_server_.Start());
    InProcessBrowserTest::SetUp();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    base::FilePath pac_script_path(kPACScript);
    command_line->AppendSwitchASCII(
        switches::kProxyPacUrl,
        ftp_server_.GetURL(pac_script_path.MaybeAsASCII()).spec());
  }

 private:
  net::SpawnedTestServer ftp_server_;

  DISALLOW_COPY_AND_ASSIGN(FtpProxyScriptBrowserTest);
};

IN_PROC_BROWSER_TEST_F(FtpProxyScriptBrowserTest, Verify) {
  VerifyProxyScript(browser());
}

// Fetch PAC script via a data: URL.
class DataProxyScriptBrowserTest : public InProcessBrowserTest {
 public:
  DataProxyScriptBrowserTest() {}
  virtual ~DataProxyScriptBrowserTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    std::string contents;
    // Read in kPACScript contents.
    ASSERT_TRUE(base::ReadFileToString(ui_test_utils::GetTestFilePath(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kPACScript)),
        &contents));
    command_line->AppendSwitchASCII(switches::kProxyPacUrl,
        std::string("data:,") + contents);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DataProxyScriptBrowserTest);
};

IN_PROC_BROWSER_TEST_F(DataProxyScriptBrowserTest, Verify) {
  VerifyProxyScript(browser());
}

}  // namespace
