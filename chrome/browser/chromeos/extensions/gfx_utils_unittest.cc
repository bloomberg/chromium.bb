// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/gfx_utils.h"

#include "base/macros.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/test/fake_app_instance.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"

namespace extensions {

namespace {

const char* kGmailArcPackage = "com.google.android.gm";
const char* kGmailExtensionId1 = "pjkljhegncpnkpknbcohdijeoejaedia";
const char* kGmailExtensionId2 = "bjdhhokmhgelphffoafoejjmlfblpdha";

const char* kDriveArcPackage = "com.google.android.apps.docs";
const char* kKeepExtensionId = "hmjkmjkepdijhoojdojkdfohbdgmmhki";

}  // namespace

class DualBadgeMapTest : public ExtensionServiceTestBase {
 public:
  DualBadgeMapTest() {}
  ~DualBadgeMapTest() override { profile_.reset(); }

  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();
    arc_test_.SetUp(profile_.get());
  }

  void TearDown() override { arc_test_.TearDown(); }

  Profile* profile() { return profile_.get(); }

 protected:
  arc::mojom::ArcPackageInfo CreateArcPackage(const std::string& package_name) {
    arc::mojom::ArcPackageInfo package;
    package.package_name = package_name;
    package.package_version = 1;
    package.last_backup_android_id = 1;
    package.last_backup_time = 1;
    package.sync = false;
    return package;
  }

  void AddArcPackage(const arc::mojom::ArcPackageInfo& package) {
    arc_test_.AddPackage(package);
    arc_test_.app_instance()->SendPackageAdded(package);
  }

  void RemoveArcPackage(const arc::mojom::ArcPackageInfo& package) {
    arc_test_.RemovePackage(package);
    arc_test_.app_instance()->SendPackageUninstalled(package.package_name);
  }

  scoped_refptr<Extension> CreateExtension(const std::string& id) {
    return ExtensionBuilder()
        .SetManifest(DictionaryBuilder()
                         .Set("name", "test")
                         .Set("version", "0.1")
                         .Build())
        .SetID(id)
        .Build();
  }

  void AddExtension(const Extension* extension) {
    service()->AddExtension(extension);
  }

  void RemoveExtension(const Extension* extension) {
    service()->UninstallExtension(extension->id(),
                                  extensions::UNINSTALL_REASON_FOR_TESTING,
                                  base::Bind(&base::DoNothing), NULL);
  }

 private:
  ArcAppTest arc_test_;

  DISALLOW_COPY_AND_ASSIGN(DualBadgeMapTest);
};

TEST_F(DualBadgeMapTest, ExtensionToArcAppMapTest) {
  // Install two Gmail extension apps.
  AddExtension(CreateExtension(kGmailExtensionId1).get());
  AddExtension(CreateExtension(kGmailExtensionId2).get());

  EXPECT_EQ("", extensions::util::GetEquivalentInstalledArcApp(
                    profile(), kGmailExtensionId1));
  EXPECT_EQ("", extensions::util::GetEquivalentInstalledArcApp(
                    profile(), kGmailExtensionId2));

  // Install Gmail Playstore app.
  const arc::mojom::ArcPackageInfo app_info =
      CreateArcPackage(kGmailArcPackage);
  AddArcPackage(app_info);
  EXPECT_EQ(kGmailArcPackage, extensions::util::GetEquivalentInstalledArcApp(
                                  profile(), kGmailExtensionId1));
  EXPECT_EQ(kGmailArcPackage, extensions::util::GetEquivalentInstalledArcApp(
                                  profile(), kGmailExtensionId2));

  RemoveArcPackage(app_info);
  EXPECT_EQ("", extensions::util::GetEquivalentInstalledArcApp(
                    profile(), kGmailExtensionId1));
  EXPECT_EQ("", extensions::util::GetEquivalentInstalledArcApp(
                    profile(), kGmailExtensionId2));

  // Install an unrelated Playstore app.
  AddArcPackage(CreateArcPackage(kDriveArcPackage));
  EXPECT_EQ("", extensions::util::GetEquivalentInstalledArcApp(
                    profile(), kGmailExtensionId1));
  EXPECT_EQ("", extensions::util::GetEquivalentInstalledArcApp(
                    profile(), kGmailExtensionId2));
}

TEST_F(DualBadgeMapTest, ArcAppToExtensionMapTest) {
  // Install Gmail Playstore app.
  AddArcPackage(CreateArcPackage(kGmailArcPackage));

  std::vector<std::string> extension_ids =
      extensions::util::GetEquivalentInstalledExtensions(profile(),
                                                         kGmailArcPackage);
  EXPECT_TRUE(0 == extension_ids.size());

  // Install Gmail extension app.
  scoped_refptr<Extension> extension1 = CreateExtension(kGmailExtensionId1);
  AddExtension(extension1.get());
  extension_ids = extensions::util::GetEquivalentInstalledExtensions(
      profile(), kGmailArcPackage);
  EXPECT_TRUE(1 == extension_ids.size());
  EXPECT_TRUE(std::find(extension_ids.begin(), extension_ids.end(),
                        kGmailExtensionId1) != extension_ids.end());

  // Install another Gmail extension app.
  scoped_refptr<Extension> extension2 = CreateExtension(kGmailExtensionId2);
  AddExtension(extension2.get());
  extension_ids = extensions::util::GetEquivalentInstalledExtensions(
      profile(), kGmailArcPackage);
  EXPECT_TRUE(2 == extension_ids.size());
  EXPECT_TRUE(std::find(extension_ids.begin(), extension_ids.end(),
                        kGmailExtensionId1) != extension_ids.end());
  EXPECT_TRUE(std::find(extension_ids.begin(), extension_ids.end(),
                        kGmailExtensionId2) != extension_ids.end());

  RemoveExtension(extension1.get());
  extension_ids = extensions::util::GetEquivalentInstalledExtensions(
      profile(), kGmailArcPackage);
  EXPECT_TRUE(1 == extension_ids.size());
  EXPECT_FALSE(std::find(extension_ids.begin(), extension_ids.end(),
                         kGmailExtensionId1) != extension_ids.end());
  EXPECT_TRUE(std::find(extension_ids.begin(), extension_ids.end(),
                        kGmailExtensionId2) != extension_ids.end());

  RemoveExtension(extension2.get());
  extension_ids = extensions::util::GetEquivalentInstalledExtensions(
      profile(), kGmailArcPackage);
  EXPECT_TRUE(0 == extension_ids.size());

  // Install an unrelated Google Keep extension app.
  scoped_refptr<Extension> extension = CreateExtension(kKeepExtensionId);
  AddExtension(extension.get());
  extension_ids = extensions::util::GetEquivalentInstalledExtensions(
      profile(), kGmailArcPackage);
  EXPECT_TRUE(0 == extension_ids.size());
}

}  // namespace extensions
