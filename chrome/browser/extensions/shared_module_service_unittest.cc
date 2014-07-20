// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/pending_extension_manager.h"
#include "chrome/browser/extensions/shared_module_service.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/install_flag.h"
#include "extensions/browser/uninstall_reason.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/id_util.h"
#include "extensions/common/value_builder.h"
#include "sync/api/string_ordinal.h"

namespace extensions {

namespace {

// Return an extension with |id| which imports a module with the given
// |import_id|.
scoped_refptr<Extension> CreateExtensionImportingModule(
    const std::string& import_id,
    const std::string& id,
    const std::string& version) {
  DictionaryBuilder builder;
  builder.Set("name", "Has Dependent Modules")
         .Set("version", version)
         .Set("manifest_version", 2);
  if (!import_id.empty()) {
    builder.Set("import",
                ListBuilder().Append(DictionaryBuilder().Set("id", import_id)));
  }
  scoped_ptr<base::DictionaryValue> manifest = builder.Build();

  return ExtensionBuilder().SetManifest(manifest.Pass())
                           .AddFlags(Extension::FROM_WEBSTORE)
                           .SetID(id)
                           .Build();
}

}  // namespace

class SharedModuleServiceUnitTest : public ExtensionServiceTestBase {
 public:
  SharedModuleServiceUnitTest() :
      // The "export" key is open for dev-channel only, but unit tests
      // run as stable channel on the official Windows build.
      current_channel_(chrome::VersionInfo::CHANNEL_UNKNOWN) {}
 protected:
  virtual void SetUp() OVERRIDE;

  // Install an extension and notify the ExtensionService.
  testing::AssertionResult InstallExtension(const Extension* extension,
                                            bool is_update);
  ScopedCurrentChannel current_channel_;
};

void SharedModuleServiceUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeGoodInstalledExtensionService();
  service()->Init();
}

testing::AssertionResult SharedModuleServiceUnitTest::InstallExtension(
    const Extension* extension,
    bool is_update) {

  const Extension* old = registry()->GetExtensionById(
      extension->id(),
      ExtensionRegistry::ENABLED);

  // Verify the extension is not already installed, if it is not update.
  if (!is_update) {
    if (old)
      return testing::AssertionFailure() << "Extension already installed.";
  } else {
    if (!old)
      return testing::AssertionFailure() << "The extension does not exist.";
  }

  // Notify the service that the extension is installed. This adds it to the
  // registry, notifies interested parties, etc.
  service()->OnExtensionInstalled(
      extension, syncer::StringOrdinal(), kInstallFlagInstallImmediately);

  // Verify that the extension is now installed.
  if (!registry()->GetExtensionById(extension->id(),
                                    ExtensionRegistry::ENABLED)) {
    return testing::AssertionFailure() << "Could not install extension.";
  }

  return testing::AssertionSuccess();
}

TEST_F(SharedModuleServiceUnitTest, AddDependentSharedModules) {
  // Create an extension that has a dependency.
  std::string import_id = id_util::GenerateId("id");
  std::string extension_id = id_util::GenerateId("extension_id");
  scoped_refptr<Extension> extension =
      CreateExtensionImportingModule(import_id, extension_id, "1.0");

  PendingExtensionManager* pending_extension_manager =
      service()->pending_extension_manager();

  // Verify that we don't currently want to install the imported module.
  EXPECT_FALSE(pending_extension_manager->IsIdPending(import_id));

  // Try to satisfy imports for the extension. This should queue the imported
  // module's installation.
  service()->shared_module_service()->SatisfyImports(extension);
  EXPECT_TRUE(pending_extension_manager->IsIdPending(import_id));
}

TEST_F(SharedModuleServiceUnitTest, PruneSharedModulesOnUninstall) {
  // Create a module which exports a resource, and install it.
  scoped_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", "Shared Module")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Set("export",
               DictionaryBuilder().Set("resources",
                                       ListBuilder().Append("foo.js"))).Build();
  scoped_refptr<Extension> shared_module =
      ExtensionBuilder().SetManifest(manifest.Pass())
                        .AddFlags(Extension::FROM_WEBSTORE)
                        .SetID(id_util::GenerateId("shared_module"))
                        .Build();

  EXPECT_TRUE(InstallExtension(shared_module, false));

  std::string extension_id = id_util::GenerateId("extension_id");
  // Create and install an extension that imports our new module.
  scoped_refptr<Extension> importing_extension =
      CreateExtensionImportingModule(shared_module->id(), extension_id, "1.0");
  EXPECT_TRUE(InstallExtension(importing_extension, false));

  // Uninstall the extension that imports our module.
  base::string16 error;
  service()->UninstallExtension(importing_extension->id(),
                                extensions::UNINSTALL_REASON_FOR_TESTING,
                                &error);
  EXPECT_TRUE(error.empty());

  // Since the module was only referenced by that single extension, it should
  // have been uninstalled as a side-effect of uninstalling the extension that
  // depended upon it.
  EXPECT_FALSE(registry()->GetExtensionById(shared_module->id(),
                                            ExtensionRegistry::EVERYTHING));
}

