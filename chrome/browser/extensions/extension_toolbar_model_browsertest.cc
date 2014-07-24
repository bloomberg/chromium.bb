// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace extensions {

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
    highlight_mode_count_ = 0;

    ExtensionBrowserTest::SetUp();
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    model_ = ExtensionToolbarModel::Get(browser()->profile());
    model_->AddObserver(this);
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    model_->RemoveObserver(this);
  }

  virtual void BrowserActionAdded(const Extension* extension,
                                  int index) OVERRIDE {
    inserted_count_++;
  }

  virtual void BrowserActionRemoved(const Extension* extension) OVERRIDE {
    removed_count_++;
  }

  virtual void BrowserActionMoved(const Extension* extension,
                                  int index) OVERRIDE {
    moved_count_++;
  }

  virtual void HighlightModeChanged(bool is_highlighting) OVERRIDE {
    // Add one if highlighting, subtract one if not.
    highlight_mode_count_ += is_highlighting ? 1 : -1;
  }

  const Extension* ExtensionAt(int index) {
    const ExtensionList& toolbar_items = model_->toolbar_items();
    for (ExtensionList::const_iterator i = toolbar_items.begin();
         i < toolbar_items.end(); ++i) {
      if (index-- == 0)
        return i->get();
    }
    return NULL;
  }

 protected:
  ExtensionToolbarModel* model_;

  int inserted_count_;
  int removed_count_;
  int moved_count_;
  int highlight_mode_count_;
};

IN_PROC_BROWSER_TEST_F(ExtensionToolbarModelTest, Basic) {
  // Load an extension with no browser action.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("none")));

  // This extension should not be in the model (has no browser action).
  EXPECT_EQ(0, inserted_count_);
  EXPECT_EQ(0u, model_->toolbar_items().size());
  ASSERT_EQ(NULL, ExtensionAt(0));

  // Load an extension with a browser action.
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("basics")));

  // We should now find our extension in the model.
  EXPECT_EQ(1, inserted_count_);
  EXPECT_EQ(1u, model_->toolbar_items().size());
  const Extension* extension = ExtensionAt(0);
  ASSERT_TRUE(NULL != extension);
  EXPECT_STREQ("A browser action with no icon that makes the page red",
               extension->name().c_str());

  // Should be a no-op, but still fires the events.
  model_->MoveBrowserAction(extension, 0);
  EXPECT_EQ(1, moved_count_);
  EXPECT_EQ(1u, model_->toolbar_items().size());
  const Extension* extension2 = ExtensionAt(0);
  EXPECT_EQ(extension, extension2);

  UnloadExtension(extension->id());
  EXPECT_EQ(1, removed_count_);
  EXPECT_EQ(0u, model_->toolbar_items().size());
  EXPECT_EQ(NULL, ExtensionAt(0));
}

