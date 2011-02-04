// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "base/string_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/extensions/extension_test_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/test/ui_test_utils.h"

namespace {

const char kTestServerPort[] = "testServer.port";

};  // namespace

ExtensionApiTest::ResultCatcher::ResultCatcher()
    : profile_restriction_(NULL),
      waiting_(false) {
  registrar_.Add(this, NotificationType::EXTENSION_TEST_PASSED,
                 NotificationService::AllSources());
  registrar_.Add(this, NotificationType::EXTENSION_TEST_FAILED,
                 NotificationService::AllSources());
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
    ui_test_utils::RunMessageLoop();
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
    NotificationType type, const NotificationSource& source,
    const NotificationDetails& details) {
  if (profile_restriction_ &&
      Source<Profile>(source).ptr() != profile_restriction_) {
    return;
  }

  switch (type.value) {
    case NotificationType::EXTENSION_TEST_PASSED:
      VLOG(1) << "Got EXTENSION_TEST_PASSED notification.";
      results_.push_back(true);
      messages_.push_back("");
      if (waiting_)
        MessageLoopForUI::current()->Quit();
      break;

    case NotificationType::EXTENSION_TEST_FAILED:
      VLOG(1) << "Got EXTENSION_TEST_FAILED notification.";
      results_.push_back(false);
      messages_.push_back(*(Details<std::string>(details).ptr()));
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
  ExtensionTestGetConfigFunction::set_test_config_state(test_config_.get());
}

void ExtensionApiTest::TearDownInProcessBrowserTestFixture() {
  ExtensionTestGetConfigFunction::set_test_config_state(NULL);
  test_config_.reset(NULL);
}

bool ExtensionApiTest::RunExtensionTest(const char* extension_name) {
  return RunExtensionTestImpl(extension_name, "", false);
}

bool ExtensionApiTest::RunExtensionTestIncognito(const char* extension_name) {
  return RunExtensionTestImpl(extension_name, "", true);
}

bool ExtensionApiTest::RunExtensionSubtest(const char* extension_name,
                                           const std::string& page_url) {
  DCHECK(!page_url.empty()) << "Argument page_url is required.";
  return RunExtensionTestImpl(extension_name, page_url, false);
}

bool ExtensionApiTest::RunPageTest(const std::string& page_url) {
  return RunExtensionSubtest("", page_url);
}

// Load |extension_name| extension and/or |page_url| and wait for
// PASSED or FAILED notification.
bool ExtensionApiTest::RunExtensionTestImpl(const char* extension_name,
                                            const std::string& page_url,
                                            bool enable_incognito) {
  ResultCatcher catcher;
  DCHECK(!std::string(extension_name).empty() || !page_url.empty()) <<
      "extension_name and page_url cannot both be empty";

  if (!std::string(extension_name).empty()) {
    bool loaded = enable_incognito ?
        LoadExtensionIncognito(test_data_dir_.AppendASCII(extension_name)) :
        LoadExtension(test_data_dir_.AppendASCII(extension_name));
    if (!loaded) {
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

      ExtensionService* service = browser()->profile()->GetExtensionService();
      const Extension* extension =
          service->GetExtensionById(last_loaded_extension_id_, false);
      if (!extension)
        return false;

      url = extension->GetResourceURL(page_url);
    }

    ui_test_utils::NavigateToURL(browser(), url);
  }

  if (!catcher.GetNextResult()) {
    message_ = catcher.message();
    return false;
  } else {
    return true;
  }
}

// Test that exactly one extension loaded.
const Extension* ExtensionApiTest::GetSingleLoadedExtension() {
  ExtensionService* service = browser()->profile()->GetExtensionService();

  int found_extension_index = -1;
  for (size_t i = 0; i < service->extensions()->size(); ++i) {
    // Ignore any component extensions. They are automatically loaded into all
    // profiles and aren't the extension we're looking for here.
    if (service->extensions()->at(i)->location() == Extension::COMPONENT)
      continue;

    if (found_extension_index != -1) {
      message_ = base::StringPrintf(
          "Expected only one extension to be present.  Found %u.",
          static_cast<unsigned>(service->extensions()->size()));
      return NULL;
    }

    found_extension_index = static_cast<int>(i);
  }

  const Extension* extension = service->extensions()->at(found_extension_index);
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
  // access the test server.  Tests can see these values using the extension
  // API function chrome.test.getConfig().
  test_config_->SetInteger(kTestServerPort,
                           test_server()->host_port_pair().port());

  return true;
}

void ExtensionApiTest::SetUpCommandLine(CommandLine* command_line) {
  ExtensionBrowserTest::SetUpCommandLine(command_line);
  test_data_dir_ = test_data_dir_.AppendASCII("api_test");
}