TEST_F(SharedModuleServiceUnitTest, PruneSharedModulesOnUpdate) {
  // Create two modules which export a resource, and install them.
  scoped_ptr<base::DictionaryValue> manifest_1 =
      DictionaryBuilder()
          .Set("name", "Shared Module 1")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Set("export",
               DictionaryBuilder().Set("resources",
                                       ListBuilder().Append("foo.js"))).Build();
  scoped_refptr<Extension> shared_module_1 =
      ExtensionBuilder().SetManifest(manifest_1.Pass())
                        .AddFlags(Extension::FROM_WEBSTORE)
                        .SetID(id_util::GenerateId("shared_module_1"))
                        .Build();
  EXPECT_TRUE(InstallExtension(shared_module_1, false));

  scoped_ptr<base::DictionaryValue> manifest_2 =
      DictionaryBuilder()
          .Set("name", "Shared Module 2")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Set("export",
               DictionaryBuilder().Set("resources",
                                       ListBuilder().Append("foo.js"))).Build();
  scoped_refptr<Extension> shared_module_2 =
      ExtensionBuilder().SetManifest(manifest_2.Pass())
                        .AddFlags(Extension::FROM_WEBSTORE)
                        .SetID(id_util::GenerateId("shared_module_2"))
                        .Build();
  EXPECT_TRUE(InstallExtension(shared_module_2, false));

  std::string extension_id = id_util::GenerateId("extension_id");

  // Create and install an extension v1.0 that imports our new module 1.
  scoped_refptr<Extension> importing_extension_1 =
      CreateExtensionImportingModule(shared_module_1->id(),
                                     extension_id,
                                     "1.0");
  EXPECT_TRUE(InstallExtension(importing_extension_1, false));

  // Create and install a new version of the extension that imports our new
  // module 2.
  scoped_refptr<Extension> importing_extension_2 =
      CreateExtensionImportingModule(shared_module_2->id(),
                                     extension_id,
                                     "1.1");
  EXPECT_TRUE(InstallExtension(importing_extension_2, true));

  // Since the extension v1.1 depends the module 2 insteand module 1.
  // So the module 1 should be uninstalled.
  EXPECT_FALSE(registry()->GetExtensionById(shared_module_1->id(),
                                            ExtensionRegistry::EVERYTHING));
  EXPECT_TRUE(registry()->GetExtensionById(shared_module_2->id(),
                                            ExtensionRegistry::EVERYTHING));

  // Create and install a new version of the extension that does not import any
  // module.
  scoped_refptr<Extension> importing_extension_3 =
      CreateExtensionImportingModule("", extension_id, "1.2");
  EXPECT_TRUE(InstallExtension(importing_extension_3, true));

  // Since the extension v1.2 does not depend any module, so the all models
  // should have been uninstalled.
  EXPECT_FALSE(registry()->GetExtensionById(shared_module_1->id(),
                                            ExtensionRegistry::EVERYTHING));
  EXPECT_FALSE(registry()->GetExtensionById(shared_module_2->id(),
                                            ExtensionRegistry::EVERYTHING));

}

TEST_F(SharedModuleServiceUnitTest, WhitelistedImports) {
  std::string whitelisted_id = id_util::GenerateId("whitelisted");
  std::string nonwhitelisted_id = id_util::GenerateId("nonwhitelisted");
  // Create a module which exports to a restricted whitelist.
  scoped_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", "Shared Module")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Set("export",
               DictionaryBuilder().Set("whitelist",
                                       ListBuilder()
                                           .Append(whitelisted_id))
                                  .Set("resources",
                                       ListBuilder().Append("*"))).Build();
  scoped_refptr<Extension> shared_module =
      ExtensionBuilder().SetManifest(manifest.Pass())
                        .AddFlags(Extension::FROM_WEBSTORE)
                        .SetID(id_util::GenerateId("shared_module"))
                        .Build();

  EXPECT_TRUE(InstallExtension(shared_module, false));

  // Create and install an extension with the whitelisted ID.
  scoped_refptr<Extension> whitelisted_extension =
      CreateExtensionImportingModule(shared_module->id(),
                                     whitelisted_id,
                                     "1.0");
  EXPECT_TRUE(InstallExtension(whitelisted_extension, false));

  // Try to install an extension with an ID that is not whitelisted.
  scoped_refptr<Extension> nonwhitelisted_extension =
      CreateExtensionImportingModule(shared_module->id(),
                                     nonwhitelisted_id,
                                     "1.0");
  EXPECT_FALSE(InstallExtension(nonwhitelisted_extension, false));
}

}  // namespace extensions