#if defined(OS_MACOSX)
// Flaky on Mac 10.8 Blink canary bots: http://crbug.com/166580
#define MAYBE_ReorderAndReinsert DISABLED_ReorderAndReinsert
#else
#define MAYBE_ReorderAndReinsert ReorderAndReinsert
#endif
IN_PROC_BROWSER_TEST_F(ExtensionToolbarModelTest, MAYBE_ReorderAndReinsert) {
  // Load an extension with a browser action.
  base::FilePath extension_a_path(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("basics"));
  ASSERT_TRUE(LoadExtension(extension_a_path));

  // First extension loaded.
  EXPECT_EQ(1, inserted_count_);
  EXPECT_EQ(1u, model_->toolbar_items().size());
  const Extension* extensionA = ExtensionAt(0);
  ASSERT_TRUE(NULL != extensionA);
  EXPECT_STREQ("A browser action with no icon that makes the page red",
               extensionA->name().c_str());

  // Load another extension with a browser action.
  base::FilePath extension_b_path(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("popup"));
  ASSERT_TRUE(LoadExtension(extension_b_path));

  // Second extension loaded.
  EXPECT_EQ(2, inserted_count_);
  EXPECT_EQ(2u, model_->toolbar_items().size());
  const Extension* extensionB = ExtensionAt(1);
  ASSERT_TRUE(NULL != extensionB);
  EXPECT_STREQ("Popup tester", extensionB->name().c_str());

  // Load yet another extension with a browser action.
  base::FilePath extension_c_path(test_data_dir_.AppendASCII("api_test")
                                          .AppendASCII("browser_action")
                                          .AppendASCII("remove_popup"));
  ASSERT_TRUE(LoadExtension(extension_c_path));

  // Third extension loaded.
  EXPECT_EQ(3, inserted_count_);
  EXPECT_EQ(3u, model_->toolbar_items().size());
  const Extension* extensionC = ExtensionAt(2);
  ASSERT_TRUE(NULL != extensionC);
  EXPECT_STREQ("A page action which removes a popup.",
               extensionC->name().c_str());

  // Order is now A, B, C. Let's put C first.
  model_->MoveBrowserAction(extensionC, 0);
  EXPECT_EQ(1, moved_count_);
  EXPECT_EQ(3u, model_->toolbar_items().size());
  EXPECT_EQ(extensionC, ExtensionAt(0));
  EXPECT_EQ(extensionA, ExtensionAt(1));
  EXPECT_EQ(extensionB, ExtensionAt(2));
  EXPECT_EQ(NULL, ExtensionAt(3));

  // Order is now C, A, B. Let's put A last.
  model_->MoveBrowserAction(extensionA, 2);
  EXPECT_EQ(2, moved_count_);
  EXPECT_EQ(3u, model_->toolbar_items().size());
  EXPECT_EQ(extensionC, ExtensionAt(0));
  EXPECT_EQ(extensionB, ExtensionAt(1));
  EXPECT_EQ(extensionA, ExtensionAt(2));
  EXPECT_EQ(NULL, ExtensionAt(3));

  // Order is now C, B, A. Let's remove B.
  std::string idB = extensionB->id();
  UnloadExtension(idB);
  EXPECT_EQ(1, removed_count_);
  EXPECT_EQ(2u, model_->toolbar_items().size());
  EXPECT_EQ(extensionC, ExtensionAt(0));
  EXPECT_EQ(extensionA, ExtensionAt(1));
  EXPECT_EQ(NULL, ExtensionAt(2));

  // Load extension B again.
  ASSERT_TRUE(LoadExtension(extension_b_path));

  // Extension B loaded again.
  EXPECT_EQ(4, inserted_count_);
  EXPECT_EQ(3u, model_->toolbar_items().size());
  // Make sure it gets its old spot in the list. We should get the same
  // extension again, otherwise the order has changed.
  ASSERT_STREQ(idB.c_str(), ExtensionAt(1)->id().c_str());

  // Unload B again.
  UnloadExtension(idB);
  EXPECT_EQ(2, removed_count_);
  EXPECT_EQ(2u, model_->toolbar_items().size());
  EXPECT_EQ(extensionC, ExtensionAt(0));
  EXPECT_EQ(extensionA, ExtensionAt(1));
  EXPECT_EQ(NULL, ExtensionAt(2));

  // Order is now C, A. Flip it.
  model_->MoveBrowserAction(extensionA, 0);
  EXPECT_EQ(3, moved_count_);
  EXPECT_EQ(2u, model_->toolbar_items().size());
  EXPECT_EQ(extensionA, ExtensionAt(0));
  EXPECT_EQ(extensionC, ExtensionAt(1));
  EXPECT_EQ(NULL, ExtensionAt(2));

  // Move A to the location it already occupies.
  model_->MoveBrowserAction(extensionA, 0);
  EXPECT_EQ(4, moved_count_);
  EXPECT_EQ(2u, model_->toolbar_items().size());
  EXPECT_EQ(extensionA, ExtensionAt(0));
  EXPECT_EQ(extensionC, ExtensionAt(1));
  EXPECT_EQ(NULL, ExtensionAt(2));

  // Order is now A, C. Remove C.
  std::string idC = extensionC->id();
  UnloadExtension(idC);
  EXPECT_EQ(3, removed_count_);
  EXPECT_EQ(1u, model_->toolbar_items().size());
  EXPECT_EQ(extensionA, ExtensionAt(0));
  EXPECT_EQ(NULL, ExtensionAt(1));

  // Load extension C again.
  ASSERT_TRUE(LoadExtension(extension_c_path));

  // Extension C loaded again.
  EXPECT_EQ(5, inserted_count_);
  EXPECT_EQ(2u, model_->toolbar_items().size());
  // Make sure it gets its old spot in the list (at the very end).
  ASSERT_STREQ(idC.c_str(), ExtensionAt(1)->id().c_str());
}

