// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"

namespace extensions {

namespace {

// Create an extension. If |action_key| is non-NULL, it should point to either
// kBrowserAction or kPageAction, and the extension will have the associated
// action.
scoped_refptr<const Extension> GetActionExtension(const std::string& name,
                                                  const char* action_key) {
  DictionaryBuilder manifest;
  manifest.Set("name", name)
          .Set("description", "An extension")
          .Set("manifest_version", 2)
          .Set("version", "1.0.0");
  if (action_key)
    manifest.Set(action_key, DictionaryBuilder().Pass());

  return ExtensionBuilder().SetManifest(manifest.Pass())
                           .SetID(crx_file::id_util::GenerateId(name))
                           .Build();
}

// A simple observer that tracks the number of times certain events occur.
class ExtensionToolbarModelTestObserver
    : public ExtensionToolbarModel::Observer {
 public:
  explicit ExtensionToolbarModelTestObserver(ExtensionToolbarModel* model);
  virtual ~ExtensionToolbarModelTestObserver();

  size_t inserted_count() const { return inserted_count_; }
  size_t removed_count() const { return removed_count_; }
  size_t moved_count() const { return moved_count_; }
  int highlight_mode_count() const { return highlight_mode_count_; }

 private:
  // ExtensionToolbarModel::Observer:
  virtual void ToolbarExtensionAdded(const Extension* extension,
                                     int index) OVERRIDE {
    ++inserted_count_;
  }

  virtual void ToolbarExtensionRemoved(const Extension* extension) OVERRIDE {
    ++removed_count_;
  }

  virtual void ToolbarExtensionMoved(const Extension* extension,
                                     int index) OVERRIDE {
    ++moved_count_;
  }

  virtual void ToolbarExtensionUpdated(const Extension* extension) OVERRIDE {
  }

  virtual bool ShowExtensionActionPopup(const Extension* extension) OVERRIDE {
    return false;
  }

  virtual void ToolbarVisibleCountChanged() OVERRIDE {
  }

  virtual void ToolbarHighlightModeChanged(bool is_highlighting) OVERRIDE {
    // Add one if highlighting, subtract one if not.
    highlight_mode_count_ += is_highlighting ? 1 : -1;
  }

  ExtensionToolbarModel* model_;

  size_t inserted_count_;
  size_t removed_count_;
  size_t moved_count_;
  // Int because it could become negative (if something goes wrong).
  int highlight_mode_count_;
};

ExtensionToolbarModelTestObserver::ExtensionToolbarModelTestObserver(
    ExtensionToolbarModel* model) : model_(model),
                                    inserted_count_(0u),
                                    removed_count_(0u),
                                    moved_count_(0u),
                                    highlight_mode_count_(0) {
  model_->AddObserver(this);
}

ExtensionToolbarModelTestObserver::~ExtensionToolbarModelTestObserver() {
  model_->RemoveObserver(this);
}

}  // namespace

class ExtensionToolbarModelUnitTest : public ExtensionServiceTestBase {
 protected:
  // Initialize the ExtensionService, ExtensionToolbarModel, and
  // ExtensionSystem.
  void Init();

  // Adds or removes the given |extension| and verify success.
  testing::AssertionResult AddExtension(
      scoped_refptr<const Extension> extension) WARN_UNUSED_RESULT;
  testing::AssertionResult RemoveExtension(
      scoped_refptr<const Extension> extension) WARN_UNUSED_RESULT;

  // Adds three extensions, all with browser actions.
  testing::AssertionResult AddBrowserActionExtensions() WARN_UNUSED_RESULT;

  // Adds three extensions, one each for browser action, page action, and no
  // action, and are added in that order.
  testing::AssertionResult AddActionExtensions() WARN_UNUSED_RESULT;

  // Returns the extension at the given index in the toolbar model, or NULL
  // if one does not exist.
  const Extension* GetExtensionAtIndex(size_t index) const;

  ExtensionToolbarModel* toolbar_model() { return toolbar_model_.get(); }

