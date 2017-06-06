// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/scoped_ignore_content_verifier_for_test.h"
#include "extensions/test/result_catcher.h"

namespace extensions {

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Events) {
  ASSERT_TRUE(RunExtensionTest("events")) << message_;
}

// Tests that events are unregistered when an extension page shuts down.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, EventsAreUnregistered) {
  // In this test, page1.html registers for a number of events, then navigates
  // to page2.html, which should unregister those events. page2.html notifies
  // pass, by which point the event should have been unregistered.
  //
  // This test relies on the extension installed with RunExtensionSubtest
  // actually being installed, which won't happen if subtests are skipped (see
  // comment on ExtensionApiTest::ExtensionSubtestsAreSkipped)
  if (ExtensionSubtestsAreSkipped())
    return;

  EventRouter* event_router = EventRouter::Get(profile());
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());

  std::string test_extension_name = "events_are_unregistered";
  ASSERT_TRUE(RunExtensionSubtest(test_extension_name, "page1.html"))
      << message_;

  // Find the extension we just installed by looking for the path.
  base::FilePath extension_path =
      test_data_dir_.AppendASCII(test_extension_name);
  const Extension* extension =
      GetExtensionByPath(registry->enabled_extensions(), extension_path);
  ASSERT_TRUE(extension) << "No extension found at \"" << extension_path.value()
                         << "\" (absolute path \""
                         << base::MakeAbsoluteFilePath(extension_path).value()
                         << "\")";
  const std::string& id = extension->id();

  // The page has closed, so no matter what all events are no longer listened
  // to. Assertions for normal events:
  EXPECT_FALSE(
      event_router->ExtensionHasEventListener(id, "browserAction.onClicked"));
  EXPECT_FALSE(
      event_router->ExtensionHasEventListener(id, "runtime.onStartup"));
  EXPECT_FALSE(
      event_router->ExtensionHasEventListener(id, "runtime.onSuspend"));
  EXPECT_FALSE(
      event_router->ExtensionHasEventListener(id, "runtime.onInstalled"));
  // Assertions for filtered events:
  EXPECT_FALSE(event_router->ExtensionHasEventListener(
      id, "webNavigation.onBeforeNavigate"));
  EXPECT_FALSE(
      event_router->ExtensionHasEventListener(id, "webNavigation.onCommitted"));
  EXPECT_FALSE(event_router->ExtensionHasEventListener(
      id, "webNavigation.onDOMContentLoaded"));
  EXPECT_FALSE(
      event_router->ExtensionHasEventListener(id, "webNavigation.onCompleted"));
}

class EventsApiTest : public ExtensionApiTest {
 public:
  EventsApiTest() {}

 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  struct ExtensionCRXData {
    std::string unpacked_relative_path;
    base::FilePath crx_path;
    explicit ExtensionCRXData(const std::string& unpacked_relative_path)
        : unpacked_relative_path(unpacked_relative_path) {}
  };

  void SetUpCRX(const std::string& root_dir,
                const std::string& pem_filename,
                std::vector<ExtensionCRXData>* crx_data_list) {
    const base::FilePath test_dir = test_data_dir_.AppendASCII(root_dir);
    const base::FilePath pem_path = test_dir.AppendASCII(pem_filename);
    for (ExtensionCRXData& crx_data : *crx_data_list) {
      crx_data.crx_path = PackExtensionWithOptions(
          test_dir.AppendASCII(crx_data.unpacked_relative_path),
          scoped_temp_dir_.GetPath().AppendASCII(
              crx_data.unpacked_relative_path + ".crx"),
          pem_path, base::FilePath());
    }
  }

 private:
  base::ScopedTempDir scoped_temp_dir_;
  ScopedIgnoreContentVerifierForTest ignore_content_verification_;

  DISALLOW_COPY_AND_ASSIGN(EventsApiTest);
};