IN_PROC_BROWSER_TEST_F(ExtensionToolbarModelTest, UnloadAndDisableMultiple) {
  // Load three extensions with browser action.
  base::FilePath extension_a_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("basics"));
  ASSERT_TRUE(LoadExtension(extension_a_path));
  base::FilePath extension_b_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("popup"));
  ASSERT_TRUE(LoadExtension(extension_b_path));
  base::FilePath extension_c_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("remove_popup"));
  ASSERT_TRUE(LoadExtension(extension_c_path));

  // Verify we got the three we asked for and that they are ordered as: A, B, C.
  const Extension* extensionA = ExtensionAt(0);
  const Extension* extensionB = ExtensionAt(1);
  const Extension* extensionC = ExtensionAt(2);
  std::string idA = extensionA->id();
  std::string idB = extensionB->id();
  std::string idC = extensionC->id();
  EXPECT_STREQ("A browser action with no icon that makes the page red",
               extensionA->name().c_str());
  EXPECT_STREQ("Popup tester", extensionB->name().c_str());
  EXPECT_STREQ("A page action which removes a popup.",
               extensionC->name().c_str());

  // Unload B, then C, then A.
  UnloadExtension(idB);
  UnloadExtension(idC);
  UnloadExtension(idA);

  // Load C, then A, then B.
  ASSERT_TRUE(LoadExtension(extension_c_path));
  ASSERT_TRUE(LoadExtension(extension_a_path));
  ASSERT_TRUE(LoadExtension(extension_b_path));
  EXPECT_EQ(0, moved_count_);

  extensionA = ExtensionAt(0);
  extensionB = ExtensionAt(1);
  extensionC = ExtensionAt(2);

  // Make sure we get the order we started with (A, B, C).
  EXPECT_STREQ(idA.c_str(), extensionA->id().c_str());
  EXPECT_STREQ(idB.c_str(), extensionB->id().c_str());
  EXPECT_STREQ(idC.c_str(), extensionC->id().c_str());

  // Put C in the middle and A to the end.
  model_->MoveBrowserAction(extensionC, 1);
  model_->MoveBrowserAction(extensionA, 2);

  // Make sure we get this order (C, B, A).
  EXPECT_STREQ(idC.c_str(), ExtensionAt(0)->id().c_str());
  EXPECT_STREQ(idB.c_str(), ExtensionAt(1)->id().c_str());
  EXPECT_STREQ(idA.c_str(), ExtensionAt(2)->id().c_str());

  // Disable B, then C, then A.
  DisableExtension(idB);
  DisableExtension(idC);
  DisableExtension(idA);

  // Enable C, then A, then B.
  EnableExtension(idA);
  EnableExtension(idB);
  EnableExtension(idC);

  // Make sure we get the order we started with.
  EXPECT_STREQ(idC.c_str(), ExtensionAt(0)->id().c_str());
  EXPECT_STREQ(idB.c_str(), ExtensionAt(1)->id().c_str());
  EXPECT_STREQ(idA.c_str(), ExtensionAt(2)->id().c_str());
}

