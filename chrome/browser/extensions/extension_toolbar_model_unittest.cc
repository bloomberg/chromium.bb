// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/id_util.h"
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
                           .SetID(id_util::GenerateId(name))
                           .Build();
}

}  // namespace

// TODO(devlin): Experiment with moving (some of the)
// ExtensionToolbarModelBrowserTests into here?
class ExtensionToolbarModelUnitTest : public ExtensionServiceTestBase {
 protected:
  // Initialize the ExtensionService, ExtensionToolbarModel, and
  // ExtensionSystem.
  void Init();

  // Add three extensions, one each for browser action, page action, and no
  // action.
  testing::AssertionResult AddActionExtensions() WARN_UNUSED_RESULT;

  ExtensionToolbarModel* toolbar_model() { return toolbar_model_.get(); }
  const Extension* browser_action() { return browser_action_extension_.get(); }
  const Extension* page_action() { return page_action_extension_.get(); }
  const Extension* no_action() { return no_action_extension_.get(); }

 private:
  // The associated (and owned) toolbar model.
  scoped_ptr<ExtensionToolbarModel> toolbar_model_;

  // Three sample extensions.
  scoped_refptr<const Extension> browser_action_extension_;
  scoped_refptr<const Extension> page_action_extension_;
  scoped_refptr<const Extension> no_action_extension_;
};

void ExtensionToolbarModelUnitTest::Init() {
  InitializeEmptyExtensionService();
  toolbar_model_.reset(
      new ExtensionToolbarModel(profile(), ExtensionPrefs::Get(profile())));
  static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile()))->
      SetReady();
  // Run tasks posted to TestExtensionSystem.
  base::RunLoop().RunUntilIdle();
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

  // Add and verify each extension.
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
  for (ExtensionList::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    service()->AddExtension(iter->get());
    if (!registry->enabled_extensions().GetByID((*iter)->id())) {
      return testing::AssertionFailure() << "Failed to install extension: " <<
          (*iter)->name();
    }
  }

  return testing::AssertionSuccess();
}

// Test that, in the absence of the extension-action-redesign switch, the
// model only contains extensions with browser actions.
TEST_F(ExtensionToolbarModelUnitTest, TestToolbarExtensionTypesNoSwitch) {
  Init();
  ASSERT_TRUE(AddActionExtensions());

  const ExtensionList& extensions = toolbar_model()->toolbar_items();
  ASSERT_EQ(1u, extensions.size());
  EXPECT_EQ(browser_action(), extensions[0]);
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
  const ExtensionList& extensions = toolbar_model()->toolbar_items();
  ASSERT_EQ(3u, extensions.size());
  EXPECT_EQ(browser_action(), extensions[0]);
  EXPECT_EQ(page_action(), extensions[1]);
  EXPECT_EQ(no_action(), extensions[2]);
}

}  // namespace extensions