  const ExtensionToolbarModelTestObserver* observer() const {
    return model_observer_.get();
  }
  size_t num_toolbar_items() const {
    return toolbar_model_->toolbar_items().size();
  }
  const Extension* browser_action_a() const {
    return browser_action_a_;
  }
  const Extension* browser_action_b() const {
    return browser_action_b_;
  }
  const Extension* browser_action_c() const {
    return browser_action_c_;
  }
  const Extension* browser_action() const {
    return browser_action_extension_.get();
  }
  const Extension* page_action() const { return page_action_extension_.get(); }
  const Extension* no_action() const { return no_action_extension_.get(); }

 private:
  // Verifies that all extensions in |extensions| are added successfully.
  testing::AssertionResult AddAndVerifyExtensions(
      const ExtensionList& extensions);

  // The associated (and owned) toolbar model.
  scoped_ptr<ExtensionToolbarModel> toolbar_model_;

  // The test observer to track events. Must come after toolbar_model_ so that
  // it is destroyed and removes itself as an observer first.
  scoped_ptr<ExtensionToolbarModelTestObserver> model_observer_;

  // Sample extensions with only browser actions.
  scoped_refptr<const Extension> browser_action_a_;
  scoped_refptr<const Extension> browser_action_b_;
  scoped_refptr<const Extension> browser_action_c_;

  // Sample extensions with different kinds of actions.
  scoped_refptr<const Extension> browser_action_extension_;
  scoped_refptr<const Extension> page_action_extension_;
  scoped_refptr<const Extension> no_action_extension_;
};

void ExtensionToolbarModelUnitTest::Init() {
  InitializeEmptyExtensionService();
  toolbar_model_.reset(
      new ExtensionToolbarModel(profile(), ExtensionPrefs::Get(profile())));
  model_observer_.reset(
      new ExtensionToolbarModelTestObserver(toolbar_model_.get()));
  static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()))->
      SetReady();
  // Run tasks posted to TestExtensionSystem.
  base::RunLoop().RunUntilIdle();
}

