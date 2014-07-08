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
#include "extensions/common/extension_builder.h"
#include "extensions/common/id_util.h"
#include "extensions/common/value_builder.h"
#include "sync/api/string_ordinal.h"

namespace extensions {

namespace {

// Return an extension with |id| which imports a module with the given
// |import_id|.
scoped_refptr<Extension> CreateExtensionImportingModule(
    const std::string& import_id, const std::string& id) {
  scoped_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", "Has Dependent Modules")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Set("import",
               ListBuilder().Append(DictionaryBuilder().Set("id", import_id)))
          .Build();

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
  testing::AssertionResult InstallExtension(const Extension* extension);
  ScopedCurrentChannel current_channel_;
};

void SharedModuleServiceUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeGoodInstalledExtensionService();
  service_->Init();
}

testing::AssertionResult SharedModuleServiceUnitTest::InstallExtension(
    const Extension* extension) {
  // Verify the extension is not already installed.
  if (registry()->GetExtensionById(extension->id(),
                                   ExtensionRegistry::ENABLED)) {
    return testing::AssertionFailure() << "Extension already installed.";
  }

  // Notify the service that the extension is installed. This adds it to the
  // registry, notifies interested parties, etc.
  service_->OnExtensionInstalled(
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
      CreateExtensionImportingModule(import_id, extension_id);

  PendingExtensionManager* pending_extension_manager =
      service_->pending_extension_manager();

  // Verify that we don't currently want to install the imported module.
  EXPECT_FALSE(pending_extension_manager->IsIdPending(import_id));

  // Try to satisfy imports for the extension. This should queue the imported
  // module's installation.
  service_->shared_module_service()->SatisfyImports(extension);
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

  EXPECT_TRUE(InstallExtension(shared_module));

  std::string extension_id = id_util::GenerateId("extension_id");
  // Create and install an extension that imports our new module.
  scoped_refptr<Extension> importing_extension =
      CreateExtensionImportingModule(shared_module->id(), extension_id);
  EXPECT_TRUE(InstallExtension(importing_extension));

  // Uninstall the extension that imports our module.
  base::string16 error;
  service_->UninstallExtension(importing_extension->id(),
                               ExtensionService::UNINSTALL_REASON_FOR_TESTING,
                               &error);
  EXPECT_TRUE(error.empty());

  // Since the module was only referenced by that single extension, it should
  // have been uninstalled as a side-effect of uninstalling the extension that
  // depended upon it.
  EXPECT_FALSE(registry()->GetExtensionById(shared_module->id(),
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

  EXPECT_TRUE(InstallExtension(shared_module));

  // Create and install an extension with the whitelisted ID.
  scoped_refptr<Extension> whitelisted_extension =
      CreateExtensionImportingModule(shared_module->id(), whitelisted_id);
  EXPECT_TRUE(InstallExtension(whitelisted_extension));

  // Try to install an extension with an ID that is not whitelisted.
  scoped_refptr<Extension> nonwhitelisted_extension =
      CreateExtensionImportingModule(shared_module->id(), nonwhitelisted_id);
  EXPECT_FALSE(InstallExtension(nonwhitelisted_extension));
}

}  // namespace extensions