// Tests that updating an extension sends runtime.onInstalled event to the
// updated extension.
IN_PROC_BROWSER_TEST_F(EventsApiTest, ExtensionUpdateSendsOnInstalledEvent) {
  std::vector<ExtensionCRXData> data;
  data.emplace_back("v1");
  data.emplace_back("v2");
  SetUpCRX("lazy_events/on_installed", "pem.pem", &data);

  ExtensionId extension_id;
  {
    // Install version 1 of the extension and expect runtime.onInstalled.
    ResultCatcher catcher;
    const int expected_change = 1;
    const Extension* extension_v1 =
        InstallExtension(data[0].crx_path, expected_change);
    extension_id = extension_v1->id();
    ASSERT_TRUE(extension_v1);
    EXPECT_TRUE(catcher.GetNextResult());
  }
  {
    // Update to version 2, also expect runtime.onInstalled.
    ResultCatcher catcher;
    const int expected_change = 0;
    const Extension* extension_v2 =
        UpdateExtension(extension_id, data[1].crx_path, expected_change);
    ASSERT_TRUE(extension_v2);
    EXPECT_TRUE(catcher.GetNextResult());
  }
}

// Tests that if updating an extension makes the extension disabled (due to
// permissions increase), then enabling the extension fires runtime.onInstalled
// correctly to the updated extension.
IN_PROC_BROWSER_TEST_F(EventsApiTest,
                       UpdateDispatchesOnInstalledAfterEnablement) {
  std::vector<ExtensionCRXData> data;
  data.emplace_back("v1");
  data.emplace_back("v2");
  SetUpCRX("lazy_events/on_installed_permissions_increase", "pem.pem", &data);

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  ExtensionId extension_id;
  {
    // Install version 1 of the extension and expect runtime.onInstalled.
    ResultCatcher catcher;
    const int expected_change = 1;
    const Extension* extension_v1 =
        InstallExtension(data[0].crx_path, expected_change);
    extension_id = extension_v1->id();
    ASSERT_TRUE(extension_v1);
    EXPECT_TRUE(catcher.GetNextResult());
  }
  {
    // Update to version 2, which will be disabled due to permissions increase.
    ResultCatcher catcher;
    const int expected_change = -1;  // Expect extension to be disabled.
    ASSERT_FALSE(
        UpdateExtension(extension_id, data[1].crx_path, expected_change));

    const Extension* extension_v2 =
        registry->disabled_extensions().GetByID(extension_id);
    ASSERT_TRUE(extension_v2);
    // Enable the extension.
    extension_service()->GrantPermissionsAndEnableExtension(extension_v2);
    EXPECT_TRUE(catcher.GetNextResult());
  }
}

// Tests that if an extension's updated version has a new lazy listener, it
// fires properly after the update.
IN_PROC_BROWSER_TEST_F(EventsApiTest, NewlyIntroducedListener) {
  std::vector<ExtensionCRXData> data;
  data.emplace_back("v1");
  data.emplace_back("v2");
  SetUpCRX("lazy_events/new_event_in_new_version", "pem.pem", &data);

  ExtensionId extension_id;
  {
    // Install version 1 of the extension.
    ResultCatcher catcher;
    const int expected_change = 1;
    const Extension* extension_v1 =
        InstallExtension(data[0].crx_path, expected_change);
    EXPECT_TRUE(extension_v1);
    extension_id = extension_v1->id();
    ASSERT_TRUE(extension_v1);
    EXPECT_TRUE(catcher.GetNextResult());
  }
  {
    // Update to version 2, that has tabs.onCreated event listener.
    ResultCatcher catcher;
    const int expected_change = 0;
    const Extension* extension_v2 =
        UpdateExtension(extension_id, data[1].crx_path, expected_change);
    ASSERT_TRUE(extension_v2);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(url::kAboutBlankURL),
        WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
    // Expect tabs.onCreated to fire.
    EXPECT_TRUE(catcher.GetNextResult());
  }
}

}  // namespace extensions