testing::AssertionResult ExtensionToolbarModelUnitTest::AddExtension(
    scoped_refptr<const Extension> extension) {
  if (registry()->enabled_extensions().GetByID(extension->id())) {
    return testing::AssertionFailure() << "Extension " << extension->name() <<
        " already installed!";
  }
  service()->AddExtension(extension);
  if (!registry()->enabled_extensions().GetByID(extension->id())) {
    return testing::AssertionFailure() << "Failed to install extension: " <<
        extension->name();
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult ExtensionToolbarModelUnitTest::RemoveExtension(
    scoped_refptr<const Extension> extension) {
  if (!registry()->enabled_extensions().GetByID(extension->id())) {
    return testing::AssertionFailure() << "Extension " << extension->name() <<
        " not installed!";
  }
  service()->UnloadExtension(extension->id(),
                             UnloadedExtensionInfo::REASON_DISABLE);
  if (registry()->enabled_extensions().GetByID(extension->id())) {
    return testing::AssertionFailure() << "Failed to unload extension: " <<
        extension->name();
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult ExtensionToolbarModelUnitTest::AddActionExtensions() {
  browser_action_extension_ =
      GetActionExtension("browser_action", manifest_keys::kBrowserAction);
  page_action_extension_ =
       GetActionExtension("page_action", manifest_keys::kPageAction);
  no_action_extension_ = GetActionExtension("no_action", NULL);

  ExtensionList extensions;
  extensions.push_back(browser_action_extension_);
  extensions.push_back(page_action_extension_);
  extensions.push_back(no_action_extension_);

  return AddAndVerifyExtensions(extensions);
}

testing::AssertionResult
ExtensionToolbarModelUnitTest::AddBrowserActionExtensions() {
  browser_action_a_ =
      GetActionExtension("browser_actionA", manifest_keys::kBrowserAction);
  browser_action_b_ =
      GetActionExtension("browser_actionB", manifest_keys::kBrowserAction);
  browser_action_c_ =
      GetActionExtension("browser_actionC", manifest_keys::kBrowserAction);

  ExtensionList extensions;
  extensions.push_back(browser_action_a_);
  extensions.push_back(browser_action_b_);
  extensions.push_back(browser_action_c_);

  return AddAndVerifyExtensions(extensions);
}

const Extension* ExtensionToolbarModelUnitTest::GetExtensionAtIndex(
    size_t index) const {
  return index < toolbar_model_->toolbar_items().size() ?
      toolbar_model_->toolbar_items()[index] : NULL;
}

testing::AssertionResult ExtensionToolbarModelUnitTest::AddAndVerifyExtensions(
    const ExtensionList& extensions) {
  for (ExtensionList::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    if (!AddExtension(*iter)) {
      return testing::AssertionFailure() << "Failed to install extension: " <<
          (*iter)->name();
    }
  }
  return testing::AssertionSuccess();
}

// A basic test for extensions with browser actions showing up in the toolbar.
TEST_F(ExtensionToolbarModelUnitTest, BasicExtensionToolbarModelTest) {
  Init();

  // Load an extension with no browser action.
  scoped_refptr<const Extension> extension1 =
      GetActionExtension("no_action", NULL);
  ASSERT_TRUE(AddExtension(extension1));

  // This extension should not be in the model (has no browser action).
  EXPECT_EQ(0u, observer()->inserted_count());
  EXPECT_EQ(0u, num_toolbar_items());
  EXPECT_EQ(NULL, GetExtensionAtIndex(0u));

  // Load an extension with a browser action.
  scoped_refptr<const Extension> extension2 =
      GetActionExtension("browser_action", manifest_keys::kBrowserAction);
  ASSERT_TRUE(AddExtension(extension2));

  // We should now find our extension in the model.
  EXPECT_EQ(1u, observer()->inserted_count());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(extension2, GetExtensionAtIndex(0u));

  // Should be a no-op, but still fires the events.
  toolbar_model()->MoveExtensionIcon(extension2, 0);
  EXPECT_EQ(1u, observer()->moved_count());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(extension2, GetExtensionAtIndex(0u));

  // Remove the extension and verify.
  ASSERT_TRUE(RemoveExtension(extension2));
  EXPECT_EQ(1u, observer()->removed_count());
  EXPECT_EQ(0u, num_toolbar_items());
  EXPECT_EQ(NULL, GetExtensionAtIndex(0u));
}

// Test various different reorderings, removals, and reinsertions.
TEST_F(ExtensionToolbarModelUnitTest, ExtensionToolbarReorderAndReinsert) {
  Init();

  // Add the three browser action extensions.
  ASSERT_TRUE(AddBrowserActionExtensions());

  // Verify the three extensions are in the model in the proper order.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(1u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(2u));

  // Order is now A, B, C. Let's put C first.
  toolbar_model()->MoveExtensionIcon(browser_action_c(), 0);
  EXPECT_EQ(1u, observer()->moved_count());
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(1u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(2u));

  // Order is now C, A, B. Let's put A last.
  toolbar_model()->MoveExtensionIcon(browser_action_a(), 2);
  EXPECT_EQ(2u, observer()->moved_count());
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(1u));
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(2u));

  // Order is now C, B, A. Let's remove B.
  ASSERT_TRUE(RemoveExtension(browser_action_b()));
  EXPECT_EQ(1u, observer()->removed_count());
  EXPECT_EQ(2u, num_toolbar_items());
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(1u));

  // Load extension B again.
  ASSERT_TRUE(AddExtension(browser_action_b()));

  // Extension B loaded again.
  EXPECT_EQ(4u, observer()->inserted_count());
  EXPECT_EQ(3u, num_toolbar_items());
  // Make sure it gets its old spot in the list.
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(1u));

  // Unload B again.
  ASSERT_TRUE(RemoveExtension(browser_action_b()));
  EXPECT_EQ(2u, observer()->removed_count());
  EXPECT_EQ(2u, num_toolbar_items());

  // Order is now C, A. Flip it.
  toolbar_model()->MoveExtensionIcon(browser_action_a(), 0);
  EXPECT_EQ(3u, observer()->moved_count());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(1u));

  // Move A to the location it already occupies.
  toolbar_model()->MoveExtensionIcon(browser_action_a(), 0);
  EXPECT_EQ(4u, observer()->moved_count());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(1u));

  // Order is now A, C. Remove C.
  ASSERT_TRUE(RemoveExtension(browser_action_c()));
  EXPECT_EQ(3u, observer()->removed_count());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));

  // Load extension C again.
  ASSERT_TRUE(AddExtension(browser_action_c()));

  // Extension C loaded again.
  EXPECT_EQ(5u, observer()->inserted_count());
  EXPECT_EQ(2u, num_toolbar_items());
  // Make sure it gets its old spot in the list (at the very end).
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(1u));
}

