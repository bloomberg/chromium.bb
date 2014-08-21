// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pepper_permission_util.h"

#include <set>
#include <string>

#include "chrome/common/extensions/features/feature_channel.h"
#include "components/crx_file/id_util.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "testing/gtest/include/gtest/gtest.h"

using chrome::IsExtensionOrSharedModuleWhitelisted;

namespace extensions {

namespace {

// Return an extension with |id| which imports a module with the given
// |import_id|.
scoped_refptr<Extension> CreateExtensionImportingModule(
    const std::string& import_id,
    const std::string& id) {
  scoped_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", "Has Dependent Modules")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Set("import",
               ListBuilder().Append(DictionaryBuilder().Set("id", import_id)))
          .Build();

  return ExtensionBuilder()
      .SetManifest(manifest.Pass())
      .AddFlags(Extension::FROM_WEBSTORE)
      .SetID(id)
      .Build();
}

}  // namespace

TEST(PepperPermissionUtilTest, ExtensionWhitelisting) {
  ScopedCurrentChannel current_channel(chrome::VersionInfo::CHANNEL_UNKNOWN);
  ExtensionSet extensions;
  std::string whitelisted_id =
      crx_file::id_util::GenerateId("whitelisted_extension");
  scoped_ptr<base::DictionaryValue> manifest =
      DictionaryBuilder()
          .Set("name", "Whitelisted Extension")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Build();
  scoped_refptr<Extension> ext = ExtensionBuilder()
                                     .SetManifest(manifest.Pass())
                                     .SetID(whitelisted_id)
                                     .Build();
  extensions.Insert(ext);
  std::set<std::string> whitelist;
  std::string url = std::string("chrome-extension://") + whitelisted_id +
                    std::string("/manifest.nmf");
  std::string bad_scheme_url =
      std::string("http://") + whitelisted_id + std::string("/manifest.nmf");
  std::string bad_host_url = std::string("chrome-extension://") +
                             crx_file::id_util::GenerateId("bad_host");
  std::string("/manifest.nmf");

  EXPECT_FALSE(
      IsExtensionOrSharedModuleWhitelisted(GURL(url), &extensions, whitelist));
  whitelist.insert(whitelisted_id);
  EXPECT_TRUE(
      IsExtensionOrSharedModuleWhitelisted(GURL(url), &extensions, whitelist));
  EXPECT_FALSE(IsExtensionOrSharedModuleWhitelisted(
      GURL(bad_scheme_url), &extensions, whitelist));
  EXPECT_FALSE(IsExtensionOrSharedModuleWhitelisted(
      GURL(bad_host_url), &extensions, whitelist));
}

TEST(PepperPermissionUtilTest, SharedModuleWhitelisting) {
  ScopedCurrentChannel current_channel(chrome::VersionInfo::CHANNEL_UNKNOWN);
  ExtensionSet extensions;
  std::string whitelisted_id = crx_file::id_util::GenerateId("extension_id");
  std::string bad_id = crx_file::id_util::GenerateId("bad_id");

  scoped_ptr<base::DictionaryValue> shared_module_manifest =
      DictionaryBuilder()
          .Set("name", "Whitelisted Shared Module")
          .Set("version", "1.0")
          .Set("manifest_version", 2)
          .Set("export",
               DictionaryBuilder()
                   .Set("resources", ListBuilder().Append("*"))
                   // Add the extension to the whitelist.  This
                   // restricts import to |whitelisted_id| only.
                   .Set("whitelist", ListBuilder().Append(whitelisted_id)))
          .Build();
  scoped_refptr<Extension> shared_module =
      ExtensionBuilder().SetManifest(shared_module_manifest.Pass()).Build();

  scoped_refptr<Extension> ext =
      CreateExtensionImportingModule(shared_module->id(), whitelisted_id);
  std::string extension_url =
      std::string("chrome-extension://") + ext->id() + std::string("/foo.html");

  std::set<std::string> whitelist;
  // Important: whitelist *only* the shared module.
  whitelist.insert(shared_module->id());

  extensions.Insert(ext);
  // This should fail because shared_module is not in the set of extensions.
  EXPECT_FALSE(IsExtensionOrSharedModuleWhitelisted(
      GURL(extension_url), &extensions, whitelist));
  extensions.Insert(shared_module);
  EXPECT_TRUE(IsExtensionOrSharedModuleWhitelisted(
      GURL(extension_url), &extensions, whitelist));
  scoped_refptr<Extension> bad_ext =
      CreateExtensionImportingModule(shared_module->id(), bad_id);
  std::string bad_extension_url = std::string("chrome-extension://") +
                                  bad_ext->id() + std::string("/foo.html");

  extensions.Insert(bad_ext);
  // This should fail because bad_ext is not whitelisted to use shared_module.
  EXPECT_FALSE(IsExtensionOrSharedModuleWhitelisted(
      GURL(bad_extension_url), &extensions, whitelist));

  // Note that the whitelist should be empty after this call, so tests checking
  // for failure to import will fail because of this.
  whitelist.erase(shared_module->id());
  EXPECT_FALSE(IsExtensionOrSharedModuleWhitelisted(
      GURL(extension_url), &extensions, whitelist));
}

}  // namespace extensions
