// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/guest_view/guest_view_manager.h"
#include "extensions/browser/guest_view/guest_view_manager_factory.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_paths.h"
#include "extensions/shell/browser/shell_extension_system.h"
#include "extensions/shell/test/shell_test.h"
#include "extensions/test/extension_test_message_listener.h"

namespace {

// TODO(lfg) Merge TestGuestViewManager and its factory with the one in chrome
// web_view_browsertest.cc.
class TestGuestViewManager : public extensions::GuestViewManager {
 public:
  explicit TestGuestViewManager(content::BrowserContext* context)
      : GuestViewManager(context),
        seen_guest_removed_(false),
        web_contents_(NULL) {}

  content::WebContents* WaitForGuestCreated() {
    if (web_contents_)
      return web_contents_;

    created_message_loop_runner_ = new content::MessageLoopRunner;
    created_message_loop_runner_->Run();
    return web_contents_;
  }

  void WaitForGuestDeleted() {
    if (seen_guest_removed_)
      return;

    deleted_message_loop_runner_ = new content::MessageLoopRunner;
    deleted_message_loop_runner_->Run();
  }

 private:
  // GuestViewManager override:
  virtual void AddGuest(int guest_instance_id,
                        content::WebContents* guest_web_contents) OVERRIDE {
    extensions::GuestViewManager::AddGuest(guest_instance_id,
                                           guest_web_contents);
    web_contents_ = guest_web_contents;
    seen_guest_removed_ = false;

    if (created_message_loop_runner_.get())
      created_message_loop_runner_->Quit();
  }

  virtual void RemoveGuest(int guest_instance_id) OVERRIDE {
    extensions::GuestViewManager::RemoveGuest(guest_instance_id);
    web_contents_ = NULL;
    seen_guest_removed_ = true;

    if (deleted_message_loop_runner_.get())
      deleted_message_loop_runner_->Quit();
  }

  bool seen_guest_removed_;
  content::WebContents* web_contents_;
  scoped_refptr<content::MessageLoopRunner> created_message_loop_runner_;
  scoped_refptr<content::MessageLoopRunner> deleted_message_loop_runner_;
};

// Test factory for creating test instances of GuestViewManager.
class TestGuestViewManagerFactory : public extensions::GuestViewManagerFactory {
 public:
  TestGuestViewManagerFactory() : test_guest_view_manager_(NULL) {}

  virtual ~TestGuestViewManagerFactory() {}

  virtual extensions::GuestViewManager* CreateGuestViewManager(
      content::BrowserContext* context) OVERRIDE {
    return GetManager(context);
  }

  // This function gets called from GuestViewManager::FromBrowserContext(),
  // where test_guest_view_manager_ is assigned to a linked_ptr that takes care
  // of deleting it.
  TestGuestViewManager* GetManager(content::BrowserContext* context) {
    DCHECK(!test_guest_view_manager_);
    test_guest_view_manager_ = new TestGuestViewManager(context);
    return test_guest_view_manager_;
  }

 private:
  TestGuestViewManager* test_guest_view_manager_;

  DISALLOW_COPY_AND_ASSIGN(TestGuestViewManagerFactory);
};

}  // namespace

namespace extensions {

// This class intercepts download request from the guest.
class WebViewAPITest : public AppShellTest {
 protected:
  void RunTest(const std::string& test_name, const std::string& app_location) {
    base::FilePath test_data_dir;
    PathService::Get(extensions::DIR_TEST_DATA, &test_data_dir);
    test_data_dir = test_data_dir.AppendASCII(app_location.c_str());

    ASSERT_TRUE(extension_system_->LoadApp(test_data_dir));
    extension_system_->LaunchApp();

    ExtensionTestMessageListener launch_listener("LAUNCHED", false);
    ASSERT_TRUE(launch_listener.WaitUntilSatisfied());

    const AppWindowRegistry::AppWindowList& app_window_list =
        AppWindowRegistry::Get(browser_context_)->app_windows();
    DCHECK(app_window_list.size() == 1);
    content::WebContents* embedder_web_contents =
        (*app_window_list.begin())->web_contents();

    ExtensionTestMessageListener done_listener("TEST_PASSED", false);
    done_listener.set_failure_message("TEST_FAILED");
    if (!content::ExecuteScript(
            embedder_web_contents,
            base::StringPrintf("runTest('%s')", test_name.c_str()))) {
      LOG(ERROR) << "Unable to start test.";
      return;
    }
    ASSERT_TRUE(done_listener.WaitUntilSatisfied());
  }

  WebViewAPITest() {
    extensions::GuestViewManager::set_factory_for_testing(&factory_);
  }

 private:
  TestGuestViewManagerFactory factory_;
};

IN_PROC_BROWSER_TEST_F(WebViewAPITest, TestAPIMethodExistence) {
  RunTest("testAPIMethodExistence", "web_view/apitest");
}

}  // namespace extensions