// Test that order persists after unloading and disabling, but not across
// uninstallation.
TEST_F(ExtensionToolbarModelUnitTest,
       ExtensionToolbarUnloadDisableAndUninstall) {
  Init();

  // Add the three browser action extensions.
  ASSERT_TRUE(AddBrowserActionExtensions());

  // Verify the three extensions are in the model in the proper order: A, B, C.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(1u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(2u));

  // Unload B, then C, then A, and then reload C, then A, then B.
  ASSERT_TRUE(RemoveExtension(browser_action_b()));
  ASSERT_TRUE(RemoveExtension(browser_action_c()));
  ASSERT_TRUE(RemoveExtension(browser_action_a()));
  EXPECT_EQ(0u, num_toolbar_items());  // Sanity check: all gone?
  ASSERT_TRUE(AddExtension(browser_action_c()));
  ASSERT_TRUE(AddExtension(browser_action_a()));
  ASSERT_TRUE(AddExtension(browser_action_b()));
  EXPECT_EQ(3u, num_toolbar_items());  // Sanity check: all back?
  EXPECT_EQ(0u, observer()->moved_count());

  // Even though we unloaded and reloaded in a different order, the original
  // order (A, B, C) should be preserved.
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(1u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(2u));

  // Disabling extensions should also preserve order.
  service()->DisableExtension(browser_action_b()->id(),
                              Extension::DISABLE_USER_ACTION);
  service()->DisableExtension(browser_action_c()->id(),
                              Extension::DISABLE_USER_ACTION);
  service()->DisableExtension(browser_action_a()->id(),
                              Extension::DISABLE_USER_ACTION);
  service()->EnableExtension(browser_action_c()->id());
  service()->EnableExtension(browser_action_a()->id());
  service()->EnableExtension(browser_action_b()->id());

  // Make sure we still get the original A, B, C order.
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(1u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(2u));

  // Move browser_action_b() to be first.
  toolbar_model()->MoveExtensionIcon(browser_action_b(), 0);
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(0u));

  // Uninstall Extension B.
  service()->UninstallExtension(browser_action_b()->id(),
                                UNINSTALL_REASON_FOR_TESTING,
                                base::Bind(&base::DoNothing),
                                NULL);  // Ignore error.
  // List contains only A and C now. Validate that.
  EXPECT_EQ(2u, num_toolbar_items());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(1u));

  ASSERT_TRUE(AddExtension(browser_action_b()));

  // Make sure Extension B is _not_ first (its old position should have been
  // forgotten at uninstall time). Order should be A, C, B.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(1u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(2u));
}

TEST_F(ExtensionToolbarModelUnitTest, ReorderOnPrefChange) {
  Init();

  // Add the three browser action extensions.
  ASSERT_TRUE(AddBrowserActionExtensions());
  EXPECT_EQ(3u, num_toolbar_items());

  // Change the value of the toolbar preference.
  ExtensionIdList new_order;
  new_order.push_back(browser_action_c()->id());
  new_order.push_back(browser_action_b()->id());
  ExtensionPrefs::Get(profile())->SetToolbarOrder(new_order);

  // Verify order is changed.
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(1u));
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(2u));
}

