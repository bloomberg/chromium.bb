// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/extension_toolbar_model_factory.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/test_extension_registry_observer.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

namespace extensions {

namespace {

// A helper class to create test web contents (tabs) for unit tests, without
// inheriting from RenderViewTestHarness. Can create web contents, and will
// clean up after itself upon destruction. Owns all created web contents.
// A few notes:
// - Works well allocated on the stack, because it should be destroyed before
//   associated browser context.
// - Doesn't play nice with web contents created any other way (because of
//   the implementation of RenderViewHostTestEnabler). But if you are creating
//   web contents already, what do you need this for? ;)
// - This will call aura::Env::Create/DeleteInstance(), because that's needed
//   for web contents. Nothing else should call it first. (This could be
//   tweaked by passing in a flag, if there's need.)
// TODO(devlin): Look around and see if this class is needed elsewhere; if so,
// move it there and expand the API a bit (methods to, e.g., delete/close a
// web contents, access existing web contents, etc).
class TestWebContentsFactory {
 public:
  TestWebContentsFactory();
  ~TestWebContentsFactory();

  // Creates a new WebContents with the given |context|, and returns it.
  content::WebContents* CreateWebContents(content::BrowserContext* context);

 private:
  // The test factory (and friends) for creating test web contents.
  scoped_ptr<content::RenderViewHostTestEnabler> rvh_enabler_;

  // The vector of web contents that this class created.
  ScopedVector<content::WebContents> web_contents_;

  DISALLOW_COPY_AND_ASSIGN(TestWebContentsFactory);
};

TestWebContentsFactory::TestWebContentsFactory()
    : rvh_enabler_(new content::RenderViewHostTestEnabler()) {
#if defined(USE_AURA)
  aura::Env::CreateInstance(true);
#endif
}

TestWebContentsFactory::~TestWebContentsFactory() {
  web_contents_.clear();
  // Let any posted tasks for web contents deletion run.
  base::RunLoop().RunUntilIdle();
  rvh_enabler_.reset();
  // Let any posted tasks for RenderProcess/ViewHost deletion run.
  base::RunLoop().RunUntilIdle();

#if defined(USE_AURA)
  aura::Env::DeleteInstance();
#endif
}

content::WebContents* TestWebContentsFactory::CreateWebContents(
    content::BrowserContext* context) {
  scoped_ptr<content::WebContents> web_contents(
      content::WebContentsTester::CreateTestWebContents(context, nullptr));
  DCHECK(web_contents);
  web_contents_.push_back(web_contents.release());
  return web_contents_.back();
}

// Creates a new ExtensionToolbarModel for the given |context|.
KeyedService* BuildToolbarModel(content::BrowserContext* context) {
  return new ExtensionToolbarModel(Profile::FromBrowserContext(context),
                                   ExtensionPrefs::Get(context));
}

// Given a |profile|, assigns the testing keyed service function to
// BuildToolbarModel() and uses it to create and initialize a new
// ExtensionToolbarModel.
ExtensionToolbarModel* CreateToolbarModelForProfile(Profile* profile) {
  ExtensionToolbarModel* model = ExtensionToolbarModel::Get(profile);
  if (model)
    return model;

  // No existing model means it's a new profile (since we, by default, don't
  // create the ToolbarModel in testing).
  ExtensionToolbarModelFactory::GetInstance()->SetTestingFactory(
      profile, &BuildToolbarModel);
  model = ExtensionToolbarModel::Get(profile);
  // Fake the extension system ready signal.
  // HACK ALERT! In production, the ready task on ExtensionSystem (and most
  // everything else on it, too) is shared between incognito and normal
  // profiles, but a TestExtensionSystem doesn't have the concept of "shared".
  // Because of this, we have to set any new profile's TestExtensionSystem's
  // ready task, too.
  static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile))->SetReady();
  // Run tasks posted to TestExtensionSystem.
  base::RunLoop().RunUntilIdle();