IN_PROC_BROWSER_TEST_F(ExtensionToolbarModelTest, Uninstall) {
  // Load two extensions with browser action.
  base::FilePath extension_a_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("basics"));
  ASSERT_TRUE(LoadExtension(extension_a_path));
  base::FilePath extension_b_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("popup"));
  ASSERT_TRUE(LoadExtension(extension_b_path));

  // Verify we got what we came for.
  const Extension* extensionA = ExtensionAt(0);
  const Extension* extensionB = ExtensionAt(1);
  std::string idA = extensionA->id();
  std::string idB = extensionB->id();
  EXPECT_STREQ("A browser action with no icon that makes the page red",
               extensionA->name().c_str());
  EXPECT_STREQ("Popup tester", extensionB->name().c_str());

  // Order is now A, B. Make B first.
  model_->MoveBrowserAction(extensionB, 0);

  // Order is now B, A. Uninstall Extension B.
  UninstallExtension(idB);

  // List contains only A now. Validate that.
  EXPECT_STREQ(idA.c_str(), ExtensionAt(0)->id().c_str());
  EXPECT_EQ(1u, model_->toolbar_items().size());

  // Load Extension B again.
  ASSERT_TRUE(LoadExtension(extension_b_path));
  EXPECT_EQ(2u, model_->toolbar_items().size());

  // Make sure Extension B is _not_ first (should have been forgotten at
  // uninstall time).
  EXPECT_STREQ(idA.c_str(), ExtensionAt(0)->id().c_str());
  EXPECT_STREQ(idB.c_str(), ExtensionAt(1)->id().c_str());
}

IN_PROC_BROWSER_TEST_F(ExtensionToolbarModelTest, ReorderOnPrefChange) {
  // Load three extensions with browser action.
  base::FilePath extension_a_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("basics"));
  ASSERT_TRUE(LoadExtension(extension_a_path));
  base::FilePath extension_b_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("popup"));
  ASSERT_TRUE(LoadExtension(extension_b_path));
  base::FilePath extension_c_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("remove_popup"));
  ASSERT_TRUE(LoadExtension(extension_c_path));
  std::string id_a = ExtensionAt(0)->id();
  std::string id_b = ExtensionAt(1)->id();
  std::string id_c = ExtensionAt(2)->id();

  // Change value of toolbar preference.
  ExtensionIdList new_order;
  new_order.push_back(id_c);
  new_order.push_back(id_b);
  ExtensionPrefs::Get(browser()->profile())->SetToolbarOrder(new_order);

  // Verify order is changed.
  EXPECT_EQ(id_c, ExtensionAt(0)->id());
  EXPECT_EQ(id_b, ExtensionAt(1)->id());
  EXPECT_EQ(id_a, ExtensionAt(2)->id());
}