TEST_F(ExtensionToolbarModelUnitTest, ExtensionToolbarHighlightMode) {
  Init();

  EXPECT_FALSE(toolbar_model()->HighlightExtensions(ExtensionIdList()));
  EXPECT_EQ(0, observer()->highlight_mode_count());

  // Add the three browser action extensions.
  ASSERT_TRUE(AddBrowserActionExtensions());
  EXPECT_EQ(3u, num_toolbar_items());

  // Highlight one extension.
  ExtensionIdList extension_ids;
  extension_ids.push_back(browser_action_b()->id());
  toolbar_model()->HighlightExtensions(extension_ids);
  EXPECT_EQ(1, observer()->highlight_mode_count());
  EXPECT_TRUE(toolbar_model()->is_highlighting());

  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(0u));

  // Stop highlighting.
  toolbar_model()->StopHighlighting();
  EXPECT_EQ(0, observer()->highlight_mode_count());
  EXPECT_FALSE(toolbar_model()->is_highlighting());

  // Verify that the extensions are back to normal.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(1u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(2u));

  // Call stop highlighting a second time (shouldn't be notified).
  toolbar_model()->StopHighlighting();
  EXPECT_EQ(0, observer()->highlight_mode_count());
  EXPECT_FALSE(toolbar_model()->is_highlighting());

  // Highlight all extensions.
  extension_ids.clear();
  extension_ids.push_back(browser_action_a()->id());
  extension_ids.push_back(browser_action_b()->id());
  extension_ids.push_back(browser_action_c()->id());
  toolbar_model()->HighlightExtensions(extension_ids);
  EXPECT_EQ(1, observer()->highlight_mode_count());
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(1u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(2u));

  // Highlight only extension b (shrink the highlight list).
  extension_ids.clear();
  extension_ids.push_back(browser_action_b()->id());
  toolbar_model()->HighlightExtensions(extension_ids);
  EXPECT_EQ(2, observer()->highlight_mode_count());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(0u));

  // Highlight extensions a and b (grow the highlight list).
  extension_ids.clear();
  extension_ids.push_back(browser_action_a()->id());
  extension_ids.push_back(browser_action_b()->id());
  toolbar_model()->HighlightExtensions(extension_ids);
  EXPECT_EQ(3, observer()->highlight_mode_count());
  EXPECT_EQ(2u, num_toolbar_items());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(1u));

  // Highlight no extensions (empty the highlight list).
  extension_ids.clear();
  toolbar_model()->HighlightExtensions(extension_ids);
  EXPECT_EQ(2, observer()->highlight_mode_count());
  EXPECT_FALSE(toolbar_model()->is_highlighting());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(1u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(2u));
}

TEST_F(ExtensionToolbarModelUnitTest, ExtensionToolbarHighlightModeRemove) {
  Init();

  // Add the three browser action extensions.
  ASSERT_TRUE(AddBrowserActionExtensions());
  EXPECT_EQ(3u, num_toolbar_items());

  // Highlight two of the extensions.
  ExtensionIdList extension_ids;
  extension_ids.push_back(browser_action_a()->id());
  extension_ids.push_back(browser_action_b()->id());
  toolbar_model()->HighlightExtensions(extension_ids);
  EXPECT_TRUE(toolbar_model()->is_highlighting());
  EXPECT_EQ(1, observer()->highlight_mode_count());
  EXPECT_EQ(2u, num_toolbar_items());

  // Disable one of them - only one should remain highlighted.
  service()->DisableExtension(browser_action_a()->id(),
                              Extension::DISABLE_USER_ACTION);
  EXPECT_TRUE(toolbar_model()->is_highlighting());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(0u));

  // Uninstall the remaining highlighted extension. This should result in
  // highlight mode exiting.
  service()->UninstallExtension(browser_action_b()->id(),
                                UNINSTALL_REASON_FOR_TESTING,
                                base::Bind(&base::DoNothing),
                                NULL);  // Ignore error.
  EXPECT_FALSE(toolbar_model()->is_highlighting());
  EXPECT_EQ(0, observer()->highlight_mode_count());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(0u));

  // Test that removing an unhighlighted extension still works.
  // Reinstall extension b, and then highlight extension c.
  ASSERT_TRUE(AddExtension(browser_action_b()));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(1u));
  extension_ids.clear();
  extension_ids.push_back(browser_action_c()->id());
  toolbar_model()->HighlightExtensions(extension_ids);
  EXPECT_EQ(1, observer()->highlight_mode_count());
  EXPECT_TRUE(toolbar_model()->is_highlighting());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(0u));

  // Uninstalling b should not have visible impact.
  service()->UninstallExtension(browser_action_b()->id(),
                                UNINSTALL_REASON_FOR_TESTING,
                                base::Bind(&base::DoNothing),
                                NULL);  // Ignore error.
  EXPECT_TRUE(toolbar_model()->is_highlighting());
  EXPECT_EQ(1, observer()->highlight_mode_count());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(0u));

  // When we stop, only extension c should remain.
  toolbar_model()->StopHighlighting();
  EXPECT_FALSE(toolbar_model()->is_highlighting());
  EXPECT_EQ(0, observer()->highlight_mode_count());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(0u));
}