  return model;
}

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
  ~ExtensionToolbarModelTestObserver() override;

  size_t inserted_count() const { return inserted_count_; }
  size_t removed_count() const { return removed_count_; }
  size_t moved_count() const { return moved_count_; }
  int highlight_mode_count() const { return highlight_mode_count_; }
  size_t reorder_count() const { return reorder_count_; }

 private:
  // ExtensionToolbarModel::Observer:
  void ToolbarExtensionAdded(const Extension* extension, int index) override {
    ++inserted_count_;
  }

  void ToolbarExtensionRemoved(const Extension* extension) override {
    ++removed_count_;
  }

  void ToolbarExtensionMoved(const Extension* extension, int index) override {
    ++moved_count_;
  }

  void ToolbarExtensionUpdated(const Extension* extension) override {}

  bool ShowExtensionActionPopup(const Extension* extension,
                                bool grant_active_tab) override {
    return false;
  }

  void ToolbarVisibleCountChanged() override {}

  void ToolbarHighlightModeChanged(bool is_highlighting) override {
    // Add one if highlighting, subtract one if not.
    highlight_mode_count_ += is_highlighting ? 1 : -1;
  }

  void OnToolbarModelInitialized() override {}

  void OnToolbarReorderNecessary(content::WebContents* web_contents) override {
    ++reorder_count_;
  }

  Browser* GetBrowser() override { return NULL; }

  ExtensionToolbarModel* model_;

  size_t inserted_count_;
  size_t removed_count_;
  size_t moved_count_;
  // Int because it could become negative (if something goes wrong).
  int highlight_mode_count_;
  size_t reorder_count_;
};

ExtensionToolbarModelTestObserver::ExtensionToolbarModelTestObserver(
    ExtensionToolbarModel* model) : model_(model),
                                    inserted_count_(0),
                                    removed_count_(0),
                                    moved_count_(0),
                                    highlight_mode_count_(0),
                                    reorder_count_(0) {
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

  void TearDown() override;

  // Adds or removes the given |extension| and verify success.
  testing::AssertionResult AddExtension(
      const scoped_refptr<const Extension>& extension) WARN_UNUSED_RESULT;
  testing::AssertionResult RemoveExtension(
      const scoped_refptr<const Extension>& extension) WARN_UNUSED_RESULT;

  // Adds three extensions, all with browser actions.
  testing::AssertionResult AddBrowserActionExtensions() WARN_UNUSED_RESULT;

  // Adds three extensions, one each for browser action, page action, and no
  // action, and are added in that order.
  testing::AssertionResult AddActionExtensions() WARN_UNUSED_RESULT;

  // Returns the extension at the given index in the toolbar model, or NULL
  // if one does not exist.
  // If |model| is specified, it is used. Otherwise, this defaults to
  // |toolbar_model_|.
  const Extension* GetExtensionAtIndex(
      size_t index, const ExtensionToolbarModel* model) const;
  const Extension* GetExtensionAtIndex(size_t index) const;

  ExtensionToolbarModel* toolbar_model() { return toolbar_model_; }

  const ExtensionToolbarModelTestObserver* observer() const {
    return model_observer_.get();
  }
  size_t num_toolbar_items() const {
    return toolbar_model_->toolbar_items().size();
  }
  const Extension* browser_action_a() const { return browser_action_a_.get(); }
  const Extension* browser_action_b() const { return browser_action_b_.get(); }
  const Extension* browser_action_c() const { return browser_action_c_.get(); }
  const Extension* browser_action() const {
    return browser_action_extension_.get();
  }
  const Extension* page_action() const { return page_action_extension_.get(); }
  const Extension* no_action() const { return no_action_extension_.get(); }

 private:
  // Verifies that all extensions in |extensions| are added successfully.
  testing::AssertionResult AddAndVerifyExtensions(
      const ExtensionList& extensions);

  // The toolbar model associated with the testing profile.
  ExtensionToolbarModel* toolbar_model_;

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
  toolbar_model_ = CreateToolbarModelForProfile(profile());
  model_observer_.reset(new ExtensionToolbarModelTestObserver(toolbar_model_));
}

void ExtensionToolbarModelUnitTest::TearDown() {
  model_observer_.reset();
  ExtensionServiceTestBase::TearDown();
}