IN_PROC_BROWSER_TEST_F(ExtensionToolbarModelTest, HighlightMode) {
  EXPECT_FALSE(model_->HighlightExtensions(ExtensionIdList()));
  EXPECT_EQ(0, highlight_mode_count_);

  // Load three extensions with browser action.
  base::FilePath extension_a_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("basics"));
  ASSERT_TRUE(LoadExtension(extension_a_path));
  base::FilePath extension_b_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("popup"));
  ASSERT_TRUE(LoadExtension(extension_b_path));
  base::FilePath extension_c_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("remove_popup"));
  ASSERT_TRUE(LoadExtension(extension_c_path));
  std::string id_a = ExtensionAt(0)->id();
  std::string id_b = ExtensionAt(1)->id();
  std::string id_c = ExtensionAt(2)->id();

  // Highlight one extension.
  ExtensionIdList extension_ids;
  extension_ids.push_back(id_b);
  model_->HighlightExtensions(extension_ids);
  EXPECT_EQ(1, highlight_mode_count_);
  EXPECT_TRUE(model_->is_highlighting());

  EXPECT_EQ(1u, model_->toolbar_items().size());
  EXPECT_EQ(id_b, ExtensionAt(0)->id());

  // Stop highlighting.
  model_->StopHighlighting();
  EXPECT_EQ(0, highlight_mode_count_);
  EXPECT_FALSE(model_->is_highlighting());

  // Verify that the extensions are back to normal.
  EXPECT_EQ(3u, model_->toolbar_items().size());
  EXPECT_EQ(id_a, ExtensionAt(0)->id());
  EXPECT_EQ(id_b, ExtensionAt(1)->id());
  EXPECT_EQ(id_c, ExtensionAt(2)->id());

  // Call stop highlighting a second time (shouldn't be notified).
  model_->StopHighlighting();
  EXPECT_EQ(0, highlight_mode_count_);
  EXPECT_FALSE(model_->is_highlighting());

  // Highlight all extensions.
  extension_ids.clear();
  extension_ids.push_back(id_a);
  extension_ids.push_back(id_b);
  extension_ids.push_back(id_c);
  model_->HighlightExtensions(extension_ids);
  EXPECT_EQ(1, highlight_mode_count_);
  EXPECT_EQ(3u, model_->toolbar_items().size());
  EXPECT_EQ(id_a, ExtensionAt(0)->id());
  EXPECT_EQ(id_b, ExtensionAt(1)->id());
  EXPECT_EQ(id_c, ExtensionAt(2)->id());

  // Highlight only extension b (shrink the highlight list).
  extension_ids.clear();
  extension_ids.push_back(id_b);
  model_->HighlightExtensions(extension_ids);
  EXPECT_EQ(2, highlight_mode_count_);
  EXPECT_EQ(1u, model_->toolbar_items().size());
  EXPECT_EQ(id_b, ExtensionAt(0)->id());

  // Highlight extensions a and b (grow the highlight list).
  extension_ids.clear();
  extension_ids.push_back(id_a);
  extension_ids.push_back(id_b);
  model_->HighlightExtensions(extension_ids);
  EXPECT_EQ(3, highlight_mode_count_);
  EXPECT_EQ(2u, model_->toolbar_items().size());
  EXPECT_EQ(id_a, ExtensionAt(0)->id());
  EXPECT_EQ(id_b, ExtensionAt(1)->id());

  // Highlight no extensions (empty the highlight list).
  extension_ids.clear();
  model_->HighlightExtensions(extension_ids);
  EXPECT_EQ(2, highlight_mode_count_);
  EXPECT_FALSE(model_->is_highlighting());
  EXPECT_EQ(id_a, ExtensionAt(0)->id());
  EXPECT_EQ(id_b, ExtensionAt(1)->id());
  EXPECT_EQ(id_c, ExtensionAt(2)->id());
}

#if defined(OS_MACOSX)
// Flaky on Mac bots: http://crbug.com/358752
#define MAYBE_HighlightModeRemove DISABLED_HighlightModeRemove
#else
#define MAYBE_HighlightModeRemove HighlightModeRemove
#endif

IN_PROC_BROWSER_TEST_F(ExtensionToolbarModelTest, MAYBE_HighlightModeRemove) {
  // Load three extensions with browser action.
  base::FilePath extension_a_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("basics"));
  ASSERT_TRUE(LoadExtension(extension_a_path));
  base::FilePath extension_b_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("popup"));
  ASSERT_TRUE(LoadExtension(extension_b_path));
  base::FilePath extension_c_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("remove_popup"));
  ASSERT_TRUE(LoadExtension(extension_c_path));
  std::string id_a = ExtensionAt(0)->id();
  std::string id_b = ExtensionAt(1)->id();
  std::string id_c = ExtensionAt(2)->id();

  // Highlight two of the extensions.
  ExtensionIdList extension_ids;
  extension_ids.push_back(id_a);
  extension_ids.push_back(id_b);
  model_->HighlightExtensions(extension_ids);
  EXPECT_TRUE(model_->is_highlighting());
  EXPECT_EQ(1, highlight_mode_count_);
  EXPECT_EQ(2u, model_->toolbar_items().size());

  // Disable one of them - only one should remain highlighted.
  DisableExtension(id_a);
  EXPECT_TRUE(model_->is_highlighting());
  EXPECT_EQ(1u, model_->toolbar_items().size());
  EXPECT_EQ(id_b, ExtensionAt(0)->id());

  // Uninstall the remaining highlighted extension. This should result in
  // highlight mode exiting.
  UninstallExtension(id_b);
  EXPECT_FALSE(model_->is_highlighting());
  EXPECT_EQ(0, highlight_mode_count_);
  EXPECT_EQ(1u, model_->toolbar_items().size());
  EXPECT_EQ(id_c, ExtensionAt(0)->id());

  // Test that removing an unhighlighted extension still works.
  // Reinstall extension b, and then highlight extension c.
  ASSERT_TRUE(LoadExtension(extension_b_path));
  EXPECT_EQ(id_b, ExtensionAt(1)->id());
  extension_ids.clear();
  extension_ids.push_back(id_c);
  model_->HighlightExtensions(extension_ids);
  EXPECT_EQ(1, highlight_mode_count_);
  EXPECT_TRUE(model_->is_highlighting());
  EXPECT_EQ(1u, model_->toolbar_items().size());
  EXPECT_EQ(id_c, ExtensionAt(0)->id());

  // Uninstalling b should not have visible impact.
  UninstallExtension(id_b);
  EXPECT_TRUE(model_->is_highlighting());
  EXPECT_EQ(1, highlight_mode_count_);
  EXPECT_EQ(1u, model_->toolbar_items().size());
  EXPECT_EQ(id_c, ExtensionAt(0)->id());

  // When we stop, only extension c should remain.
  model_->StopHighlighting();
  EXPECT_FALSE(model_->is_highlighting());
  EXPECT_EQ(0, highlight_mode_count_);
  EXPECT_EQ(1u, model_->toolbar_items().size());
  EXPECT_EQ(id_c, ExtensionAt(0)->id());
}