TEST_F(ExtensionToolbarModelUnitTest, ExtensionToolbarHighlightModeAdd) {
  Init();

  // Add the three browser action extensions.
  ASSERT_TRUE(AddBrowserActionExtensions());
  EXPECT_EQ(3u, num_toolbar_items());

  // Remove one (down to two).
  ASSERT_TRUE(RemoveExtension(browser_action_c()));

  // Highlight one of the two extensions.
  ExtensionIdList extension_ids;
  extension_ids.push_back(browser_action_a()->id());
  toolbar_model()->HighlightExtensions(extension_ids);
  EXPECT_TRUE(toolbar_model()->is_highlighting());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));

  // Adding a new extension should have no visible effect.
  ASSERT_TRUE(AddExtension(browser_action_c()));
  EXPECT_TRUE(toolbar_model()->is_highlighting());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));

  // When we stop highlighting, we should see the new extension show up.
  toolbar_model()->StopHighlighting();
  EXPECT_FALSE(toolbar_model()->is_highlighting());
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(1u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(2u));
}

// Test that the extension toolbar maintains the proper size, even after a pref
// change.
TEST_F(ExtensionToolbarModelUnitTest, ExtensionToolbarSizeAfterPrefChange) {
  Init();

  // Add the three browser action extensions.
  ASSERT_TRUE(AddBrowserActionExtensions());
  EXPECT_EQ(3u, num_toolbar_items());

  // Should be at max size (-1).
  EXPECT_EQ(-1, toolbar_model()->GetVisibleIconCount());
  toolbar_model()->OnExtensionToolbarPrefChange();
  // Should still be at max size.
  EXPECT_EQ(-1, toolbar_model()->GetVisibleIconCount());
}

// Test that, in the absence of the extension-action-redesign switch, the
// model only contains extensions with browser actions.
TEST_F(ExtensionToolbarModelUnitTest, TestToolbarExtensionTypesNoSwitch) {
  Init();
  ASSERT_TRUE(AddActionExtensions());

  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(browser_action(), GetExtensionAtIndex(0u));
}

// Test that, with the extension-action-redesign switch, the model contains
// all types of extensions, except those which should not be displayed on the
// toolbar (like component extensions).
TEST_F(ExtensionToolbarModelUnitTest, TestToolbarExtensionTypesSwitch) {
  FeatureSwitch::ScopedOverride enable_redesign(
      FeatureSwitch::extension_action_redesign(), true);
  Init();
  ASSERT_TRUE(AddActionExtensions());

  // With the switch on, extensions with page actions and no action should also
  // be displayed in the toolbar.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action(), GetExtensionAtIndex(0u));
  EXPECT_EQ(page_action(), GetExtensionAtIndex(1u));
  EXPECT_EQ(no_action(), GetExtensionAtIndex(2u));
}

// Test that hiding actions on the toolbar results in their removal from the
// model when the redesign switch is not enabled.
TEST_F(ExtensionToolbarModelUnitTest,
       ExtensionToolbarActionsVisibilityNoSwitch) {
  Init();

  ASSERT_TRUE(AddBrowserActionExtensions());
  // Sanity check: Order should start as A B C.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(1u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(2u));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  // By default, all actions should be visible.
  EXPECT_TRUE(ExtensionActionAPI::GetBrowserActionVisibility(
                  prefs, browser_action_a()->id()));
  EXPECT_TRUE(ExtensionActionAPI::GetBrowserActionVisibility(
                  prefs, browser_action_b()->id()));
  EXPECT_TRUE(ExtensionActionAPI::GetBrowserActionVisibility(
                  prefs, browser_action_c()->id()));

  // Hiding an action should result in its removal from the toolbar.
  ExtensionActionAPI::SetBrowserActionVisibility(
      prefs, browser_action_b()->id(), false);
  EXPECT_FALSE(ExtensionActionAPI::GetBrowserActionVisibility(
                   prefs, browser_action_b()->id()));
  // Thus, there should now only be two items on the toolbar - A and C.
  EXPECT_EQ(2u, num_toolbar_items());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(1u));

  // Resetting the visibility to 'true' should result in the extension being
  // added back at its original position.
  ExtensionActionAPI::SetBrowserActionVisibility(
      prefs, browser_action_b()->id(), true);
  EXPECT_TRUE(ExtensionActionAPI::GetBrowserActionVisibility(
                  prefs, browser_action_b()->id()));
  // So the toolbar order should be A B C.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(1u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(2u));
}