testing::AssertionResult ExtensionToolbarModelUnitTest::AddExtension(
    const scoped_refptr<const Extension>& extension) {
  if (registry()->enabled_extensions().GetByID(extension->id())) {
    return testing::AssertionFailure() << "Extension " << extension->name() <<
        " already installed!";
  }
  service()->AddExtension(extension.get());
  if (!registry()->enabled_extensions().GetByID(extension->id())) {
    return testing::AssertionFailure() << "Failed to install extension: " <<
        extension->name();
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult ExtensionToolbarModelUnitTest::RemoveExtension(
    const scoped_refptr<const Extension>& extension) {
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
    size_t index, const ExtensionToolbarModel* model) const {
  return index < model->toolbar_items().size()
             ? model->toolbar_items()[index].get()
             : NULL;
}

const Extension* ExtensionToolbarModelUnitTest::GetExtensionAtIndex(
    size_t index) const {
  return GetExtensionAtIndex(index, toolbar_model_);
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
  EXPECT_EQ(extension2.get(), GetExtensionAtIndex(0u));

  // Should be a no-op, but still fires the events.
  toolbar_model()->MoveExtensionIcon(extension2->id(), 0);
  EXPECT_EQ(1u, observer()->moved_count());
  EXPECT_EQ(1u, num_toolbar_items());
  EXPECT_EQ(extension2.get(), GetExtensionAtIndex(0u));

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
  toolbar_model()->MoveExtensionIcon(browser_action_c()->id(), 0);
  EXPECT_EQ(1u, observer()->moved_count());
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(1u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(2u));

  // Order is now C, A, B. Let's put A last.
  toolbar_model()->MoveExtensionIcon(browser_action_a()->id(), 2);
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
  toolbar_model()->MoveExtensionIcon(browser_action_a()->id(), 0);
  EXPECT_EQ(3u, observer()->moved_count());
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(1u));

  // Move A to the location it already occupies.
  toolbar_model()->MoveExtensionIcon(browser_action_a()->id(), 0);
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
  toolbar_model()->MoveExtensionIcon(browser_action_b()->id(), 0);
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

  // Should be at max size.
  EXPECT_TRUE(toolbar_model()->all_icons_visible());
  EXPECT_EQ(num_toolbar_items(), toolbar_model()->visible_icon_count());
  toolbar_model()->OnExtensionToolbarPrefChange();
  // Should still be at max size.
  EXPECT_TRUE(toolbar_model()->all_icons_visible());
  EXPECT_EQ(num_toolbar_items(), toolbar_model()->visible_icon_count());
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

TEST_F(ExtensionToolbarModelUnitTest, ExtensionToolbarIncognitoModeTest) {
  Init();
  ASSERT_TRUE(AddBrowserActionExtensions());

  // Give two extensions incognito access.
  // Note: We use ExtensionPrefs::SetIsIncognitoEnabled instead of
  // util::SetIsIncognitoEnabled because the latter tries to reload the
  // extension, which requries a filepath associated with the extension (and,
  // for this test, reloading the extension is irrelevant to us).
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(profile());
  extension_prefs->SetIsIncognitoEnabled(browser_action_b()->id(), true);
  extension_prefs->SetIsIncognitoEnabled(browser_action_c()->id(), true);

  util::SetIsIncognitoEnabled(browser_action_b()->id(), profile(), true);
  util::SetIsIncognitoEnabled(browser_action_c()->id(), profile(), true);

  // Move C to the second index.
  toolbar_model()->MoveExtensionIcon(browser_action_c()->id(), 1u);
  // Set visible count to 2 so that c is overflowed. State is A C [B].
  toolbar_model()->SetVisibleIconCount(2);
  EXPECT_EQ(1u, observer()->moved_count());

  // Get an incognito profile and toolbar.
  ExtensionToolbarModel* incognito_model =
      CreateToolbarModelForProfile(profile()->GetOffTheRecordProfile());

  ExtensionToolbarModelTestObserver incognito_observer(incognito_model);
  EXPECT_EQ(0u, incognito_observer.moved_count());

  // We should have two items, C and B, and the order should be preserved from
  // the original model.
  EXPECT_EQ(2u, incognito_model->toolbar_items().size());
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(0u, incognito_model));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(1u, incognito_model));

  // Extensions in the overflow menu in the regular toolbar should remain in
  // overflow in the incognito toolbar. So, we should have C [B].
  EXPECT_EQ(1u, incognito_model->visible_icon_count());
  // The regular model should still have two icons visible.
  EXPECT_EQ(2u, toolbar_model()->visible_icon_count());

  // Changing the incognito model size should not affect the regular model.
  incognito_model->SetVisibleIconCount(0);
  EXPECT_EQ(0u, incognito_model->visible_icon_count());
  EXPECT_EQ(2u, toolbar_model()->visible_icon_count());

  // Expanding the incognito model to 2 should register as "all icons"
  // since it is all of the incognito-enabled extensions.
  incognito_model->SetVisibleIconCount(2u);
  EXPECT_EQ(2u, incognito_model->visible_icon_count());
  EXPECT_TRUE(incognito_model->all_icons_visible());

  // Moving icons in the incognito toolbar should not affect the regular
  // toolbar. Incognito currently has C B...
  incognito_model->MoveExtensionIcon(browser_action_b()->id(), 0u);
  // So now it should be B C...
  EXPECT_EQ(1u, incognito_observer.moved_count());
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(0u, incognito_model));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(1u, incognito_model));
  // ... and the regular toolbar should be unaffected.
  EXPECT_EQ(browser_action_a(), GetExtensionAtIndex(0u));
  EXPECT_EQ(browser_action_c(), GetExtensionAtIndex(1u));
  EXPECT_EQ(browser_action_b(), GetExtensionAtIndex(2u));

  // Similarly, the observer for the regular model should not have received
  // any updates.
  EXPECT_EQ(1u, observer()->moved_count());

  // And performing moves on the regular model should have no effect on the
  // incognito model or its observers.
  toolbar_model()->MoveExtensionIcon(browser_action_c()->id(), 2u);
  EXPECT_EQ(2u, observer()->moved_count());
  EXPECT_EQ(1u, incognito_observer.moved_count());
}

