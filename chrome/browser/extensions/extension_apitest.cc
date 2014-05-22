// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "net/base/escape.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/spawned_test_server/spawned_test_server.h"

namespace {

const char kTestCustomArg[] = "customArg";
const char kTestServerPort[] = "testServer.port";
const char kTestDataDirectory[] = "testDataDirectory";
const char kTestWebSocketPort[] = "testWebSocketPort";
const char kFtpServerPort[] = "ftpServer.port";
const char kSpawnedTestServerPort[] = "spawnedTestServer.port";

scoped_ptr<net::test_server::HttpResponse> HandleServerRedirectRequest(
    const net::test_server::HttpRequest& request) {
  if (!StartsWithASCII(request.relative_url, "/server-redirect?", true))
    return scoped_ptr<net::test_server::HttpResponse>();

  size_t query_string_pos = request.relative_url.find('?');
  std::string redirect_target =
      request.relative_url.substr(query_string_pos + 1);

  scoped_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", redirect_target);
  return http_response.PassAs<net::test_server::HttpResponse>();
}

scoped_ptr<net::test_server::HttpResponse> HandleEchoHeaderRequest(
    const net::test_server::HttpRequest& request) {
  if (!StartsWithASCII(request.relative_url, "/echoheader?", true))
    return scoped_ptr<net::test_server::HttpResponse>();

  size_t query_string_pos = request.relative_url.find('?');
  std::string header_name =
      request.relative_url.substr(query_string_pos + 1);

  std::string header_value;
  std::map<std::string, std::string>::const_iterator it = request.headers.find(
      header_name);
  if (it != request.headers.end())
    header_value = it->second;

  scoped_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(header_value);
  return http_response.PassAs<net::test_server::HttpResponse>();
}

scoped_ptr<net::test_server::HttpResponse> HandleSetCookieRequest(
    const net::test_server::HttpRequest& request) {
  if (!StartsWithASCII(request.relative_url, "/set-cookie?", true))
    return scoped_ptr<net::test_server::HttpResponse>();

  scoped_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);

  size_t query_string_pos = request.relative_url.find('?');
  std::string cookie_value =
      request.relative_url.substr(query_string_pos + 1);

  std::vector<std::string> cookies;
  base::SplitString(cookie_value, '&', &cookies);

  for (size_t i = 0; i < cookies.size(); i++)
    http_response->AddCustomHeader("Set-Cookie", cookies[i]);

  return http_response.PassAs<net::test_server::HttpResponse>();
}

scoped_ptr<net::test_server::HttpResponse> HandleSetHeaderRequest(
    const net::test_server::HttpRequest& request) {
  if (!StartsWithASCII(request.relative_url, "/set-header?", true))
    return scoped_ptr<net::test_server::HttpResponse>();

  size_t query_string_pos = request.relative_url.find('?');
  std::string escaped_header =
      request.relative_url.substr(query_string_pos + 1);

  std::string header =
      net::UnescapeURLComponent(escaped_header,
                                net::UnescapeRule::NORMAL |
                                net::UnescapeRule::SPACES |
                                net::UnescapeRule::URL_SPECIAL_CHARS);

  size_t colon_pos = header.find(':');
  if (colon_pos == std::string::npos)
    return scoped_ptr<net::test_server::HttpResponse>();

  std::string header_name = header.substr(0, colon_pos);
  // Skip space after colon.
  std::string header_value = header.substr(colon_pos + 2);

  scoped_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);
  http_response->AddCustomHeader(header_name, header_value);
  return http_response.PassAs<net::test_server::HttpResponse>();
}

};  // namespace

ExtensionApiTest::ExtensionApiTest() {
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&HandleServerRedirectRequest));
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&HandleEchoHeaderRequest));
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&HandleSetCookieRequest));
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&HandleSetHeaderRequest));
}

ExtensionApiTest::~ExtensionApiTest() {}