// Test that hiding actions on the toolbar results in sending them to the
// overflow menu when the redesign switch is enabled.
TEST_F(ExtensionToolbarModelUnitTest,
       ExtensionToolbarActionsVisibilityWithSwitch) {
  FeatureSwitch::ScopedOverride enable_redesign(
      FeatureSwitch::extension_action_redesign(), true);
  Init();

  // We choose to use all types of extensions here, since the misnamed
  // BrowserActionVisibility is now for toolbar visibility.
  ASSERT_TRUE(AddActionExtensions());

  // For readability, alias extensions A B C.
  const Extension* extension_a = browser_action();
  const Extension* extension_b = page_action();
  const Extension* extension_c = no_action();

  // Sanity check: Order should start as A B C, with all three visible.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(-1, toolbar_model()->GetVisibleIconCount());  // -1 = 'all'.
  EXPECT_EQ(extension_a, GetExtensionAtIndex(0u));
  EXPECT_EQ(extension_b, GetExtensionAtIndex(1u));
  EXPECT_EQ(extension_c, GetExtensionAtIndex(2u));

  ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  // By default, all actions should be visible.
  EXPECT_TRUE(ExtensionActionAPI::GetBrowserActionVisibility(
                  prefs, extension_a->id()));
  EXPECT_TRUE(ExtensionActionAPI::GetBrowserActionVisibility(
                  prefs, extension_c->id()));
  EXPECT_TRUE(ExtensionActionAPI::GetBrowserActionVisibility(
                  prefs, extension_b->id()));

  // Hiding an action should result in it being sent to the overflow menu.
  ExtensionActionAPI::SetBrowserActionVisibility(
      prefs, extension_b->id(), false);

  // Thus, the order should be A C B, with B in the overflow.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(2, toolbar_model()->GetVisibleIconCount());
  EXPECT_EQ(extension_a, GetExtensionAtIndex(0u));
  EXPECT_EQ(extension_c, GetExtensionAtIndex(1u));
  EXPECT_EQ(extension_b, GetExtensionAtIndex(2u));

  // Hiding an extension's action should result in it being sent to the overflow
  // as well, but as the _first_ extension in the overflow.
  ExtensionActionAPI::SetBrowserActionVisibility(
      prefs, extension_a->id(), false);
  // Thus, the order should be C A B, with A and B in the overflow.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(1, toolbar_model()->GetVisibleIconCount());
  EXPECT_EQ(extension_c, GetExtensionAtIndex(0u));
  EXPECT_EQ(extension_a, GetExtensionAtIndex(1u));
  EXPECT_EQ(extension_b, GetExtensionAtIndex(2u));

  // Resetting A's visibility to true should send it back to the visible icons
  // (and should grow visible icons by 1), but it should be added to the end of
  // the visible icon list (not to its original position).
  ExtensionActionAPI::SetBrowserActionVisibility(
      prefs, extension_a->id(), true);
  // So order is C A B, with only B in the overflow.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(2, toolbar_model()->GetVisibleIconCount());
  EXPECT_EQ(extension_c, GetExtensionAtIndex(0u));
  EXPECT_EQ(extension_a, GetExtensionAtIndex(1u));
  EXPECT_EQ(extension_b, GetExtensionAtIndex(2u));

  // Resetting B to be visible should make the order C A B, with no overflow.
  ExtensionActionAPI::SetBrowserActionVisibility(
      prefs, extension_b->id(), true);
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(-1, toolbar_model()->GetVisibleIconCount());  // -1 = 'all'
  EXPECT_EQ(extension_c, GetExtensionAtIndex(0u));
  EXPECT_EQ(extension_a, GetExtensionAtIndex(1u));
  EXPECT_EQ(extension_b, GetExtensionAtIndex(2u));
}

}  // namespace extensions