// Test that enabling extensions incognito with an active incognito profile
// works.
TEST_F(ExtensionToolbarModelUnitTest,
       ExtensionToolbarIncognitoEnableExtension) {
  Init();

  const char* kManifest =
      "{"
      "  \"name\": \"%s\","
      "  \"version\": \"1.0\","
      "  \"manifest_version\": 2,"
      "  \"browser_action\": {}"
      "}";

  // For this test, we need to have "real" extension files, because we need to
  // be able to reload them during the incognito process. Since the toolbar
  // needs to be notified of the reload, we need it this time (as opposed to
  // above, where we simply set the prefs before the incognito bar was
  // created.
  TestExtensionDir dir1;
  dir1.WriteManifest(base::StringPrintf(kManifest, "incognito1"));
  TestExtensionDir dir2;
  dir2.WriteManifest(base::StringPrintf(kManifest, "incognito2"));

  TestExtensionDir* dirs[] = { &dir1, &dir2 };
  const Extension* extensions[] = { nullptr, nullptr };
  for (size_t i = 0; i < arraysize(dirs); ++i) {
    // The extension id will be calculated from the file path; we need this to
    // wait for the extension to load.
    base::FilePath path_for_id =
        base::MakeAbsoluteFilePath(dirs[i]->unpacked_path());
    std::string id = crx_file::id_util::GenerateIdForPath(path_for_id);
    TestExtensionRegistryObserver observer(registry(), id);
    UnpackedInstaller::Create(service())->Load(dirs[i]->unpacked_path());
    observer.WaitForExtensionLoaded();
    extensions[i] = registry()->enabled_extensions().GetByID(id);
    ASSERT_TRUE(extensions[i]);
  }

  // For readability, alias to A and B. Since we'll be reloading these
  // extensions, we also can't rely on pointers.
  std::string extension_a = extensions[0]->id();
  std::string extension_b = extensions[1]->id();

  // The first model should have both extensions visible.
  EXPECT_EQ(2u, toolbar_model()->toolbar_items().size());
  EXPECT_EQ(extension_a, GetExtensionAtIndex(0)->id());
  EXPECT_EQ(extension_b, GetExtensionAtIndex(1)->id());

  // Set the model to only show one extension, so the order is A [B].
  toolbar_model()->SetVisibleIconCount(1u);

  // Get an incognito profile and toolbar.
  ExtensionToolbarModel* incognito_model =
      CreateToolbarModelForProfile(profile()->GetOffTheRecordProfile());
  ExtensionToolbarModelTestObserver incognito_observer(incognito_model);

  // Right now, no extensions are enabled in incognito mode.
  EXPECT_EQ(0u, incognito_model->toolbar_items().size());

  // Set extension b (which is overflowed) to be enabled in incognito. This
  // results in b reloading, so wait for it.
  {
    TestExtensionRegistryObserver observer(registry(), extension_b);
    util::SetIsIncognitoEnabled(extension_b, profile(), true);
    observer.WaitForExtensionLoaded();
  }

  // Now, we should have one icon in the incognito bar. But, since B is
  // overflowed in the main bar, it shouldn't be visible.
  EXPECT_EQ(1u, incognito_model->toolbar_items().size());
  EXPECT_EQ(extension_b, GetExtensionAtIndex(0u, incognito_model)->id());
  EXPECT_EQ(0u, incognito_model->visible_icon_count());

  // Also enable extension a for incognito (again, wait for the reload).
  {
    TestExtensionRegistryObserver observer(registry(), extension_a);
    util::SetIsIncognitoEnabled(extension_a, profile(), true);
    observer.WaitForExtensionLoaded();
  }

  // Now, both extensions should be enabled in incognito mode. In addition, the
  // incognito toolbar should have expanded to show extension a (since it isn't
  // overflowed in the main bar).
  EXPECT_EQ(2u, incognito_model->toolbar_items().size());
  EXPECT_EQ(extension_a, GetExtensionAtIndex(0u, incognito_model)->id());
  EXPECT_EQ(extension_b, GetExtensionAtIndex(1u, incognito_model)->id());
  EXPECT_EQ(1u, incognito_model->visible_icon_count());
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
  EXPECT_TRUE(toolbar_model()->all_icons_visible());
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
  EXPECT_EQ(2u, toolbar_model()->visible_icon_count());
  EXPECT_EQ(extension_a, GetExtensionAtIndex(0u));
  EXPECT_EQ(extension_c, GetExtensionAtIndex(1u));
  EXPECT_EQ(extension_b, GetExtensionAtIndex(2u));

  // Hiding an extension's action should result in it being sent to the overflow
  // as well, but as the _first_ extension in the overflow.
  ExtensionActionAPI::SetBrowserActionVisibility(
      prefs, extension_a->id(), false);
  // Thus, the order should be C A B, with A and B in the overflow.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_EQ(1u, toolbar_model()->visible_icon_count());
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
  EXPECT_EQ(2u, toolbar_model()->visible_icon_count());
  EXPECT_EQ(extension_c, GetExtensionAtIndex(0u));
  EXPECT_EQ(extension_a, GetExtensionAtIndex(1u));
  EXPECT_EQ(extension_b, GetExtensionAtIndex(2u));

  // Resetting B to be visible should make the order C A B, with no overflow.
  ExtensionActionAPI::SetBrowserActionVisibility(
      prefs, extension_b->id(), true);
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_TRUE(toolbar_model()->all_icons_visible());
  EXPECT_EQ(extension_c, GetExtensionAtIndex(0u));
  EXPECT_EQ(extension_a, GetExtensionAtIndex(1u));
  EXPECT_EQ(extension_b, GetExtensionAtIndex(2u));
}