ExtensionApiTest::ResultCatcher::ResultCatcher()
    : profile_restriction_(NULL),
      waiting_(false) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_TEST_PASSED,
                 content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_TEST_FAILED,
                 content::NotificationService::AllSources());
}

ExtensionApiTest::ResultCatcher::~ResultCatcher() {
}

bool ExtensionApiTest::ResultCatcher::GetNextResult() {
  // Depending on the tests, multiple results can come in from a single call
  // to RunMessageLoop(), so we maintain a queue of results and just pull them
  // off as the test calls this, going to the run loop only when the queue is
  // empty.
  if (results_.empty()) {
    waiting_ = true;
    content::RunMessageLoop();
    waiting_ = false;
  }

  if (!results_.empty()) {
    bool ret = results_.front();
    results_.pop_front();
    message_ = messages_.front();
    messages_.pop_front();
    return ret;
  }

  NOTREACHED();
  return false;
}

void ExtensionApiTest::ResultCatcher::Observe(
    int type, const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (profile_restriction_ &&
      content::Source<Profile>(source).ptr() != profile_restriction_) {
    return;
  }

  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_TEST_PASSED:
      VLOG(1) << "Got EXTENSION_TEST_PASSED notification.";
      results_.push_back(true);
      messages_.push_back(std::string());
      if (waiting_)
        base::MessageLoopForUI::current()->Quit();
      break;

    case chrome::NOTIFICATION_EXTENSION_TEST_FAILED:
      VLOG(1) << "Got EXTENSION_TEST_FAILED notification.";
      results_.push_back(false);
      messages_.push_back(*(content::Details<std::string>(details).ptr()));
      if (waiting_)
        base::MessageLoopForUI::current()->Quit();
      break;

    default:
      NOTREACHED();
  }
}

void ExtensionApiTest::SetUpInProcessBrowserTestFixture() {
  DCHECK(!test_config_.get()) << "Previous test did not clear config state.";
  test_config_.reset(new base::DictionaryValue());
  test_config_->SetString(kTestDataDirectory,
                          net::FilePathToFileURL(test_data_dir_).spec());
  test_config_->SetInteger(kTestWebSocketPort, 0);
  extensions::TestGetConfigFunction::set_test_config_state(
      test_config_.get());
}

void ExtensionApiTest::TearDownInProcessBrowserTestFixture() {
  extensions::TestGetConfigFunction::set_test_config_state(NULL);
  test_config_.reset(NULL);
}

bool ExtensionApiTest::RunExtensionTest(const std::string& extension_name) {
  return RunExtensionTestImpl(
      extension_name, std::string(), NULL, kFlagEnableFileAccess);
}

bool ExtensionApiTest::RunExtensionTestIncognito(
    const std::string& extension_name) {
  return RunExtensionTestImpl(extension_name,
                              std::string(),
                              NULL,
                              kFlagEnableIncognito | kFlagEnableFileAccess);
}

bool ExtensionApiTest::RunExtensionTestIgnoreManifestWarnings(
    const std::string& extension_name) {
  return RunExtensionTestImpl(
      extension_name, std::string(), NULL, kFlagIgnoreManifestWarnings);
}

bool ExtensionApiTest::RunExtensionTestAllowOldManifestVersion(
    const std::string& extension_name) {
  return RunExtensionTestImpl(
      extension_name,
      std::string(),
      NULL,
      kFlagEnableFileAccess | kFlagAllowOldManifestVersions);
}

bool ExtensionApiTest::RunComponentExtensionTest(
    const std::string& extension_name) {
  return RunExtensionTestImpl(extension_name,
                              std::string(),
                              NULL,
                              kFlagEnableFileAccess | kFlagLoadAsComponent);
}

bool ExtensionApiTest::RunExtensionTestNoFileAccess(
    const std::string& extension_name) {
  return RunExtensionTestImpl(extension_name, std::string(), NULL, kFlagNone);
}

