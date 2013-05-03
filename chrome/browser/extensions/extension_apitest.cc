// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/api/test/test_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "net/base/net_util.h"
#include "net/test/spawned_test_server.h"

namespace {

const char kTestServerPort[] = "testServer.port";
const char kTestDataDirectory[] = "testDataDirectory";
const char kTestWebSocketPort[] = "testWebSocketPort";

};  // namespace

ExtensionApiTest::ExtensionApiTest() {}

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
        MessageLoopForUI::current()->Quit();
      break;

    case chrome::NOTIFICATION_EXTENSION_TEST_FAILED:
      VLOG(1) << "Got EXTENSION_TEST_FAILED notification.";
      results_.push_back(false);
      messages_.push_back(*(content::Details<std::string>(details).ptr()));
      if (waiting_)
        MessageLoopForUI::current()->Quit();
      break;

    default:
      NOTREACHED();
  }
}

void ExtensionApiTest::SetUpInProcessBrowserTestFixture() {
  DCHECK(!test_config_.get()) << "Previous test did not clear config state.";
  test_config_.reset(new DictionaryValue());
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

bool ExtensionApiTest::RunExtensionTest(const char* extension_name) {
  return RunExtensionTestImpl(
      extension_name, std::string(), kFlagEnableFileAccess);
}

bool ExtensionApiTest::RunExtensionTestIncognito(const char* extension_name) {
  return RunExtensionTestImpl(extension_name,
                              std::string(),
                              kFlagEnableIncognito | kFlagEnableFileAccess);
}

bool ExtensionApiTest::RunExtensionTestIgnoreManifestWarnings(
    const char* extension_name) {
  return RunExtensionTestImpl(
      extension_name, std::string(), kFlagIgnoreManifestWarnings);
}

bool ExtensionApiTest::RunExtensionTestAllowOldManifestVersion(
    const char* extension_name) {
  return RunExtensionTestImpl(
      extension_name,
      std::string(),
      kFlagEnableFileAccess | kFlagAllowOldManifestVersions);
}

bool ExtensionApiTest::RunComponentExtensionTest(const char* extension_name) {
  return RunExtensionTestImpl(extension_name,
                              std::string(),
                              kFlagEnableFileAccess | kFlagLoadAsComponent);
}

bool ExtensionApiTest::RunExtensionTestNoFileAccess(
    const char* extension_name) {
  return RunExtensionTestImpl(extension_name, std::string(), kFlagNone);
}

bool ExtensionApiTest::RunExtensionTestIncognitoNoFileAccess(
    const char* extension_name) {
  return RunExtensionTestImpl(
      extension_name, std::string(), kFlagEnableIncognito);
}

bool ExtensionApiTest::RunExtensionSubtest(const char* extension_name,
                                           const std::string& page_url) {
  return RunExtensionSubtest(extension_name, page_url, kFlagEnableFileAccess);
}

bool ExtensionApiTest::RunExtensionSubtest(const char* extension_name,
                                           const std::string& page_url,
                                           int flags) {
  DCHECK(!page_url.empty()) << "Argument page_url is required.";
  return RunExtensionTestImpl(extension_name, page_url, flags);
}


bool ExtensionApiTest::RunPageTest(const std::string& page_url) {
  return RunExtensionSubtest("", page_url);
}

bool ExtensionApiTest::RunPageTest(const std::string& page_url,
                                   int flags) {
  return RunExtensionSubtest("", page_url, flags);
}

bool ExtensionApiTest::RunPlatformAppTest(const char* extension_name) {
  return RunExtensionTestImpl(
      extension_name, std::string(), kFlagLaunchPlatformApp);
}

// Load |extension_name| extension and/or |page_url| and wait for
// PASSED or FAILED notification.
bool ExtensionApiTest::RunExtensionTestImpl(const char* extension_name,
                                            const std::string& page_url,
                                            int flags) {
  bool load_as_component = (flags & kFlagLoadAsComponent) != 0;
  bool launch_platform_app = (flags & kFlagLaunchPlatformApp) != 0;
  bool use_incognito = (flags & kFlagUseIncognito) != 0;

  ResultCatcher catcher;
  DCHECK(!std::string(extension_name).empty() || !page_url.empty()) <<
      "extension_name and page_url cannot both be empty";

  const extensions::Extension* extension = NULL;
  if (!std::string(extension_name).empty()) {
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
      DCHECK(!std::string(extension_name).empty()) <<
          "Relative page_url given with no extension_name";

      url = extension->GetResourceURL(page_url);
    }

    if (use_incognito)
      ui_test_utils::OpenURLOffTheRecord(browser()->profile(), url);
    else
      ui_test_utils::NavigateToURL(browser(), url);

  } else if (launch_platform_app) {
    chrome::AppLaunchParams params(browser()->profile(), extension,
                                   extension_misc::LAUNCH_NONE,
                                   NEW_WINDOW);
    params.command_line = CommandLine::ForCurrentProcess();
    chrome::OpenApplication(params);
  }

  if (!catcher.GetNextResult()) {
    message_ = catcher.message();
    return false;
  } else {
    return true;
  }
}

// Test that exactly one extension is loaded, and return it.
const extensions::Extension* ExtensionApiTest::GetSingleLoadedExtension() {
  ExtensionService* service = extensions::ExtensionSystem::Get(
      browser()->profile())->extension_service();

  const extensions::Extension* extension = NULL;
  for (ExtensionSet::const_iterator it = service->extensions()->begin();
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

    extension = *it;
  }

  if (!extension) {
    message_ = "extension pointer is NULL.";
    return NULL;
  }
  return extension;
}

bool ExtensionApiTest::StartTestServer() {
  if (!test_server()->Start())
    return false;

  // Build a dictionary of values that tests can use to build URLs that
  // access the test server and local file system.  Tests can see these values
  // using the extension API function chrome.test.getConfig().
  test_config_->SetInteger(kTestServerPort,
                           test_server()->host_port_pair().port());

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

void ExtensionApiTest::SetUpCommandLine(CommandLine* command_line) {
  ExtensionBrowserTest::SetUpCommandLine(command_line);
  test_data_dir_ = test_data_dir_.AppendASCII("api_test");
}
