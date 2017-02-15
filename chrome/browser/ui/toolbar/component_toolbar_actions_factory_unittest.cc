// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/toolbar/component_toolbar_actions_factory.h"
#include "base/macros.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/value_builder.h"

class ComponentToolbarActionsFactoryTest
    : public extensions::ExtensionServiceTestBase {
 public:
  ComponentToolbarActionsFactoryTest() {}
  ~ComponentToolbarActionsFactoryTest() override {}

  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
  }

  scoped_refptr<const extensions::Extension> CreateExtensionWithId(
      const std::string& extension_id) {
    extensions::DictionaryBuilder manifest;
    manifest.Set(extensions::manifest_keys::kName, "test name")
        .Set(extensions::manifest_keys::kDescription, "test description")
        .Set(extensions::manifest_keys::kManifestVersion, 1)
        .Set(extensions::manifest_keys::kVersion, "1.0.0")
        .Set(extensions::manifest_keys::kBrowserAction,
             extensions::DictionaryBuilder().Build());

    return extensions::ExtensionBuilder()
        .SetManifest(manifest.Build())
        .SetID(extension_id)
        .SetLocation(extensions::Manifest::INTERNAL)
        .Build();
  }

  // Adds |extension| and unloads migrated extensions. Returns true if
  // |extension| was unloaded.
  bool TestUnloadingMigratedExtensions(
      scoped_refptr<const extensions::Extension> extension) {
    service()->AddExtension(extension.get());
    CHECK(registry()->enabled_extensions().Contains(extension->id()));
    ComponentToolbarActionsFactory::GetInstance()->UnloadMigratedExtensions(
        service(), registry());
    return !registry()->enabled_extensions().Contains(extension->id());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ComponentToolbarActionsFactoryTest);
};

TEST_F(ComponentToolbarActionsFactoryTest, UnloadMigratedExtensions) {
  EXPECT_TRUE(TestUnloadingMigratedExtensions(
      CreateExtensionWithId(ComponentToolbarActionsFactory::kCastExtensionId)));
  EXPECT_TRUE(TestUnloadingMigratedExtensions(CreateExtensionWithId(
      ComponentToolbarActionsFactory::kCastBetaExtensionId)));
  EXPECT_FALSE(TestUnloadingMigratedExtensions(
      extensions::extension_action_test_util::CreateActionExtension(
          "not_migrated_extension",
          extensions::extension_action_test_util::BROWSER_ACTION)));
}