bool ExtensionApiTest::RunExtensionTestIncognitoNoFileAccess(
    const std::string& extension_name) {
  return RunExtensionTestImpl(
      extension_name, std::string(), NULL, kFlagEnableIncognito);
}

bool ExtensionApiTest::RunExtensionSubtest(const std::string& extension_name,
                                           const std::string& page_url) {
  return RunExtensionSubtest(extension_name, page_url, kFlagEnableFileAccess);
}

bool ExtensionApiTest::RunExtensionSubtest(const std::string& extension_name,
                                           const std::string& page_url,
                                           int flags) {
  DCHECK(!page_url.empty()) << "Argument page_url is required.";
  // See http://crbug.com/177163 for details.
#if defined(OS_WIN) && !defined(NDEBUG)
  LOG(WARNING) << "Workaround for 177163, prematurely returning";
  return true;
#else
  return RunExtensionTestImpl(extension_name, page_url, NULL, flags);
#endif
}


bool ExtensionApiTest::RunPageTest(const std::string& page_url) {
  return RunExtensionSubtest(std::string(), page_url);
}

bool ExtensionApiTest::RunPageTest(const std::string& page_url,
                                   int flags) {
  return RunExtensionSubtest(std::string(), page_url, flags);
}

bool ExtensionApiTest::RunPlatformAppTest(const std::string& extension_name) {
  return RunExtensionTestImpl(
      extension_name, std::string(), NULL, kFlagLaunchPlatformApp);
}

bool ExtensionApiTest::RunPlatformAppTestWithArg(
    const std::string& extension_name, const char* custom_arg) {
  return RunExtensionTestImpl(
      extension_name, std::string(), custom_arg, kFlagLaunchPlatformApp);
}

bool ExtensionApiTest::RunPlatformAppTestWithFlags(
    const std::string& extension_name, int flags) {
  return RunExtensionTestImpl(
      extension_name, std::string(), NULL, flags | kFlagLaunchPlatformApp);
}

// Load |extension_name| extension and/or |page_url| and wait for
// PASSED or FAILED notification.
bool ExtensionApiTest::RunExtensionTestImpl(const std::string& extension_name,
                                            const std::string& page_url,
                                            const char* custom_arg,
                                            int flags) {
  bool load_as_component = (flags & kFlagLoadAsComponent) != 0;
  bool launch_platform_app = (flags & kFlagLaunchPlatformApp) != 0;
  bool use_incognito = (flags & kFlagUseIncognito) != 0;

  if (custom_arg && custom_arg[0])
    test_config_->SetString(kTestCustomArg, custom_arg);

  ResultCatcher catcher;
  DCHECK(!extension_name.empty() || !page_url.empty()) <<
      "extension_name and page_url cannot both be empty";

  const extensions::Extension* extension = NULL;
  if (!extension_name.empty()) {
    base::FilePath extension_path = test_data_dir_.AppendASCII(extension_name);
    if (load_as_component) {
      extension = LoadExtensionAsComponent(extension_path);
    } else {
      int browser_test_flags = ExtensionBrowserTest::kFlagNone;
      if (flags & kFlagEnableIncognito)
        browser_test_flags |= ExtensionBrowserTest::kFlagEnableIncognito;
      if (flags & kFlagEnableFileAccess)
        browser_test_flags |= ExtensionBrowserTest::kFlagEnableFileAccess;
      if (flags & kFlagIgnoreManifestWarnings)
        browser_test_flags |= ExtensionBrowserTest::kFlagIgnoreManifestWarnings;
      if (flags & kFlagAllowOldManifestVersions) {
        browser_test_flags |=
            ExtensionBrowserTest::kFlagAllowOldManifestVersions;
      }
      extension = LoadExtensionWithFlags(extension_path, browser_test_flags);
    }
    if (!extension) {
      message_ = "Failed to load extension.";
      return false;
    }
  }

  // If there is a page_url to load, navigate it.
  if (!page_url.empty()) {
    GURL url = GURL(page_url);

    // Note: We use is_valid() here in the expectation that the provided url
    // may lack a scheme & host and thus be a relative url within the loaded
    // extension.
    if (!url.is_valid()) {
      DCHECK(!extension_name.empty()) <<
          "Relative page_url given with no extension_name";

      url = extension->GetResourceURL(page_url);
    }

    if (use_incognito)
      ui_test_utils::OpenURLOffTheRecord(browser()->profile(), url);
    else
      ui_test_utils::NavigateToURL(browser(), url);
  } else if (launch_platform_app) {
    AppLaunchParams params(browser()->profile(),
                           extension,
                           extensions::LAUNCH_CONTAINER_NONE,
                           NEW_WINDOW);
    params.command_line = *CommandLine::ForCurrentProcess();
    OpenApplication(params);
  }

  if (!catcher.GetNextResult()) {
    message_ = catcher.message();
    return false;
  }

  return true;
}

