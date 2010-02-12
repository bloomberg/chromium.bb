// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/browser_action_test_util.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/views/browser_actions_container.h"
#include "chrome/common/chrome_switches.h"

class BrowserActionsContainerTest : public ExtensionBrowserTest {
 public:
  BrowserActionsContainerTest() {
  }
  virtual ~BrowserActionsContainerTest() {}

  virtual Browser* CreateBrowser(Profile* profile) {
    Browser* b = InProcessBrowserTest::CreateBrowser(profile);
    browser_actions_bar_.reset(new BrowserActionTestUtil(b));
    return b;
  }

  BrowserActionTestUtil* browser_actions_bar() {
    return browser_actions_bar_.get();
  }

 private:
  scoped_ptr<BrowserActionTestUtil> browser_actions_bar_;
};

// Test the basic functionality.
IN_PROC_BROWSER_TEST_F(BrowserActionsContainerTest, Basic) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  BrowserActionsContainer::disable_animations_during_testing_ = true;

  // Load an extension with no browser action.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("none")));
  // This extension should not be in the model (has no browser action).
  EXPECT_EQ(0, browser_actions_bar()->NumberOfBrowserActions());

  // Load an extension with a browser action.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("basics")));
  EXPECT_EQ(1, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_TRUE(browser_actions_bar()->HasIcon(0));

  // Unload the extension.
  std::string id = browser_actions_bar()->GetExtensionId(0);
  UnloadExtension(id);
  EXPECT_EQ(0, browser_actions_bar()->NumberOfBrowserActions());
}

#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
// Linux with toolkits doesn't yet support an overflow for the browser actions
// container, so it doesn't make sense to test that feature yet.
// See http://crbug.com/35593.
#define MAYBE_TestVisibility DISABLED_TestVisibility
#else
#define MAYBE_TestVisibility TestVisibility
#endif

IN_PROC_BROWSER_TEST_F(BrowserActionsContainerTest, MAYBE_TestVisibility) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  BrowserActionsContainer::disable_animations_during_testing_ = true;

  // Load extension A (contains browser action).
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("basics")));
  EXPECT_EQ(1, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_TRUE(browser_actions_bar()->HasIcon(0));
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  std::string idA = browser_actions_bar()->GetExtensionId(0);

  // Load extension B (contains browser action).
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("add_popup")));
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_TRUE(browser_actions_bar()->HasIcon(1));
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  std::string idB = browser_actions_bar()->GetExtensionId(1);

  EXPECT_STRNE(idA.c_str(), idB.c_str());

  // Load extension C (contains browser action).
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("remove_popup")));
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_TRUE(browser_actions_bar()->HasIcon(2));
  EXPECT_EQ(3, browser_actions_bar()->VisibleBrowserActions());
  std::string idC = browser_actions_bar()->GetExtensionId(2);

  // Change container to show only one action, rest in overflow: A, [B, C].
  browser_actions_bar()->SetIconVisibilityCount(1);
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());

  // Disable extension A (should disappear). State becomes: B [C].
  DisableExtension(idA);
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_STREQ(idB.c_str(), browser_actions_bar()->GetExtensionId(0).c_str());

  // Enable A again. A should get its spot in the same location and the bar
  // should not grow (chevron is showing). For details: http://crbug.com/35349.
  // State becomes: A, [B, C].
  EnableExtension(idA);
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_STREQ(idA.c_str(), browser_actions_bar()->GetExtensionId(0).c_str());

  // Disable C (in overflow). State becomes: A, [B].
  DisableExtension(idC);
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_STREQ(idA.c_str(), browser_actions_bar()->GetExtensionId(0).c_str());

  // Enable C again. State becomes: A, [B, C].
  EnableExtension(idC);
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_STREQ(idA.c_str(), browser_actions_bar()->GetExtensionId(0).c_str());

  // Now we have 3 extensions. Make sure they are all visible. State: A, B, C.
  browser_actions_bar()->SetIconVisibilityCount(3);
  EXPECT_EQ(3, browser_actions_bar()->VisibleBrowserActions());

  // Disable extension A (should disappear). State becomes: B, C.
  DisableExtension(idA);
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_STREQ(idB.c_str(), browser_actions_bar()->GetExtensionId(0).c_str());

  // Disable extension B (should disappear). State becomes: C.
  DisableExtension(idB);
  EXPECT_EQ(1, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(1, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_STREQ(idC.c_str(), browser_actions_bar()->GetExtensionId(0).c_str());

  // Enable B (makes B and C showing now). State becomes: B, C.
  EnableExtension(idB);
  EXPECT_EQ(2, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(2, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_STREQ(idB.c_str(), browser_actions_bar()->GetExtensionId(0).c_str());

  // Enable A (makes A, B and C showing now). State becomes: B, C, A.
  EnableExtension(idA);
  EXPECT_EQ(3, browser_actions_bar()->NumberOfBrowserActions());
  EXPECT_EQ(3, browser_actions_bar()->VisibleBrowserActions());
  EXPECT_STREQ(idA.c_str(), browser_actions_bar()->GetExtensionId(2).c_str());
}