IN_PROC_BROWSER_TEST_F(ExtensionToolbarModelTest, HighlightModeAdd) {
  // Load two extensions with browser action.
  base::FilePath extension_a_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("basics"));
  ASSERT_TRUE(LoadExtension(extension_a_path));
  base::FilePath extension_b_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("popup"));
  ASSERT_TRUE(LoadExtension(extension_b_path));
  std::string id_a = ExtensionAt(0)->id();
  std::string id_b = ExtensionAt(1)->id();

  // Highlight one of the extensions.
  ExtensionIdList extension_ids;
  extension_ids.push_back(id_a);
  model_->HighlightExtensions(extension_ids);
  EXPECT_TRUE(model_->is_highlighting());
  EXPECT_EQ(1u, model_->toolbar_items().size());
  EXPECT_EQ(id_a, ExtensionAt(0)->id());

  // Adding the new extension should have no visible effect.
  base::FilePath extension_c_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("remove_popup"));
  const Extension* extension_c = LoadExtension(extension_c_path);
  ASSERT_TRUE(extension_c);
  std::string id_c = extension_c->id();
  EXPECT_TRUE(model_->is_highlighting());
  EXPECT_EQ(1u, model_->toolbar_items().size());
  EXPECT_EQ(id_a, ExtensionAt(0)->id());

  // When we stop highlighting, we should see the new extension show up.
  model_->StopHighlighting();
  EXPECT_FALSE(model_->is_highlighting());
  EXPECT_EQ(3u, model_->toolbar_items().size());
  EXPECT_EQ(id_a, ExtensionAt(0)->id());
  EXPECT_EQ(id_b, ExtensionAt(1)->id());
  EXPECT_EQ(id_c, ExtensionAt(2)->id());
}

// Test is flaky (see crbug.com/379170), but currently enabled to gather traces.
// If it fails, ping Finnur.
IN_PROC_BROWSER_TEST_F(ExtensionToolbarModelTest, SizeAfterPrefChange) {
  // Load two extensions with browser action.
  base::FilePath extension_a_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("basics"));
  ASSERT_TRUE(LoadExtension(extension_a_path));
  base::FilePath extension_b_path(test_data_dir_.AppendASCII("api_test")
                                                .AppendASCII("browser_action")
                                                .AppendASCII("popup"));
  ASSERT_TRUE(LoadExtension(extension_b_path));
  std::string id_a = ExtensionAt(0)->id();
  std::string id_b = ExtensionAt(1)->id();

  // Should be at max size (-1).
  EXPECT_EQ(-1, model_->GetVisibleIconCount());

  model_->OnExtensionToolbarPrefChange();

  // Should still be at max size.
  EXPECT_EQ(-1, model_->GetVisibleIconCount());
}

}  // namespace extensions