// Test that toolbar actions can pop themselves out of overflow if they want
// to act on a given web contents.
TEST_F(ExtensionToolbarModelUnitTest, ToolbarActionsPopOutToAct) {
  // Extensions popping themselves out to act is part of the toolbar redesign,
  // and hidden behind a flag.
  FeatureSwitch::ScopedOverride enable_redesign(
      FeatureSwitch::extension_action_redesign(), true);
  Init();
  TestWebContentsFactory web_contents_factory;

  ASSERT_TRUE(AddActionExtensions());

  // We should start in the order of "browser action" "page action" "no action"
  // and have all extensions visible.
  EXPECT_EQ(3u, num_toolbar_items());
  EXPECT_TRUE(toolbar_model()->all_icons_visible());
  EXPECT_EQ(browser_action(), GetExtensionAtIndex(0u));
  EXPECT_EQ(page_action(), GetExtensionAtIndex(1u));
  EXPECT_EQ(no_action(), GetExtensionAtIndex(2u));

  // Shrink the model to only show one action, and move the page action to the
  // end.
  toolbar_model()->SetVisibleIconCount(1);
  toolbar_model()->MoveExtensionIcon(page_action()->id(), 2u);

  // Quickly verify that the move/visible count worked.
  EXPECT_EQ(1u, toolbar_model()->visible_icon_count());
  EXPECT_EQ(browser_action(), GetExtensionAtIndex(0u));
  EXPECT_EQ(no_action(), GetExtensionAtIndex(1u));
  EXPECT_EQ(page_action(), GetExtensionAtIndex(2u));

  // Create two test web contents, and a session tab helper for each. We need
  // a session tab helper, since we rely on tab ids.
  content::WebContents* web_contents =
      web_contents_factory.CreateWebContents(profile());
  ASSERT_TRUE(web_contents);
  SessionTabHelper::CreateForWebContents(web_contents);
  content::WebContents* second_web_contents =
      web_contents_factory.CreateWebContents(profile());
  ASSERT_TRUE(second_web_contents);
  SessionTabHelper::CreateForWebContents(second_web_contents);

  // Find the tab ids, ensure that the two web contents have different ids, and
  // verify that neither is -1 (invalid).
  int tab_id = SessionTabHelper::IdForTab(web_contents);
  int second_tab_id = SessionTabHelper::IdForTab(second_web_contents);
  EXPECT_NE(tab_id, second_tab_id);
  EXPECT_NE(-1, second_tab_id);
  EXPECT_NE(-1, tab_id);

  // First, check the model order for the first tab. Since we haven't changed
  // anything (i.e., no extensions want to act), this should be the same as we
  // left it: "browser action", "no action", "page action", with only one
  // visible.
  ExtensionList tab_order = toolbar_model()->GetItemOrderForTab(web_contents);
  ASSERT_EQ(3u, tab_order.size());
  EXPECT_EQ(browser_action(), tab_order[0].get());
  EXPECT_EQ(no_action(), tab_order[1].get());
  EXPECT_EQ(page_action(), tab_order[2].get());
  EXPECT_EQ(1u, toolbar_model()->GetVisibleIconCountForTab(web_contents));
  // And we should have no notifications to reorder the toolbar.
  EXPECT_EQ(0u, observer()->reorder_count());

  // Make "page action" want to act by making it's page action visible on the
  // first tab, and notify the API of the change.
  ExtensionActionManager* action_manager =
      ExtensionActionManager::Get(profile());
  ExtensionAction* action = action_manager->GetExtensionAction(*page_action());
  ASSERT_TRUE(action);
  action->SetIsVisible(tab_id, true);
  ExtensionActionAPI* extension_action_api = ExtensionActionAPI::Get(profile());
  extension_action_api->NotifyChange(action, web_contents, profile());

  // This should result in "page action" being popped out of the overflow menu.
  // This has two visible effects:
  // - page action should move to the second index (the one right after the last
  //   originally-visible).
  // - The visible count should increase by one (so page action is visible).
  tab_order = toolbar_model()->GetItemOrderForTab(web_contents);
  ASSERT_EQ(3u, tab_order.size());
  EXPECT_EQ(browser_action(), tab_order[0].get());
  EXPECT_EQ(page_action(), tab_order[1].get());
  EXPECT_EQ(no_action(), tab_order[2].get());
  EXPECT_EQ(2u, toolbar_model()->GetVisibleIconCountForTab(web_contents));
  // We should also have been told to reorder the toolbar.
  EXPECT_EQ(1u, observer()->reorder_count());

  // This should not have any effect on the second tab, which should still have
  // the original order and visible count.
  tab_order = toolbar_model()->GetItemOrderForTab(second_web_contents);
  ASSERT_EQ(3u, tab_order.size());
  EXPECT_EQ(browser_action(), tab_order[0].get());
  EXPECT_EQ(no_action(), tab_order[1].get());
  EXPECT_EQ(page_action(), tab_order[2].get());
  EXPECT_EQ(1u,
            toolbar_model()->GetVisibleIconCountForTab(second_web_contents));

  // Now, set the action to be hidden again, and notify of the change.
  action->SetIsVisible(tab_id, false);
  extension_action_api->NotifyChange(action, web_contents, profile());
  // The order and visible count should return to normal (the page action should
  // move back to its original index in overflow). So, order should be "browser
  // action", "no action", "page action".
  tab_order = toolbar_model()->GetItemOrderForTab(web_contents);
  ASSERT_EQ(3u, tab_order.size());
  EXPECT_EQ(browser_action(), tab_order[0].get());
  EXPECT_EQ(no_action(), tab_order[1].get());
  EXPECT_EQ(page_action(), tab_order[2].get());
  EXPECT_EQ(1u, toolbar_model()->GetVisibleIconCountForTab(web_contents));
  // This should also result in a reorder.
  EXPECT_EQ(2u, observer()->reorder_count());

  // Move page action to the first index (so it's naturally visible), and make
  // it want to act.
  toolbar_model()->MoveExtensionIcon(page_action()->id(), 0u);
  action->SetIsVisible(tab_id, true);
  extension_action_api->NotifyChange(action, web_contents, profile());
  // Since the action is already visible, this should have no effect - the order
  // and visible count should remain unchanged. Order is "page action", "browser
  // action", "no action".
  tab_order = toolbar_model()->GetItemOrderForTab(web_contents);
  ASSERT_EQ(3u, tab_order.size());
  EXPECT_EQ(page_action(), tab_order[0].get());
  EXPECT_EQ(browser_action(), tab_order[1].get());
  EXPECT_EQ(no_action(), tab_order[2].get());
  EXPECT_EQ(1u, toolbar_model()->GetVisibleIconCountForTab(web_contents));

  // We should still be able to increase the size of the model, and to move the
  // page action.
  toolbar_model()->SetVisibleIconCount(2);
  toolbar_model()->MoveExtensionIcon(page_action()->id(), 1u);
  tab_order = toolbar_model()->GetItemOrderForTab(web_contents);
  ASSERT_EQ(3u, tab_order.size());
  EXPECT_EQ(browser_action(), tab_order[0].get());
  EXPECT_EQ(page_action(), tab_order[1].get());
  EXPECT_EQ(no_action(), tab_order[2].get());
  EXPECT_EQ(2u, toolbar_model()->GetVisibleIconCountForTab(web_contents));

  // Neither of the above operations should have precipitated a reorder.
  EXPECT_EQ(2u, observer()->reorder_count());

  // If we moved the page action, the move should remain in effect even after
  // the action no longer wants to act.
  action->SetIsVisible(tab_id, false);
  extension_action_api->NotifyChange(action, web_contents, profile());
  tab_order = toolbar_model()->GetItemOrderForTab(web_contents);
  ASSERT_EQ(3u, tab_order.size());
  EXPECT_EQ(browser_action(), tab_order[0].get());
  EXPECT_EQ(page_action(), tab_order[1].get());
  EXPECT_EQ(no_action(), tab_order[2].get());
  EXPECT_EQ(2u, toolbar_model()->GetVisibleIconCountForTab(web_contents));
  // The above change should *not* require a reorder, because the extension is
  // in a new, visible spot and doesn't need to change its position.
  EXPECT_EQ(2u, observer()->reorder_count());

  // Test the edge case of having no icons visible.
  toolbar_model()->SetVisibleIconCount(0);
  EXPECT_EQ(0u, toolbar_model()->GetVisibleIconCountForTab(web_contents));
  action->SetIsVisible(tab_id, true);
  extension_action_api->NotifyChange(action, web_contents, profile());
  tab_order = toolbar_model()->GetItemOrderForTab(web_contents);
  ASSERT_EQ(3u, tab_order.size());
  EXPECT_EQ(page_action(), tab_order[0].get());
  EXPECT_EQ(browser_action(), tab_order[1].get());
  EXPECT_EQ(no_action(), tab_order[2].get());
  EXPECT_EQ(1u, toolbar_model()->GetVisibleIconCountForTab(web_contents));
  EXPECT_EQ(3u, observer()->reorder_count());
}

}  // namespace extensions
