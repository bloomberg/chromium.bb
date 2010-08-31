// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#include "base/string_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/extensions/extensions_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/test/ui_test_utils.h"

ExtensionApiTest::ResultCatcher::ResultCatcher()
    : profile_restriction_(NULL) {
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
  if (results_.empty())
    ui_test_utils::RunMessageLoop();

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
      std::cout << "Got EXTENSION_TEST_PASSED notification.\n";
      results_.push_back(true);
      messages_.push_back("");
      MessageLoopForUI::current()->Quit();
      break;

    case NotificationType::EXTENSION_TEST_FAILED:
      std::cout << "Got EXTENSION_TEST_FAILED notification.\n";
      results_.push_back(false);
      messages_.push_back(*(Details<std::string>(details).ptr()));
      MessageLoopForUI::current()->Quit();
      break;

    default:
      NOTREACHED();
  }
}

bool ExtensionApiTest::RunExtensionTest(const char* extension_name) {
  return RunExtensionTestImpl(extension_name, "");
}

bool ExtensionApiTest::RunExtensionSubtest(const char* extension_name,
                                           const std::string& subtest_page) {
  DCHECK(!subtest_page.empty()) << "Argument subtest_page is required.";
  return RunExtensionTestImpl(extension_name, subtest_page);
}

// Load an extension and wait for it to notify of PASSED or FAILED.
bool ExtensionApiTest::RunExtensionTestImpl(const char* extension_name,
                                            const std::string& subtest_page) {
  ResultCatcher catcher;
  LOG(INFO) << "Running ExtensionApiTest with: " << extension_name;

  if (!LoadExtension(test_data_dir_.AppendASCII(extension_name))) {
    message_ = "Failed to load extension.";
    return false;
  }

  // If there is a subtest to load, navigate to the subtest page.
  if (!subtest_page.empty()) {
    ExtensionsService* service = browser()->profile()->GetExtensionsService();
    Extension* extension =
        service->GetExtensionById(last_loaded_extension_id_, false);
    if (!extension)
      return false;

    GURL url = extension->GetResourceURL(subtest_page);
    LOG(ERROR) << "Loading subtest page url: " << url.spec();
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
Extension* ExtensionApiTest::GetSingleLoadedExtension() {
  ExtensionsService* service = browser()->profile()->GetExtensionsService();

  int found_extension_index = -1;
  for (size_t i = 0; i < service->extensions()->size(); ++i) {
    // Ignore any component extensions. They are automatically loaded into all
    // profiles and aren't the extension we're looking for here.
    if (service->extensions()->at(i)->location() == Extension::COMPONENT)
      continue;

    if (found_extension_index != -1) {
      message_ = StringPrintf(
          "Expected only one extension to be present.  Found %u.",
          static_cast<unsigned>(service->extensions()->size()));
      return NULL;
    }

    found_extension_index = static_cast<int>(i);
  }

  Extension* extension = service->extensions()->at(found_extension_index);
  if (!extension) {
    message_ = "extension pointer is NULL.";
    return NULL;
  }
  return extension;
}

void ExtensionApiTest::SetUpCommandLine(CommandLine* command_line) {
  ExtensionBrowserTest::SetUpCommandLine(command_line);
  test_data_dir_ = test_data_dir_.AppendASCII("api_test");
}
