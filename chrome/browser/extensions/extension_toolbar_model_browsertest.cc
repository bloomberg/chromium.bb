// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/in_process_browser_test.h"

// An InProcessBrowserTest for testing the ExtensionToolbarModel.
// TODO(erikkay) It's unfortunate that this needs to be an in-proc browser test.
// It would be nice to refactor things so that ExtensionService could run
// without so much of the browser in place.
class ExtensionToolbarModelTest : public ExtensionBrowserTest,
                                  public ExtensionToolbarModel::Observer {
 public:
  virtual void SetUp() {
    inserted_count_ = 0;
    removed_count_ = 0;
    moved_count_ = 0;

    ExtensionBrowserTest::SetUp();
  }

  virtual Browser* CreateBrowser(Profile* profile) {
    Browser* b = InProcessBrowserTest::CreateBrowser(profile);
    ExtensionService* service = b->profile()->GetExtensionService();
    model_ = service->toolbar_model();
    model_->AddObserver(this);
    return b;
  }

  virtual void CleanUpOnMainThread() {
    model_->RemoveObserver(this);
  }

  virtual void BrowserActionAdded(const Extension* extension, int index) {
    inserted_count_++;
  }

  virtual void BrowserActionRemoved(const Extension* extension) {
    removed_count_++;
  }

  virtual void BrowserActionMoved(const Extension* extension, int index) {
    moved_count_++;
  }

  const Extension* ExtensionAt(int index) {
    for (ExtensionList::iterator i = model_->begin(); i < model_->end(); ++i) {
      if (index-- == 0)
        return *i;
    }
    return NULL;
  }

 protected:
  ExtensionToolbarModel* model_;

  int inserted_count_;
  int removed_count_;
  int moved_count_;
};

IN_PROC_BROWSER_TEST_F(ExtensionToolbarModelTest, Basic) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  // Load an extension with no browser action.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("none")));

  // This extension should not be in the model (has no browser action).
  EXPECT_EQ(0, inserted_count_);
  EXPECT_EQ(0u, model_->size());
  ASSERT_EQ(NULL, ExtensionAt(0));

  // Load an extension with a browser action.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("basics")));

  // We should now find our extension in the model.
  EXPECT_EQ(1, inserted_count_);
  EXPECT_EQ(1u, model_->size());
  const Extension* extension = ExtensionAt(0);
  ASSERT_TRUE(NULL != extension);
  EXPECT_STREQ("A browser action with no icon that makes the page red",
               extension->name().c_str());

  // Should be a no-op, but still fires the events.
  model_->MoveBrowserAction(extension, 0);
  EXPECT_EQ(1, moved_count_);
  EXPECT_EQ(1u, model_->size());
  const Extension* extension2 = ExtensionAt(0);
  EXPECT_EQ(extension, extension2);

  UnloadExtension(extension->id());
  EXPECT_EQ(1, removed_count_);
  EXPECT_EQ(0u, model_->size());
  EXPECT_EQ(NULL, ExtensionAt(0));
}