// Test that exactly one extension is loaded, and return it.
const extensions::Extension* ExtensionApiTest::GetSingleLoadedExtension() {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();

  const extensions::Extension* extension = NULL;
  for (extensions::ExtensionSet::const_iterator it =
           service->extensions()->begin();
       it != service->extensions()->end(); ++it) {
    // Ignore any component extensions. They are automatically loaded into all
    // profiles and aren't the extension we're looking for here.
    if ((*it)->location() == extensions::Manifest::COMPONENT)
      continue;

    if (extension != NULL) {
      // TODO(yoz): this is misleading; it counts component extensions.
      message_ = base::StringPrintf(
          "Expected only one extension to be present.  Found %u.",
          static_cast<unsigned>(service->extensions()->size()));
      return NULL;
    }

    extension = it->get();
  }

  if (!extension) {
    message_ = "extension pointer is NULL.";
    return NULL;
  }
  return extension;
}

bool ExtensionApiTest::StartEmbeddedTestServer() {
  if (!embedded_test_server()->InitializeAndWaitUntilReady())
    return false;

  // Build a dictionary of values that tests can use to build URLs that
  // access the test server and local file system.  Tests can see these values
  // using the extension API function chrome.test.getConfig().
  test_config_->SetInteger(kTestServerPort,
                           embedded_test_server()->port());

  return true;
}

bool ExtensionApiTest::StartWebSocketServer(
    const base::FilePath& root_directory) {
  websocket_server_.reset(new net::SpawnedTestServer(
      net::SpawnedTestServer::TYPE_WS,
      net::SpawnedTestServer::kLocalhost,
      root_directory));

  if (!websocket_server_->Start())
    return false;

  test_config_->SetInteger(kTestWebSocketPort,
                           websocket_server_->host_port_pair().port());

  return true;
}

bool ExtensionApiTest::StartFTPServer(const base::FilePath& root_directory) {
  ftp_server_.reset(new net::SpawnedTestServer(
      net::SpawnedTestServer::TYPE_FTP,
      net::SpawnedTestServer::kLocalhost,
      root_directory));

  if (!ftp_server_->Start())
    return false;

  test_config_->SetInteger(kFtpServerPort,
                           ftp_server_->host_port_pair().port());

  return true;
}

bool ExtensionApiTest::StartSpawnedTestServer() {
  if (!test_server()->Start())
    return false;

  // Build a dictionary of values that tests can use to build URLs that
  // access the test server and local file system.  Tests can see these values
  // using the extension API function chrome.test.getConfig().
  test_config_->SetInteger(kSpawnedTestServerPort,
                           test_server()->host_port_pair().port());

  return true;
}

void ExtensionApiTest::SetUpCommandLine(CommandLine* command_line) {
  ExtensionBrowserTest::SetUpCommandLine(command_line);
  test_data_dir_ = test_data_dir_.AppendASCII("api_test");
}