IN_PROC_BROWSER_TEST_F(ExtensionToolbarModelTest, ReorderAndReinsert) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  // Load an extension with a browser action.
  FilePath extension_a_path(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("basics"));
  ASSERT_TRUE(LoadExtension(extension_a_path));

  // First extension loaded.
  EXPECT_EQ(1, inserted_count_);
  EXPECT_EQ(1u, model_->size());
  const Extension* extensionA = ExtensionAt(0);
  ASSERT_TRUE(NULL != extensionA);
  EXPECT_STREQ("A browser action with no icon that makes the page red",
               extensionA->name().c_str());

  // Load another extension with a browser action.
  FilePath extension_b_path(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("popup"));
  ASSERT_TRUE(LoadExtension(extension_b_path));

  // Second extension loaded.
  EXPECT_EQ(2, inserted_count_);
  EXPECT_EQ(2u, model_->size());
  const Extension* extensionB = ExtensionAt(1);
  ASSERT_TRUE(NULL != extensionB);
  EXPECT_STREQ("Popup tester", extensionB->name().c_str());

  // Load yet another extension with a browser action.
  FilePath extension_c_path(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("remove_popup"));
  ASSERT_TRUE(LoadExtension(extension_c_path));

  // Third extension loaded.
  EXPECT_EQ(3, inserted_count_);
  EXPECT_EQ(3u, model_->size());
  const Extension* extensionC = ExtensionAt(2);
  ASSERT_TRUE(NULL != extensionC);
  EXPECT_STREQ("A page action which removes a popup.",
               extensionC->name().c_str());

  // Order is now A, B, C. Let's put C first.
  model_->MoveBrowserAction(extensionC, 0);
  EXPECT_EQ(1, moved_count_);
  EXPECT_EQ(3u, model_->size());
  EXPECT_EQ(extensionC, ExtensionAt(0));
  EXPECT_EQ(extensionA, ExtensionAt(1));
  EXPECT_EQ(extensionB, ExtensionAt(2));
  EXPECT_EQ(NULL, ExtensionAt(3));

  // Order is now C, A, B. Let's put A last.
  model_->MoveBrowserAction(extensionA, 2);
  EXPECT_EQ(2, moved_count_);
  EXPECT_EQ(3u, model_->size());
  EXPECT_EQ(extensionC, ExtensionAt(0));
  EXPECT_EQ(extensionB, ExtensionAt(1));
  EXPECT_EQ(extensionA, ExtensionAt(2));
  EXPECT_EQ(NULL, ExtensionAt(3));

  // Order is now C, B, A. Let's remove B.
  std::string idB = extensionB->id();
  UnloadExtension(idB);
  EXPECT_EQ(1, removed_count_);
  EXPECT_EQ(2u, model_->size());
  EXPECT_EQ(extensionC, ExtensionAt(0));
  EXPECT_EQ(extensionA, ExtensionAt(1));
  EXPECT_EQ(NULL, ExtensionAt(2));

  // Load extension B again.
  ASSERT_TRUE(LoadExtension(extension_b_path));

  // Extension B loaded again.
  EXPECT_EQ(4, inserted_count_);
  EXPECT_EQ(3u, model_->size());
  // Make sure it gets its old spot in the list. We should get the same
  // extension again, otherwise the order has changed.
  ASSERT_STREQ(idB.c_str(), ExtensionAt(1)->id().c_str());

  // Unload B again.
  UnloadExtension(idB);
  EXPECT_EQ(2, removed_count_);
  EXPECT_EQ(2u, model_->size());
  EXPECT_EQ(extensionC, ExtensionAt(0));
  EXPECT_EQ(extensionA, ExtensionAt(1));
  EXPECT_EQ(NULL, ExtensionAt(2));

  // Order is now C, A. Flip it.
  model_->MoveBrowserAction(extensionA, 0);
  EXPECT_EQ(3, moved_count_);
  EXPECT_EQ(2u, model_->size());
  EXPECT_EQ(extensionA, ExtensionAt(0));
  EXPECT_EQ(extensionC, ExtensionAt(1));
  EXPECT_EQ(NULL, ExtensionAt(2));

  // Move A to the location it already occupies.
  model_->MoveBrowserAction(extensionA, 0);
  EXPECT_EQ(4, moved_count_);
  EXPECT_EQ(2u, model_->size());
  EXPECT_EQ(extensionA, ExtensionAt(0));
  EXPECT_EQ(extensionC, ExtensionAt(1));
  EXPECT_EQ(NULL, ExtensionAt(2));

  // Order is now A, C. Remove C.
  std::string idC = extensionC->id();
  UnloadExtension(idC);
  EXPECT_EQ(3, removed_count_);
  EXPECT_EQ(1u, model_->size());
  EXPECT_EQ(extensionA, ExtensionAt(0));
  EXPECT_EQ(NULL, ExtensionAt(1));

  // Load extension C again.
  ASSERT_TRUE(LoadExtension(extension_c_path));

  // Extension C loaded again.
  EXPECT_EQ(5, inserted_count_);
  EXPECT_EQ(2u, model_->size());
  // Make sure it gets its old spot in the list (at the very end).
  ASSERT_STREQ(idC.c_str(), ExtensionAt(1)->id().c_str());
}
