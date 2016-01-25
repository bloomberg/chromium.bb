// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/test/mock_entropy_provider.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_service_test_with_install.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/sync_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/crx_file/id_util.h"
#include "components/variations/variations_associated_data.h"
#include "extensions/browser/app_sorting.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/test_management_policy.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_url_handlers.h"
#include "extensions/common/permissions/permission_set.h"
#include "extensions/common/value_builder.h"
#include "sync/api/fake_sync_change_processor.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error_factory_mock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/permission_request_creator.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#endif

using extensions::AppSorting;
using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionSyncData;
using extensions::ExtensionSystem;
using extensions::Manifest;
using extensions::PermissionSet;
using syncer::SyncChange;
using syncer::SyncChangeList;

namespace {

const char good0[] = "behllobkkfkfnphdnhnkndlbkcpglgmj";
const char good2[] = "bjafgdebaacbbbecmhlhpofkepfkgcpa";
const char good_crx[] = "ldnnhddmnhbkjipkidpdiheffobcpfmf";
const char page_action[] = "obcimlgaoabeegjmmpldobjndiealpln";
const char theme2_crx[] = "pjpgmfcmabopnnfonnhmdjglfpjjfkbf";

SyncChangeList MakeSyncChangeList(const std::string& id,
                                  const sync_pb::EntitySpecifics& specifics,
                                  SyncChange::SyncChangeType change_type) {
  syncer::SyncData sync_data =
      syncer::SyncData::CreateLocalData(id, "Name", specifics);
  return SyncChangeList(1, SyncChange(FROM_HERE, change_type, sync_data));
}

}  // namespace

class ExtensionServiceSyncTest
    : public extensions::ExtensionServiceTestWithInstall {
 public:
  void MockSyncStartFlare(bool* was_called,
                          syncer::ModelType* model_type_passed_in,
                          syncer::ModelType model_type) {
    *was_called = true;
    *model_type_passed_in = model_type;
  }

 protected:
  // Paths to some of the fake extensions.
  base::FilePath good0_path() {
    return data_dir()
        .AppendASCII("good")
        .AppendASCII("Extensions")
        .AppendASCII(good0)
        .AppendASCII("1.0.0.0");
  }

  ExtensionSyncService* extension_sync_service() {
    return ExtensionSyncService::Get(profile());
  }
};

TEST_F(ExtensionServiceSyncTest, DeferredSyncStartupPreInstalledComponent) {
  InitializeEmptyExtensionService();

  bool flare_was_called = false;
  syncer::ModelType triggered_type(syncer::UNSPECIFIED);
  base::WeakPtrFactory<ExtensionServiceSyncTest> factory(this);
  extension_sync_service()->SetSyncStartFlareForTesting(
      base::Bind(&ExtensionServiceSyncTest::MockSyncStartFlare,
                 factory.GetWeakPtr(),
                 &flare_was_called,  // Safe due to WeakPtrFactory scope.
                 &triggered_type));  // Safe due to WeakPtrFactory scope.

  // Install a component extension.
  std::string manifest;
  ASSERT_TRUE(base::ReadFileToString(
      good0_path().Append(extensions::kManifestFilename), &manifest));
  service()->component_loader()->Add(manifest, good0_path());
  ASSERT_FALSE(service()->is_ready());
  service()->Init();
  ASSERT_TRUE(service()->is_ready());

  // Extensions added before service is_ready() don't trigger sync startup.
  EXPECT_FALSE(flare_was_called);
  ASSERT_EQ(syncer::UNSPECIFIED, triggered_type);
}

TEST_F(ExtensionServiceSyncTest, DeferredSyncStartupPreInstalledNormal) {
  InitializeGoodInstalledExtensionService();

  bool flare_was_called = false;
  syncer::ModelType triggered_type(syncer::UNSPECIFIED);
  base::WeakPtrFactory<ExtensionServiceSyncTest> factory(this);
  extension_sync_service()->SetSyncStartFlareForTesting(
      base::Bind(&ExtensionServiceSyncTest::MockSyncStartFlare,
                 factory.GetWeakPtr(),
                 &flare_was_called,  // Safe due to WeakPtrFactory scope.
                 &triggered_type));  // Safe due to WeakPtrFactory scope.

  ASSERT_FALSE(service()->is_ready());
  service()->Init();
  ASSERT_EQ(3u, loaded_.size());
  ASSERT_TRUE(service()->is_ready());

  // Extensions added before service is_ready() don't trigger sync startup.
  EXPECT_FALSE(flare_was_called);
  ASSERT_EQ(syncer::UNSPECIFIED, triggered_type);
}

TEST_F(ExtensionServiceSyncTest, DeferredSyncStartupOnInstall) {
  InitializeEmptyExtensionService();
  service()->Init();
  ASSERT_TRUE(service()->is_ready());

  bool flare_was_called = false;
  syncer::ModelType triggered_type(syncer::UNSPECIFIED);
  base::WeakPtrFactory<ExtensionServiceSyncTest> factory(this);
  extension_sync_service()->SetSyncStartFlareForTesting(
      base::Bind(&ExtensionServiceSyncTest::MockSyncStartFlare,
                 factory.GetWeakPtr(),
                 &flare_was_called,  // Safe due to WeakPtrFactory scope.
                 &triggered_type));  // Safe due to WeakPtrFactory scope.

  base::FilePath path = data_dir().AppendASCII("good.crx");
  InstallCRX(path, INSTALL_NEW);

  EXPECT_TRUE(flare_was_called);
  EXPECT_EQ(syncer::EXTENSIONS, triggered_type);

  // Reset.
  flare_was_called = false;
  triggered_type = syncer::UNSPECIFIED;

  // Once sync starts, flare should no longer be invoked.
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));
  path = data_dir().AppendASCII("page_action.crx");
  InstallCRX(path, INSTALL_NEW);
  EXPECT_FALSE(flare_was_called);
  ASSERT_EQ(syncer::UNSPECIFIED, triggered_type);
}

TEST_F(ExtensionServiceSyncTest, DisableExtensionFromSync) {
  // Start the extensions service with one external extension already installed.
  base::FilePath source_install_dir =
      data_dir().AppendASCII("good").AppendASCII("Extensions");
  base::FilePath pref_path =
      source_install_dir.DirName().Append(chrome::kPreferencesFilename);

  InitializeInstalledExtensionService(pref_path, source_install_dir);

  // The user has enabled sync.
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile());
  sync_service->SetFirstSetupComplete();

  service()->Init();
  ASSERT_TRUE(service()->is_ready());

  ASSERT_EQ(3u, loaded_.size());

  // We start enabled.
  const Extension* extension = service()->GetExtensionById(good0, true);
  ASSERT_TRUE(extension);
  ASSERT_TRUE(service()->IsExtensionEnabled(good0));

  // Sync starts up.
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  // Then sync data arrives telling us to disable |good0|.
  ExtensionSyncData disable_good_crx(*extension, false,
                                     Extension::DISABLE_USER_ACTION, false,
                                     false, ExtensionSyncData::BOOLEAN_UNSET);
  SyncChangeList list(
      1, disable_good_crx.GetSyncChange(SyncChange::ACTION_UPDATE));
  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

  ASSERT_FALSE(service()->IsExtensionEnabled(good0));
}

TEST_F(ExtensionServiceSyncTest, IgnoreSyncChangesWhenLocalStateIsMoreRecent) {
  // Start the extension service with three extensions already installed.
  base::FilePath source_install_dir =
      data_dir().AppendASCII("good").AppendASCII("Extensions");
  base::FilePath pref_path =
      source_install_dir.DirName().Append(chrome::kPreferencesFilename);

  InitializeInstalledExtensionService(pref_path, source_install_dir);

  // The user has enabled sync.
  ProfileSyncService* sync_service =
      ProfileSyncServiceFactory::GetForProfile(profile());
  sync_service->SetFirstSetupComplete();
  // Make sure ExtensionSyncService is created, so it'll be notified of changes.
  extension_sync_service();

  service()->Init();
  ASSERT_TRUE(service()->is_ready());
  ASSERT_EQ(3u, loaded_.size());

  ASSERT_TRUE(service()->IsExtensionEnabled(good0));
  ASSERT_TRUE(service()->IsExtensionEnabled(good2));

  // Disable and re-enable good0 before first sync data arrives.
  service()->DisableExtension(good0, Extension::DISABLE_USER_ACTION);
  ASSERT_FALSE(service()->IsExtensionEnabled(good0));
  service()->EnableExtension(good0);
  ASSERT_TRUE(service()->IsExtensionEnabled(good0));
  // Disable good2 before first sync data arrives (good1 is considered
  // non-syncable because it has plugin permission).
  service()->DisableExtension(good2, Extension::DISABLE_USER_ACTION);
  ASSERT_FALSE(service()->IsExtensionEnabled(good2));

  const Extension* extension0 = service()->GetExtensionById(good0, true);
  const Extension* extension2 = service()->GetExtensionById(good2, true);
  ASSERT_TRUE(extensions::sync_helper::IsSyncable(extension0));
  ASSERT_TRUE(extensions::sync_helper::IsSyncable(extension2));

  // Now sync data comes in that says to disable good0 and enable good2.
  ExtensionSyncData disable_good0(*extension0, false,
                                  Extension::DISABLE_USER_ACTION, false, false,
                                  ExtensionSyncData::BOOLEAN_UNSET);
  ExtensionSyncData enable_good2(*extension2, true, Extension::DISABLE_NONE,
                                 false, false,
                                 ExtensionSyncData::BOOLEAN_UNSET);
  syncer::SyncDataList sync_data;
  sync_data.push_back(disable_good0.GetSyncData());
  sync_data.push_back(enable_good2.GetSyncData());
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS,
      sync_data,
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  // Both sync changes should be ignored, since the local state was changed
  // before sync started, and so the local state is considered more recent.
  EXPECT_TRUE(service()->IsExtensionEnabled(good0));
  EXPECT_FALSE(service()->IsExtensionEnabled(good2));
}

TEST_F(ExtensionServiceSyncTest, DontSelfNotify) {
  // Start the extension service with three extensions already installed.
  base::FilePath source_install_dir =
      data_dir().AppendASCII("good").AppendASCII("Extensions");
  base::FilePath pref_path =
      source_install_dir.DirName().Append(chrome::kPreferencesFilename);

  InitializeInstalledExtensionService(pref_path, source_install_dir);

  // The user has enabled sync.
  ProfileSyncServiceFactory::GetForProfile(profile())->SetFirstSetupComplete();
  // Make sure ExtensionSyncService is created, so it'll be notified of changes.
  extension_sync_service();

  service()->Init();
  ASSERT_TRUE(service()->is_ready());
  ASSERT_EQ(3u, loaded_.size());
  ASSERT_TRUE(service()->IsExtensionEnabled(good0));

  syncer::FakeSyncChangeProcessor* processor =
      new syncer::FakeSyncChangeProcessor;
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS,
      syncer::SyncDataList(),
      make_scoped_ptr(processor),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  processor->changes().clear();

  // Simulate various incoming sync changes, and make sure they don't result in
  // any outgoing changes.

  {
    const Extension* extension = service()->GetExtensionById(good0, true);
    ASSERT_TRUE(extension);

    // Disable the extension.
    ExtensionSyncData data(*extension, false, Extension::DISABLE_USER_ACTION,
                           false, false, ExtensionSyncData::BOOLEAN_UNSET);
    SyncChangeList list(1, data.GetSyncChange(SyncChange::ACTION_UPDATE));

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

    EXPECT_TRUE(processor->changes().empty());
  }

  {
    const Extension* extension = service()->GetExtensionById(good0, true);
    ASSERT_TRUE(extension);

    // Set incognito enabled to true.
    ExtensionSyncData data(*extension, false, Extension::DISABLE_NONE, true,
                           false, ExtensionSyncData::BOOLEAN_UNSET);
    SyncChangeList list(1, data.GetSyncChange(SyncChange::ACTION_UPDATE));

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

    EXPECT_TRUE(processor->changes().empty());
  }

  {
    const Extension* extension = service()->GetExtensionById(good0, true);
    ASSERT_TRUE(extension);

    // Add another disable reason.
    ExtensionSyncData data(*extension, false,
                           Extension::DISABLE_USER_ACTION |
                               Extension::DISABLE_PERMISSIONS_INCREASE,
                           false, false, ExtensionSyncData::BOOLEAN_UNSET);
    SyncChangeList list(1, data.GetSyncChange(SyncChange::ACTION_UPDATE));

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

    EXPECT_TRUE(processor->changes().empty());
  }

  {
    const Extension* extension = service()->GetExtensionById(good0, true);
    ASSERT_TRUE(extension);

    // Uninstall the extension.
    ExtensionSyncData data(*extension, false,
                           Extension::DISABLE_USER_ACTION |
                               Extension::DISABLE_PERMISSIONS_INCREASE,
                           false, false, ExtensionSyncData::BOOLEAN_UNSET);
    SyncChangeList list(1, data.GetSyncChange(SyncChange::ACTION_DELETE));

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

    EXPECT_TRUE(processor->changes().empty());
  }
}

TEST_F(ExtensionServiceSyncTest, GetSyncData) {
  InitializeEmptyExtensionService();
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  const Extension* extension = service()->GetInstalledExtension(good_crx);
  ASSERT_TRUE(extension);

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  syncer::SyncDataList list =
      extension_sync_service()->GetAllSyncData(syncer::EXTENSIONS);
  ASSERT_EQ(list.size(), 1U);
  scoped_ptr<ExtensionSyncData> data =
      ExtensionSyncData::CreateFromSyncData(list[0]);
  ASSERT_TRUE(data.get());
  EXPECT_EQ(extension->id(), data->id());
  EXPECT_FALSE(data->uninstalled());
  EXPECT_EQ(service()->IsExtensionEnabled(good_crx), data->enabled());
  EXPECT_EQ(extensions::util::IsIncognitoEnabled(good_crx, profile()),
            data->incognito_enabled());
  EXPECT_EQ(ExtensionSyncData::BOOLEAN_UNSET, data->all_urls_enabled());
  EXPECT_EQ(data->version(), *extension->version());
  EXPECT_EQ(extensions::ManifestURL::GetUpdateURL(extension),
            data->update_url());
  EXPECT_EQ(extension->name(), data->name());
}

TEST_F(ExtensionServiceSyncTest, GetSyncDataDisableReasons) {
  InitializeEmptyExtensionService();
  const Extension* extension =
      InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  ASSERT_TRUE(extension);

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncData(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    scoped_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_TRUE(data->enabled());
    EXPECT_TRUE(data->supports_disable_reasons());
    EXPECT_EQ(Extension::DISABLE_NONE, data->disable_reasons());
  }

  // Syncable disable reason, should propagate to sync.
  service()->DisableExtension(good_crx, Extension::DISABLE_USER_ACTION);
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncData(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    scoped_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_FALSE(data->enabled());
    EXPECT_TRUE(data->supports_disable_reasons());
    EXPECT_EQ(Extension::DISABLE_USER_ACTION, data->disable_reasons());
  }
  service()->EnableExtension(good_crx);

  // Non-syncable disable reason. The sync data should still say "enabled".
  service()->DisableExtension(good_crx, Extension::DISABLE_RELOAD);
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncData(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    scoped_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_TRUE(data->enabled());
    EXPECT_TRUE(data->supports_disable_reasons());
    EXPECT_EQ(Extension::DISABLE_NONE, data->disable_reasons());
  }
  service()->EnableExtension(good_crx);

  // Both a syncable and a non-syncable disable reason, only the former should
  // propagate to sync.
  service()->DisableExtension(
      good_crx, Extension::DISABLE_USER_ACTION | Extension::DISABLE_RELOAD);
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncData(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    scoped_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_FALSE(data->enabled());
    EXPECT_TRUE(data->supports_disable_reasons());
    EXPECT_EQ(Extension::DISABLE_USER_ACTION, data->disable_reasons());
  }
  service()->EnableExtension(good_crx);
}

TEST_F(ExtensionServiceSyncTest, GetSyncDataTerminated) {
  InitializeEmptyExtensionService();
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  TerminateExtension(good_crx);
  const Extension* extension = service()->GetInstalledExtension(good_crx);
  ASSERT_TRUE(extension);

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  syncer::SyncDataList list =
      extension_sync_service()->GetAllSyncData(syncer::EXTENSIONS);
  ASSERT_EQ(list.size(), 1U);
  scoped_ptr<ExtensionSyncData> data =
      ExtensionSyncData::CreateFromSyncData(list[0]);
  ASSERT_TRUE(data.get());
  EXPECT_EQ(extension->id(), data->id());
  EXPECT_FALSE(data->uninstalled());
  EXPECT_EQ(service()->IsExtensionEnabled(good_crx), data->enabled());
  EXPECT_EQ(extensions::util::IsIncognitoEnabled(good_crx, profile()),
            data->incognito_enabled());
  EXPECT_EQ(ExtensionSyncData::BOOLEAN_UNSET, data->all_urls_enabled());
  EXPECT_EQ(data->version(), *extension->version());
  EXPECT_EQ(extensions::ManifestURL::GetUpdateURL(extension),
            data->update_url());
  EXPECT_EQ(extension->name(), data->name());
}

TEST_F(ExtensionServiceSyncTest, GetSyncDataFilter) {
  InitializeEmptyExtensionService();
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  const Extension* extension = service()->GetInstalledExtension(good_crx);
  ASSERT_TRUE(extension);

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::APPS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  syncer::SyncDataList list =
      extension_sync_service()->GetAllSyncData(syncer::EXTENSIONS);
  ASSERT_EQ(list.size(), 0U);
}

TEST_F(ExtensionServiceSyncTest, GetSyncExtensionDataUserSettings) {
  InitializeEmptyExtensionService();
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  const Extension* extension = service()->GetInstalledExtension(good_crx);
  ASSERT_TRUE(extension);

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncData(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    scoped_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_TRUE(data->enabled());
    EXPECT_FALSE(data->incognito_enabled());
    EXPECT_EQ(ExtensionSyncData::BOOLEAN_UNSET, data->all_urls_enabled());
  }

  service()->DisableExtension(good_crx, Extension::DISABLE_USER_ACTION);
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncData(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    scoped_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_FALSE(data->enabled());
    EXPECT_FALSE(data->incognito_enabled());
    EXPECT_EQ(ExtensionSyncData::BOOLEAN_UNSET, data->all_urls_enabled());
  }

  extensions::util::SetIsIncognitoEnabled(good_crx, profile(), true);
  extensions::util::SetAllowedScriptingOnAllUrls(
      good_crx, profile(), false);
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncData(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    scoped_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_FALSE(data->enabled());
    EXPECT_TRUE(data->incognito_enabled());
    EXPECT_EQ(ExtensionSyncData::BOOLEAN_FALSE, data->all_urls_enabled());
  }

  service()->EnableExtension(good_crx);
  extensions::util::SetAllowedScriptingOnAllUrls(
      good_crx, profile(), true);
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncData(syncer::EXTENSIONS);
    ASSERT_EQ(list.size(), 1U);
    scoped_ptr<ExtensionSyncData> data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(data.get());
    EXPECT_TRUE(data->enabled());
    EXPECT_TRUE(data->incognito_enabled());
    EXPECT_EQ(ExtensionSyncData::BOOLEAN_TRUE, data->all_urls_enabled());
  }
}

TEST_F(ExtensionServiceSyncTest, SyncForUninstalledExternalExtension) {
  InitializeEmptyExtensionService();
  InstallCRXWithLocation(
      data_dir().AppendASCII("good.crx"), Manifest::EXTERNAL_PREF, INSTALL_NEW);
  const Extension* extension = service()->GetInstalledExtension(good_crx);
  ASSERT_TRUE(extension);

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  UninstallExtension(good_crx, false);
  EXPECT_TRUE(
      ExtensionPrefs::Get(profile())->IsExternalExtensionUninstalled(good_crx));

  sync_pb::EntitySpecifics specifics;
  sync_pb::AppSpecifics* app_specifics = specifics.mutable_app();
  sync_pb::ExtensionSpecifics* extension_specifics =
      app_specifics->mutable_extension();
  extension_specifics->set_id(good_crx);
  extension_specifics->set_version("1.0");
  extension_specifics->set_enabled(true);

  SyncChangeList list =
      MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_UPDATE);

  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_TRUE(
      ExtensionPrefs::Get(profile())->IsExternalExtensionUninstalled(good_crx));
}

TEST_F(ExtensionServiceSyncTest, GetSyncAppDataUserSettings) {
  InitializeEmptyExtensionService();
  const Extension* app =
      PackAndInstallCRX(data_dir().AppendASCII("app"), INSTALL_NEW);
  ASSERT_TRUE(app);
  ASSERT_TRUE(app->is_app());

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::APPS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  syncer::StringOrdinal initial_ordinal =
      syncer::StringOrdinal::CreateInitialOrdinal();
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncData(syncer::APPS);
    ASSERT_EQ(list.size(), 1U);

    scoped_ptr<ExtensionSyncData> app_sync_data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    EXPECT_TRUE(initial_ordinal.Equals(app_sync_data->app_launch_ordinal()));
    EXPECT_TRUE(initial_ordinal.Equals(app_sync_data->page_ordinal()));
  }

  AppSorting* sorting = ExtensionSystem::Get(profile())->app_sorting();
  sorting->SetAppLaunchOrdinal(app->id(), initial_ordinal.CreateAfter());
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncData(syncer::APPS);
    ASSERT_EQ(list.size(), 1U);

    scoped_ptr<ExtensionSyncData> app_sync_data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(app_sync_data.get());
    EXPECT_TRUE(initial_ordinal.LessThan(app_sync_data->app_launch_ordinal()));
    EXPECT_TRUE(initial_ordinal.Equals(app_sync_data->page_ordinal()));
  }

  sorting->SetPageOrdinal(app->id(), initial_ordinal.CreateAfter());
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncData(syncer::APPS);
    ASSERT_EQ(list.size(), 1U);

    scoped_ptr<ExtensionSyncData> app_sync_data =
        ExtensionSyncData::CreateFromSyncData(list[0]);
    ASSERT_TRUE(app_sync_data.get());
    EXPECT_TRUE(initial_ordinal.LessThan(app_sync_data->app_launch_ordinal()));
    EXPECT_TRUE(initial_ordinal.LessThan(app_sync_data->page_ordinal()));
  }
}

// TODO (rdevlin.cronin): The OnExtensionMoved() method has been removed from
// ExtensionService, so this test probably needs a new home. Unfortunately, it
// relies pretty heavily on things like InitializeExtension[Sync]Service() and
// PackAndInstallCRX(). When we clean up a bit more, this should move out.
TEST_F(ExtensionServiceSyncTest, GetSyncAppDataUserSettingsOnExtensionMoved) {
  InitializeEmptyExtensionService();
  const size_t kAppCount = 3;
  const Extension* apps[kAppCount];
  apps[0] = PackAndInstallCRX(data_dir().AppendASCII("app1"), INSTALL_NEW);
  apps[1] = PackAndInstallCRX(data_dir().AppendASCII("app2"), INSTALL_NEW);
  apps[2] = PackAndInstallCRX(data_dir().AppendASCII("app4"), INSTALL_NEW);
  for (size_t i = 0; i < kAppCount; ++i) {
    ASSERT_TRUE(apps[i]);
    ASSERT_TRUE(apps[i]->is_app());
  }

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::APPS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  ExtensionSystem::Get(service()->GetBrowserContext())
      ->app_sorting()
      ->OnExtensionMoved(apps[0]->id(), apps[1]->id(), apps[2]->id());
  {
    syncer::SyncDataList list =
        extension_sync_service()->GetAllSyncData(syncer::APPS);
    ASSERT_EQ(list.size(), 3U);

    scoped_ptr<ExtensionSyncData> data[kAppCount];
    for (size_t i = 0; i < kAppCount; ++i) {
      data[i] = ExtensionSyncData::CreateFromSyncData(list[i]);
      ASSERT_TRUE(data[i].get());
    }

    // The sync data is not always in the same order our apps were installed in,
    // so we do that sorting here so we can make sure the values are changed as
    // expected.
    syncer::StringOrdinal app_launch_ordinals[kAppCount];
    for (size_t i = 0; i < kAppCount; ++i) {
      for (size_t j = 0; j < kAppCount; ++j) {
        if (apps[i]->id() == data[j]->id())
          app_launch_ordinals[i] = data[j]->app_launch_ordinal();
      }
    }

    EXPECT_TRUE(app_launch_ordinals[1].LessThan(app_launch_ordinals[0]));
    EXPECT_TRUE(app_launch_ordinals[0].LessThan(app_launch_ordinals[2]));
  }
}

TEST_F(ExtensionServiceSyncTest, GetSyncDataList) {
  InitializeEmptyExtensionService();
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  InstallCRX(data_dir().AppendASCII("page_action.crx"), INSTALL_NEW);
  InstallCRX(data_dir().AppendASCII("theme.crx"), INSTALL_NEW);
  InstallCRX(data_dir().AppendASCII("theme2.crx"), INSTALL_NEW);

  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::APPS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  service()->DisableExtension(page_action, Extension::DISABLE_USER_ACTION);
  TerminateExtension(theme2_crx);

  EXPECT_EQ(0u, extension_sync_service()->GetAllSyncData(syncer::APPS).size());
  EXPECT_EQ(
      2u, extension_sync_service()->GetAllSyncData(syncer::EXTENSIONS).size());
}

TEST_F(ExtensionServiceSyncTest, ProcessSyncDataUninstall) {
  InitializeEmptyExtensionService();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
  ext_specifics->set_id(good_crx);
  ext_specifics->set_version("1.0");

  SyncChangeList list =
      MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_DELETE);

  // Should do nothing.
  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_FALSE(service()->GetExtensionById(good_crx, true));

  // Install the extension.
  base::FilePath extension_path = data_dir().AppendASCII("good.crx");
  InstallCRX(extension_path, INSTALL_NEW);
  EXPECT_TRUE(service()->GetExtensionById(good_crx, true));

  // Should uninstall the extension.
  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_FALSE(service()->GetExtensionById(good_crx, true));

  // Should again do nothing.
  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_FALSE(service()->GetExtensionById(good_crx, true));
}

TEST_F(ExtensionServiceSyncTest, ProcessSyncDataWrongType) {
  InitializeEmptyExtensionService();

  // Install the extension.
  base::FilePath extension_path = data_dir().AppendASCII("good.crx");
  InstallCRX(extension_path, INSTALL_NEW);
  EXPECT_TRUE(service()->GetExtensionById(good_crx, true));

  sync_pb::EntitySpecifics specifics;
  sync_pb::AppSpecifics* app_specifics = specifics.mutable_app();
  sync_pb::ExtensionSpecifics* extension_specifics =
      app_specifics->mutable_extension();
  extension_specifics->set_id(good_crx);
  extension_specifics->set_version(
      service()->GetInstalledExtension(good_crx)->version()->GetString());

  {
    extension_specifics->set_enabled(true);

    SyncChangeList list =
        MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_DELETE);

    // Should do nothing
    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_TRUE(service()->GetExtensionById(good_crx, true));
  }

  {
    extension_specifics->set_enabled(false);

    SyncChangeList list =
        MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_UPDATE);

    // Should again do nothing.
    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_TRUE(service()->GetExtensionById(good_crx, false));
  }
}

TEST_F(ExtensionServiceSyncTest, ProcessSyncDataSettings) {
  InitializeEmptyExtensionService();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_TRUE(service()->IsExtensionEnabled(good_crx));
  EXPECT_FALSE(extensions::util::IsIncognitoEnabled(good_crx, profile()));
  EXPECT_FALSE(extensions::util::HasSetAllowedScriptingOnAllUrls(
      good_crx, profile()));
  const bool kDefaultAllowedScripting =
      extensions::util::DefaultAllowedScriptingOnAllUrls();
  EXPECT_EQ(kDefaultAllowedScripting,
            extensions::util::AllowedScriptingOnAllUrls(good_crx, profile()));

  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
  ext_specifics->set_id(good_crx);
  ext_specifics->set_version(
      service()->GetInstalledExtension(good_crx)->version()->GetString());
  ext_specifics->set_enabled(false);

  {
    SyncChangeList list =
        MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_FALSE(service()->IsExtensionEnabled(good_crx));
    EXPECT_FALSE(extensions::util::IsIncognitoEnabled(good_crx, profile()));
    EXPECT_FALSE(extensions::util::HasSetAllowedScriptingOnAllUrls(
        good_crx, profile()));
    EXPECT_EQ(kDefaultAllowedScripting,
              extensions::util::AllowedScriptingOnAllUrls(good_crx, profile()));
  }

  {
    ext_specifics->set_enabled(true);
    ext_specifics->set_incognito_enabled(true);

    SyncChangeList list =
        MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_TRUE(service()->IsExtensionEnabled(good_crx));
    EXPECT_TRUE(extensions::util::IsIncognitoEnabled(good_crx, profile()));
  }

  {
    ext_specifics->set_enabled(false);
    ext_specifics->set_incognito_enabled(true);

    SyncChangeList list =
        MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_FALSE(service()->IsExtensionEnabled(good_crx));
    EXPECT_TRUE(extensions::util::IsIncognitoEnabled(good_crx, profile()));
  }

  {
    ext_specifics->set_enabled(true);
    ext_specifics->set_all_urls_enabled(!kDefaultAllowedScripting);

    SyncChangeList list =
        MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_TRUE(service()->IsExtensionEnabled(good_crx));
    EXPECT_TRUE(extensions::util::HasSetAllowedScriptingOnAllUrls(
        good_crx, profile()));
    EXPECT_EQ(!kDefaultAllowedScripting,
              extensions::util::AllowedScriptingOnAllUrls(good_crx, profile()));
  }

  {
    ext_specifics->set_all_urls_enabled(kDefaultAllowedScripting);

    SyncChangeList list =
        MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_TRUE(service()->IsExtensionEnabled(good_crx));
    EXPECT_TRUE(extensions::util::HasSetAllowedScriptingOnAllUrls(
        good_crx, profile()));
    EXPECT_EQ(kDefaultAllowedScripting,
              extensions::util::AllowedScriptingOnAllUrls(good_crx, profile()));
  }

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(good_crx));
}

TEST_F(ExtensionServiceSyncTest, ProcessSyncDataNewExtension) {
  InitializeEmptyExtensionService();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  const base::FilePath path = data_dir().AppendASCII("good.crx");
  const ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  struct TestCase {
    const char* name;  // For failure output only.
    bool sync_enabled;  // The "enabled" flag coming in from Sync.
    // The disable reason(s) coming in from Sync, or -1 for "not set".
    int sync_disable_reasons;
    // The disable reason(s) that should be set on the installed extension.
    // This will usually be the same as |sync_disable_reasons|, but see the
    // "Legacy" case.
    int expect_disable_reasons;
    // Whether the extension's permissions should be auto-granted during
    // installation.
    bool expect_permissions_granted;
  } test_cases[] = {
    // Standard case: Extension comes in enabled; permissions should be granted
    // during installation.
    { "Standard", true, 0, 0, true },
    // If the extension comes in disabled, its permissions should still be
    // granted (the user already approved them on another machine).
    { "Disabled", false, Extension::DISABLE_USER_ACTION,
      Extension::DISABLE_USER_ACTION, true },
    // Legacy case (<M45): No disable reasons come in from Sync (see
    // crbug.com/484214). After installation, the reason should be set to
    // DISABLE_USER_ACTION (default assumption).
    { "Legacy", false, -1, Extension::DISABLE_USER_ACTION, true },
    // If the extension came in disabled due to a permissions increase, then the
    // user has *not* approved the permissions, and they shouldn't be granted.
    // crbug.com/484214
    { "PermissionsIncrease", false, Extension::DISABLE_PERMISSIONS_INCREASE,
      Extension::DISABLE_PERMISSIONS_INCREASE, false },
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);

    sync_pb::EntitySpecifics specifics;
    sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
    ext_specifics->set_id(good_crx);
    ext_specifics->set_version(base::Version("1").GetString());
    ext_specifics->set_enabled(test_case.sync_enabled);
    if (test_case.sync_disable_reasons != -1)
      ext_specifics->set_disable_reasons(test_case.sync_disable_reasons);

    SyncChangeList list =
        MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

    ASSERT_TRUE(service()->pending_extension_manager()->IsIdPending(good_crx));
    UpdateExtension(good_crx, path, test_case.sync_enabled ? ENABLED
                                                           : DISABLED);
    EXPECT_EQ(test_case.expect_disable_reasons,
              prefs->GetDisableReasons(good_crx));
    scoped_ptr<const PermissionSet> permissions =
        prefs->GetGrantedPermissions(good_crx);
    EXPECT_EQ(test_case.expect_permissions_granted, !permissions->IsEmpty());
    ASSERT_FALSE(service()->pending_extension_manager()->IsIdPending(good_crx));

    // Remove the extension again, so we can install it again for the next case.
    UninstallExtension(good_crx, false,
                       test_case.sync_enabled ? Extension::ENABLED
                                              : Extension::DISABLED);
  }
}

TEST_F(ExtensionServiceSyncTest, ProcessSyncDataTerminatedExtension) {
  InitializeExtensionServiceWithUpdater();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  TerminateExtension(good_crx);
  EXPECT_TRUE(service()->IsExtensionEnabled(good_crx));
  EXPECT_FALSE(extensions::util::IsIncognitoEnabled(good_crx, profile()));

  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
  ext_specifics->set_id(good_crx);
  ext_specifics->set_version(
      service()->GetInstalledExtension(good_crx)->version()->GetString());
  ext_specifics->set_enabled(false);
  ext_specifics->set_incognito_enabled(true);

  SyncChangeList list =
      MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_UPDATE);

  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_FALSE(service()->IsExtensionEnabled(good_crx));
  EXPECT_TRUE(extensions::util::IsIncognitoEnabled(good_crx, profile()));

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(good_crx));
}

TEST_F(ExtensionServiceSyncTest, ProcessSyncDataVersionCheck) {
  InitializeExtensionServiceWithUpdater();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_TRUE(service()->IsExtensionEnabled(good_crx));
  EXPECT_FALSE(extensions::util::IsIncognitoEnabled(good_crx, profile()));

  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
  ext_specifics->set_id(good_crx);
  ext_specifics->set_enabled(true);

  const base::Version installed_version =
      *service()->GetInstalledExtension(good_crx)->version();

  {
    ext_specifics->set_version(installed_version.GetString());

    SyncChangeList list =
        MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_UPDATE);

    // Should do nothing if extension version == sync version.
    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_FALSE(service()->updater()->WillCheckSoon());
    // Make sure the version we'll send back to sync didn't change.
    syncer::SyncDataList data =
        extension_sync_service()->GetAllSyncData(syncer::EXTENSIONS);
    ASSERT_EQ(1u, data.size());
    scoped_ptr<ExtensionSyncData> extension_data =
        ExtensionSyncData::CreateFromSyncData(data[0]);
    ASSERT_TRUE(extension_data);
    EXPECT_EQ(installed_version, extension_data->version());
  }

  // Should do nothing if extension version > sync version.
  {
    ext_specifics->set_version("0.0.0.0");

    SyncChangeList list =
        MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_FALSE(service()->updater()->WillCheckSoon());
    // Make sure the version we'll send back to sync didn't change.
    syncer::SyncDataList data =
        extension_sync_service()->GetAllSyncData(syncer::EXTENSIONS);
    ASSERT_EQ(1u, data.size());
    scoped_ptr<ExtensionSyncData> extension_data =
        ExtensionSyncData::CreateFromSyncData(data[0]);
    ASSERT_TRUE(extension_data);
    EXPECT_EQ(installed_version, extension_data->version());
  }

  // Should kick off an update if extension version < sync version.
  {
    const base::Version new_version("9.9.9.9");
    ext_specifics->set_version(new_version.GetString());

    SyncChangeList list =
        MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
    EXPECT_TRUE(service()->updater()->WillCheckSoon());
    // Make sure that we'll send the NEW version back to sync, even though we
    // haven't actually updated yet. This is to prevent the data in sync from
    // flip-flopping back and forth until all clients are up to date.
    syncer::SyncDataList data =
        extension_sync_service()->GetAllSyncData(syncer::EXTENSIONS);
    ASSERT_EQ(1u, data.size());
    scoped_ptr<ExtensionSyncData> extension_data =
        ExtensionSyncData::CreateFromSyncData(data[0]);
    ASSERT_TRUE(extension_data);
    EXPECT_EQ(new_version, extension_data->version());
  }

  EXPECT_FALSE(service()->pending_extension_manager()->IsIdPending(good_crx));
}

TEST_F(ExtensionServiceSyncTest, ProcessSyncDataNotInstalled) {
  InitializeExtensionServiceWithUpdater();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
  ext_specifics->set_id(good_crx);
  ext_specifics->set_enabled(false);
  ext_specifics->set_incognito_enabled(true);
  ext_specifics->set_update_url("http://www.google.com/");
  ext_specifics->set_version("1.2.3.4");

  SyncChangeList list =
      MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_UPDATE);

  EXPECT_TRUE(service()->IsExtensionEnabled(good_crx));
  EXPECT_FALSE(extensions::util::IsIncognitoEnabled(good_crx, profile()));
  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  EXPECT_TRUE(service()->updater()->WillCheckSoon());
  EXPECT_FALSE(service()->IsExtensionEnabled(good_crx));
  EXPECT_TRUE(extensions::util::IsIncognitoEnabled(good_crx, profile()));

  const extensions::PendingExtensionInfo* info;
  EXPECT_TRUE(
      (info = service()->pending_extension_manager()->GetById(good_crx)));
  EXPECT_EQ(ext_specifics->update_url(), info->update_url().spec());
  EXPECT_TRUE(info->is_from_sync());
  EXPECT_EQ(Manifest::INTERNAL, info->install_source());
  // TODO(akalin): Figure out a way to test |info.ShouldAllowInstall()|.
}

TEST_F(ExtensionServiceSyncTest, ProcessSyncDataEnableDisable) {
  InitializeEmptyExtensionService();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  const ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  struct TestCase {
    const char* name;  // For failure output only.
    // Set of disable reasons before any Sync data comes in. If this is != 0,
    // the extension is disabled.
    int previous_disable_reasons;
    bool sync_enable;  // The enabled flag coming in from Sync.
    // The disable reason(s) coming in from Sync, or -1 for "not set".
    int sync_disable_reasons;
    // The expected set of disable reasons after processing the Sync update. The
    // extension should be disabled iff this is != 0.
    int expect_disable_reasons;
  } test_cases[] = {
    { "NopEnable", 0, true, 0, 0 },
    { "NopDisable", Extension::DISABLE_USER_ACTION, false,
      Extension::DISABLE_USER_ACTION, Extension::DISABLE_USER_ACTION },
    { "Enable", Extension::DISABLE_USER_ACTION, true, 0, 0 },
    { "Disable", 0, false, Extension::DISABLE_USER_ACTION,
      Extension::DISABLE_USER_ACTION },
    { "AddDisableReason", Extension::DISABLE_REMOTE_INSTALL, false,
      Extension::DISABLE_REMOTE_INSTALL | Extension::DISABLE_USER_ACTION,
      Extension::DISABLE_REMOTE_INSTALL | Extension::DISABLE_USER_ACTION },
    { "RemoveDisableReason",
      Extension::DISABLE_REMOTE_INSTALL | Extension::DISABLE_USER_ACTION, false,
      Extension::DISABLE_USER_ACTION, Extension::DISABLE_USER_ACTION },
    { "PreserveLocalDisableReason", Extension::DISABLE_RELOAD, true, 0,
      Extension::DISABLE_RELOAD },
    { "PreserveOnlyLocalDisableReason",
      Extension::DISABLE_USER_ACTION | Extension::DISABLE_RELOAD, true, 0,
      Extension::DISABLE_RELOAD },

    // Interaction with Chrome clients <=M44, which don't sync disable_reasons
    // at all (any existing reasons are preserved).
    { "M44Enable", Extension::DISABLE_USER_ACTION, true, -1, 0 },
    // An M44 client enables an extension that had been disabled on a new
    // client. The disable reasons are still be there, but should be ignored.
    { "M44ReEnable", Extension::DISABLE_USER_ACTION, true,
      Extension::DISABLE_USER_ACTION, 0 },
    { "M44Disable", 0, false, -1, Extension::DISABLE_USER_ACTION },
    { "M44ReDisable", 0, false, 0, Extension::DISABLE_USER_ACTION },
    { "M44AlreadyDisabledByUser", Extension::DISABLE_USER_ACTION, false, -1,
      Extension::DISABLE_USER_ACTION},
    { "M44AlreadyDisabledWithOtherReason", Extension::DISABLE_REMOTE_INSTALL,
      false, -1,
      Extension::DISABLE_REMOTE_INSTALL | Extension::DISABLE_USER_ACTION },
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);

    std::string id;
    std::string version;
    // Don't keep |extension| around longer than necessary.
    {
      const Extension* extension =
          InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
      // The extension should now be installed and enabled.
      ASSERT_TRUE(extension);
      id = extension->id();
      version = extension->VersionString();
    }
    ASSERT_TRUE(registry()->enabled_extensions().Contains(id));

    // Disable it if the test case says so.
    if (test_case.previous_disable_reasons) {
      service()->DisableExtension(id, test_case.previous_disable_reasons);
      ASSERT_TRUE(registry()->disabled_extensions().Contains(id));
    }

    // Now a sync update comes in.
    sync_pb::EntitySpecifics specifics;
    sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
    ext_specifics->set_id(id);
    ext_specifics->set_enabled(test_case.sync_enable);
    ext_specifics->set_version(version);
    if (test_case.sync_disable_reasons != -1)
      ext_specifics->set_disable_reasons(test_case.sync_disable_reasons);

    SyncChangeList list =
        MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

    // Check expectations.
    const bool expect_enabled = !test_case.expect_disable_reasons;
    EXPECT_EQ(expect_enabled, service()->IsExtensionEnabled(id));
    EXPECT_EQ(test_case.expect_disable_reasons, prefs->GetDisableReasons(id));

    // Remove the extension again, so we can install it again for the next case.
    UninstallExtension(id, false, expect_enabled ? Extension::ENABLED
                                                 : Extension::DISABLED);
  }
}

TEST_F(ExtensionServiceSyncTest, ProcessSyncDataDeferredEnable) {
  InitializeEmptyExtensionService();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  base::FilePath base_path = data_dir().AppendASCII("permissions_increase");
  base::FilePath pem_path = base_path.AppendASCII("permissions.pem");

  base::FilePath path = base_path.AppendASCII("v1");
  const Extension* extension = PackAndInstallCRX(path, pem_path, INSTALL_NEW);
  // The extension must now be installed and enabled.
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

  // Save the id, as the extension object will be destroyed during updating.
  std::string id = extension->id();

  // Update to a new version with increased permissions.
  path = base_path.AppendASCII("v2");
  PackCRXAndUpdateExtension(id, path, pem_path, DISABLED);

  // Now a sync update comes in, telling us to re-enable a *newer* version.
  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
  ext_specifics->set_id(id);
  ext_specifics->set_version("3");
  ext_specifics->set_enabled(true);
  ext_specifics->set_disable_reasons(Extension::DISABLE_NONE);

  SyncChangeList list =
      MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_UPDATE);

  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

  // Since the version didn't match, the extension should still be disabled.
  EXPECT_TRUE(registry()->disabled_extensions().Contains(id));

  // After we update to the matching version, the extension should get enabled.
  path = base_path.AppendASCII("v3");
  PackCRXAndUpdateExtension(id, path, pem_path, ENABLED);
}

TEST_F(ExtensionServiceSyncTest, ProcessSyncDataPermissionApproval) {
  // This is the update URL specified in the test extension. Setting it here is
  // necessary to make it considered syncable.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAppsGalleryUpdateURL,
      "http://localhost/autoupdate/updates.xml");

  InitializeEmptyExtensionService();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  const base::FilePath base_path =
      data_dir().AppendASCII("permissions_increase");
  const base::FilePath pem_path = base_path.AppendASCII("permissions.pem");
  const base::FilePath path_v1 = base_path.AppendASCII("v1");
  const base::FilePath path_v2 = base_path.AppendASCII("v2");

  base::ScopedTempDir crx_dir;
  ASSERT_TRUE(crx_dir.CreateUniqueTempDir());
  const base::FilePath crx_path_v1 = crx_dir.path().AppendASCII("temp1.crx");
  PackCRX(path_v1, pem_path, crx_path_v1);
  const base::FilePath crx_path_v2 = crx_dir.path().AppendASCII("temp2.crx");
  PackCRX(path_v2, pem_path, crx_path_v2);

  const std::string v1("1");
  const std::string v2("2");

  const ExtensionPrefs* prefs = ExtensionPrefs::Get(profile());

  struct TestCase {
    const char* name;  // For failure output only.
    const std::string& sync_version;  // The version coming in from Sync.
    // The disable reason(s) coming in from Sync, or -1 for "not set".
    int sync_disable_reasons;
    // The expected set of disable reasons after processing the Sync update. The
    // extension should be enabled iff this is 0.
    int expect_disable_reasons;
    // Whether the extension's permissions should be auto-granted.
    bool expect_permissions_granted;
  } test_cases[] = {
    // Sync tells us to re-enable an older version. No permissions should be
    // granted, since we can't be sure if the user actually approved the right
    // set of permissions.
    { "OldVersion", v1, 0, Extension::DISABLE_PERMISSIONS_INCREASE, false },
    // Legacy case: Sync tells us to re-enable the extension, but doesn't
    // specify disable reasons. No permissions should be granted.
    { "Legacy", v2, -1, Extension::DISABLE_PERMISSIONS_INCREASE, false },
    // Sync tells us to re-enable the extension and explicitly removes the
    // disable reasons. Now the extension should have its permissions granted.
    { "GrantPermissions", v2, 0, Extension::DISABLE_NONE, true },
  };

  for (const TestCase& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);

    std::string id;
    // Don't keep |extension| around longer than necessary (it'll be destroyed
    // during updating).
    {
      const Extension* extension = InstallCRX(crx_path_v1, INSTALL_NEW);
      // The extension should now be installed and enabled.
      ASSERT_TRUE(extension);
      ASSERT_EQ(v1, extension->VersionString());
      id = extension->id();
    }
    ASSERT_TRUE(registry()->enabled_extensions().Contains(id));

    scoped_ptr<const PermissionSet> granted_permissions_v1 =
        prefs->GetGrantedPermissions(id);

    // Update to a new version with increased permissions.
    UpdateExtension(id, crx_path_v2, DISABLED);

    // Now the extension should be disabled due to a permissions increase.
    {
      const Extension* extension =
          registry()->disabled_extensions().GetByID(id);
      ASSERT_TRUE(extension);
      ASSERT_EQ(v2, extension->VersionString());
    }
    ASSERT_TRUE(prefs->HasDisableReason(
        id, Extension::DISABLE_PERMISSIONS_INCREASE));

    // No new permissions should have been granted.
    scoped_ptr<const PermissionSet> granted_permissions_v2 =
        prefs->GetGrantedPermissions(id);
    ASSERT_EQ(*granted_permissions_v1, *granted_permissions_v2);

    // Now a sync update comes in.
    sync_pb::EntitySpecifics specifics;
    sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
    ext_specifics->set_id(id);
    ext_specifics->set_enabled(true);
    ext_specifics->set_version(test_case.sync_version);
    if (test_case.sync_disable_reasons != -1)
      ext_specifics->set_disable_reasons(test_case.sync_disable_reasons);

    SyncChangeList list =
        MakeSyncChangeList(good_crx, specifics, SyncChange::ACTION_UPDATE);

    extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

    // Check expectations.
    const bool expect_enabled = !test_case.expect_disable_reasons;
    EXPECT_EQ(expect_enabled, service()->IsExtensionEnabled(id));
    EXPECT_EQ(test_case.expect_disable_reasons, prefs->GetDisableReasons(id));
    scoped_ptr<const PermissionSet> granted_permissions =
        prefs->GetGrantedPermissions(id);
    if (test_case.expect_permissions_granted) {
      scoped_ptr<const PermissionSet> active_permissions =
          prefs->GetActivePermissions(id);
      EXPECT_EQ(*granted_permissions, *active_permissions);
    } else {
      EXPECT_EQ(*granted_permissions, *granted_permissions_v1);
    }

    // Remove the extension again, so we can install it again for the next case.
    UninstallExtension(id, false, expect_enabled ? Extension::ENABLED
                                                 : Extension::DISABLED);
  }
}

// Regression test for crbug.com/558299
TEST_F(ExtensionServiceSyncTest, DontSyncThemes) {
  InitializeEmptyExtensionService();

  // The user has enabled sync.
  ProfileSyncServiceFactory::GetForProfile(profile())->SetFirstSetupComplete();
  // Make sure ExtensionSyncService is created, so it'll be notified of changes.
  extension_sync_service();

  service()->Init();
  ASSERT_TRUE(service()->is_ready());

  syncer::FakeSyncChangeProcessor* processor =
      new syncer::FakeSyncChangeProcessor;
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS,
      syncer::SyncDataList(),
      make_scoped_ptr(processor),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock));

  processor->changes().clear();

  // Sanity check: Installing an extension should result in a sync change.
  InstallCRX(data_dir().AppendASCII("good.crx"), INSTALL_NEW);
  EXPECT_EQ(1u, processor->changes().size());

  processor->changes().clear();

  // Installing a theme should not result in a sync change (themes are handled
  // separately by ThemeSyncableService).
  InstallCRX(data_dir().AppendASCII("theme.crx"), INSTALL_NEW);
  EXPECT_TRUE(processor->changes().empty());
}

#if defined(ENABLE_SUPERVISED_USERS)

class ExtensionServiceTestSupervised : public ExtensionServiceSyncTest,
                                       public SupervisedUserService::Delegate {
 public:
  ExtensionServiceTestSupervised()
      : field_trial_list_(new base::MockEntropyProvider()) {}

  void SetUp() override {
    ExtensionServiceSyncTest::SetUp();

    // This is the update URL specified in the permissions test extension.
    // Setting it here is necessary to make the extension considered syncable.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kAppsGalleryUpdateURL,
        "http://localhost/autoupdate/updates.xml");
  }

  void TearDown() override {
    supervised_user_service()->SetDelegate(nullptr);

    ExtensionServiceSyncTest::TearDown();
  }

 protected:
  void InitNeedCustodianApprovalFieldTrial(bool enabled) {
    // Group name doesn't matter.
    base::FieldTrialList::CreateFieldTrial(
        "SupervisedUserExtensionPermissionIncrease", "group");
    std::map<std::string, std::string> params;
    params["legacy_supervised_user"] = enabled ? "true" : "false";
    params["child_account"] = enabled ? "true" : "false";
    variations::AssociateVariationParams(
        "SupervisedUserExtensionPermissionIncrease", "group", params);
  }

  void InitServices(bool profile_is_supervised) {
    ExtensionServiceInitParams params = CreateDefaultInitParams();
    params.profile_is_supervised = profile_is_supervised;
    InitializeExtensionService(params);

    supervised_user_service()->SetDelegate(this);
    supervised_user_service()->Init();
  }

  std::string InstallPermissionsTestExtension() {
    const std::string version("1");

    const Extension* extension =
        PackAndInstallCRX(dir_path(version), pem_path(), INSTALL_NEW,
                          Extension::WAS_INSTALLED_BY_CUSTODIAN);
    // The extension must now be installed and enabled.
    EXPECT_TRUE(extension);
    EXPECT_TRUE(registry()->enabled_extensions().Contains(extension->id()));
    EXPECT_EQ(version, extension->VersionString());

    return extension->id();
  }

  void UpdatePermissionsTestExtension(const std::string& id,
                                      const std::string& version,
                                      UpdateState expected_state) {
    PackCRXAndUpdateExtension(id, dir_path(version), pem_path(),
                              expected_state);
    const Extension* extension = registry()->GetInstalledExtension(id);
    ASSERT_TRUE(extension);
    // The version should have been updated.
    EXPECT_EQ(version, extension->VersionString());
  }

  SupervisedUserService* supervised_user_service() {
    return SupervisedUserServiceFactory::GetForProfile(profile());
  }

  static std::string UpdateRequestId(const std::string& extension_id,
                                     const std::string& version) {
    return SupervisedUserService::GetExtensionUpdateRequestId(
        extension_id, base::Version(version));
  }

 private:
  // This prevents the legacy supervised user init code from running.
  bool SetActive(bool active) override { return true; }

  base::FilePath base_path() const {
    return data_dir().AppendASCII("permissions_increase");
  }
  base::FilePath dir_path(const std::string& version) const {
    return base_path().AppendASCII("v" + version);
  }
  base::FilePath pem_path() const {
    return base_path().AppendASCII("permissions.pem");
  }

  base::FieldTrialList field_trial_list_;
};

class MockPermissionRequestCreator : public PermissionRequestCreator {
 public:
  MockPermissionRequestCreator() {}
  ~MockPermissionRequestCreator() override {}

  bool IsEnabled() const override { return true; }

  void CreateURLAccessRequest(const GURL& url_requested,
                              const SuccessCallback& callback) override {
    FAIL();
  }

  MOCK_METHOD2(CreateExtensionUpdateRequest,
               void(const std::string& id,
                    const SupervisedUserService::SuccessCallback& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPermissionRequestCreator);
};

TEST_F(ExtensionServiceTestSupervised, InstallOnlyAllowedByCustodian) {
  InitServices(true /* profile_is_supervised */);

  base::FilePath path1 = data_dir().AppendASCII("good.crx");
  base::FilePath path2 = data_dir().AppendASCII("good2048.crx");
  const Extension* extensions[] = {
    InstallCRX(path1, INSTALL_FAILED),
    InstallCRX(path2, INSTALL_NEW, Extension::WAS_INSTALLED_BY_CUSTODIAN)
  };

  // Only the extension with the "installed by custodian" flag should have been
  // installed and enabled.
  EXPECT_FALSE(extensions[0]);
  ASSERT_TRUE(extensions[1]);
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extensions[1]->id()));
}

TEST_F(ExtensionServiceTestSupervised, PreinstalledExtension) {
  InitServices(false /* profile_is_supervised */);

  // Install an extension.
  base::FilePath path = data_dir().AppendASCII("good.crx");
  const Extension* extension = InstallCRX(path, INSTALL_NEW);
  std::string id = extension->id();

  // Now make the profile supervised.
  profile()->AsTestingProfile()->SetSupervisedUserId(
      supervised_users::kChildAccountSUID);

  // The extension should not be enabled anymore.
  EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
}

TEST_F(ExtensionServiceTestSupervised, UpdateWithoutPermissionIncrease) {
  InitServices(true /* profile_is_supervised */);

  base::FilePath base_path = data_dir().AppendASCII("autoupdate");
  base::FilePath pem_path = base_path.AppendASCII("key.pem");

  const Extension* extension =
      PackAndInstallCRX(base_path.AppendASCII("v1"), pem_path, INSTALL_NEW,
                        Extension::WAS_INSTALLED_BY_CUSTODIAN);
  // The extension must now be installed and enabled.
  ASSERT_TRUE(extension);
  ASSERT_TRUE(registry()->enabled_extensions().Contains(extension->id()));

  // Save the id, as the extension object will be destroyed during updating.
  std::string id = extension->id();

  std::string old_version = extension->VersionString();

  // Update to a new version.
  PackCRXAndUpdateExtension(id, base_path.AppendASCII("v2"), pem_path, ENABLED);

  // The extension should still be there and enabled.
  extension = registry()->enabled_extensions().GetByID(id);
  ASSERT_TRUE(extension);
  // The version should have changed.
  EXPECT_NE(extension->VersionString(), old_version);
}

TEST_F(ExtensionServiceTestSupervised, UpdateWithPermissionIncreaseNoApproval) {
  InitNeedCustodianApprovalFieldTrial(false);

  InitServices(true /* profile_is_supervised */);

  MockPermissionRequestCreator* creator = new MockPermissionRequestCreator;
  supervised_user_service()->AddPermissionRequestCreator(
      make_scoped_ptr(creator));

  std::string id = InstallPermissionsTestExtension();

  // Update to a new version with increased permissions.
  // Since we don't require the custodian's approval, no permission request
  // should be created.
  const std::string version2("2");
  EXPECT_CALL(*creator, CreateExtensionUpdateRequest(
                            UpdateRequestId(id, version2), testing::_))
      .Times(0);
  UpdatePermissionsTestExtension(id, version2, DISABLED);
}

TEST_F(ExtensionServiceTestSupervised,
       UpdateWithPermissionIncreaseApprovalOldVersion) {
  InitNeedCustodianApprovalFieldTrial(true);

  InitServices(true /* profile_is_supervised */);

  MockPermissionRequestCreator* creator = new MockPermissionRequestCreator;
  supervised_user_service()->AddPermissionRequestCreator(
      make_scoped_ptr(creator));

  const std::string version1("1");
  const std::string version2("2");

  std::string id = InstallPermissionsTestExtension();

  // Update to a new version with increased permissions.
  EXPECT_CALL(*creator, CreateExtensionUpdateRequest(
                            UpdateRequestId(id, version2), testing::_));
  UpdatePermissionsTestExtension(id, version2, DISABLED);

  // Simulate a custodian approval for re-enabling the extension coming in
  // through Sync, but set the old version. This can happen when there already
  // was a pending request for an earlier version of the extension.
  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
  ext_specifics->set_id(id);
  ext_specifics->set_enabled(true);
  ext_specifics->set_disable_reasons(Extension::DISABLE_NONE);
  ext_specifics->set_installed_by_custodian(true);
  ext_specifics->set_version(version1);

  // Attempting to re-enable an old version should result in a permission
  // request for the current version.
  EXPECT_CALL(*creator, CreateExtensionUpdateRequest(
                            UpdateRequestId(id, version2), testing::_));

  SyncChangeList list =
      MakeSyncChangeList(id, specifics, SyncChange::ACTION_UPDATE);

  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  // The re-enable should be ignored, since the version doesn't match.
  EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
  EXPECT_FALSE(extension_sync_service()->HasPendingReenable(
      id, base::Version(version1)));
  EXPECT_FALSE(extension_sync_service()->HasPendingReenable(
      id, base::Version(version2)));
}

TEST_F(ExtensionServiceTestSupervised,
       UpdateWithPermissionIncreaseApprovalMatchingVersion) {
  InitNeedCustodianApprovalFieldTrial(true);

  InitServices(true /* profile_is_supervised */);

  MockPermissionRequestCreator* creator = new MockPermissionRequestCreator;
  supervised_user_service()->AddPermissionRequestCreator(
      make_scoped_ptr(creator));

  std::string id = InstallPermissionsTestExtension();

  // Update to a new version with increased permissions.
  const std::string version2("2");
  EXPECT_CALL(*creator, CreateExtensionUpdateRequest(
                            UpdateRequestId(id, version2), testing::_));
  UpdatePermissionsTestExtension(id, version2, DISABLED);

  // Simulate a custodian approval for re-enabling the extension coming in
  // through Sync.
  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
  ext_specifics->set_id(id);
  ext_specifics->set_enabled(true);
  ext_specifics->set_disable_reasons(Extension::DISABLE_NONE);
  ext_specifics->set_installed_by_custodian(true);
  ext_specifics->set_version(version2);

  SyncChangeList list =
      MakeSyncChangeList(id, specifics, SyncChange::ACTION_UPDATE);

  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  // The extension should have gotten re-enabled.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(id));
}

TEST_F(ExtensionServiceTestSupervised,
       UpdateWithPermissionIncreaseApprovalNewVersion) {
  InitNeedCustodianApprovalFieldTrial(true);

  InitServices(true /* profile_is_supervised */);

  MockPermissionRequestCreator* creator = new MockPermissionRequestCreator;
  supervised_user_service()->AddPermissionRequestCreator(
      make_scoped_ptr(creator));

  std::string id = InstallPermissionsTestExtension();

  // Update to a new version with increased permissions.
  const std::string version2("2");
  EXPECT_CALL(*creator, CreateExtensionUpdateRequest(
                            UpdateRequestId(id, version2), testing::_));
  UpdatePermissionsTestExtension(id, version2, DISABLED);

  // Simulate a custodian approval for re-enabling the extension coming in
  // through Sync. Set a newer version than we have installed.
  const std::string version3("3");
  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
  ext_specifics->set_id(id);
  ext_specifics->set_enabled(true);
  ext_specifics->set_disable_reasons(Extension::DISABLE_NONE);
  ext_specifics->set_installed_by_custodian(true);
  ext_specifics->set_version(version3);

  // This should *not* result in a new permission request.
  EXPECT_CALL(*creator, CreateExtensionUpdateRequest(
                            UpdateRequestId(id, version3), testing::_))
      .Times(0);

  SyncChangeList list =
      MakeSyncChangeList(id, specifics, SyncChange::ACTION_UPDATE);

  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);
  // The re-enable should be delayed until the extension is updated to the
  // matching version.
  EXPECT_FALSE(registry()->enabled_extensions().Contains(id));
  EXPECT_TRUE(extension_sync_service()->HasPendingReenable(
      id, base::Version(version3)));

  // Update to the matching version. Now the extension should get enabled.
  UpdatePermissionsTestExtension(id, version3, ENABLED);
}

TEST_F(ExtensionServiceSyncTest, SyncUninstallByCustodianSkipsPolicy) {
  InitializeEmptyExtensionService();
  extension_sync_service()->MergeDataAndStartSyncing(
      syncer::EXTENSIONS, syncer::SyncDataList(),
      make_scoped_ptr(new syncer::FakeSyncChangeProcessor()),
      make_scoped_ptr(new syncer::SyncErrorFactoryMock()));

  // Install two extensions.
  base::FilePath path1 = data_dir().AppendASCII("good.crx");
  base::FilePath path2 = data_dir().AppendASCII("good2048.crx");
  const Extension* extensions[] = {
    InstallCRX(path1, INSTALL_NEW),
    InstallCRX(path2, INSTALL_NEW, Extension::WAS_INSTALLED_BY_CUSTODIAN)
  };

  // Add a policy provider that will disallow any changes.
  extensions::TestManagementPolicyProvider provider(
      extensions::TestManagementPolicyProvider::PROHIBIT_MODIFY_STATUS);
  ExtensionSystem::Get(
      browser_context())->management_policy()->RegisterProvider(&provider);

  // Create a sync deletion for each extension.
  SyncChangeList list;
  for (size_t i = 0; i < arraysize(extensions); i++) {
    const std::string& id = extensions[i]->id();
    sync_pb::EntitySpecifics specifics;
    sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
    ext_specifics->set_id(id);
    ext_specifics->set_version("1.0");
    ext_specifics->set_installed_by_custodian(
        extensions[i]->was_installed_by_custodian());
    syncer::SyncData sync_data =
        syncer::SyncData::CreateLocalData(id, "Name", specifics);
    list.push_back(SyncChange(FROM_HERE, SyncChange::ACTION_DELETE, sync_data));
  }

  // Save the extension ids, as uninstalling destroys the Extension instance.
  std::string extension_ids[] = {
    extensions[0]->id(),
    extensions[1]->id()
  };

  // Now apply the uninstallations.
  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

  // Uninstalling the extension without installed_by_custodian should have been
  // blocked by policy, so it should still be there.
  EXPECT_TRUE(registry()->enabled_extensions().Contains(extension_ids[0]));

  // But installed_by_custodian should result in bypassing the policy check.
  EXPECT_FALSE(
      registry()->GenerateInstalledExtensionsSet()->Contains(extension_ids[1]));
}

TEST_F(ExtensionServiceSyncTest, SyncExtensionHasAllhostsWithheld) {
  InitializeEmptyExtensionService();

  // Create an extension that needs all-hosts.
  const std::string kName("extension");
  scoped_refptr<const Extension> extension =
      extensions::ExtensionBuilder()
          .SetLocation(Manifest::INTERNAL)
          .SetManifest(std::move(
              extensions::DictionaryBuilder()
                  .Set("name", kName)
                  .Set("description", "foo")
                  .Set("manifest_version", 2)
                  .Set("version", "1.0")
                  .Set("permissions",
                       std::move(extensions::ListBuilder().Append("*://*/*")))))
          .SetID(crx_file::id_util::GenerateId(kName))
          .Build();

  // Install and enable it.
  service()->AddExtension(extension.get());
  service()->GrantPermissionsAndEnableExtension(extension.get());
  const std::string id = extension->id();
  EXPECT_TRUE(registry()->enabled_extensions().GetByID(id));

  // Simulate a sync node coming in where the extension had all-hosts withheld.
  // This means that it should have all-hosts withheld on this machine, too.
  sync_pb::EntitySpecifics specifics;
  sync_pb::ExtensionSpecifics* ext_specifics = specifics.mutable_extension();
  ext_specifics->set_id(id);
  ext_specifics->set_name(kName);
  ext_specifics->set_version("1.0");
  ext_specifics->set_all_urls_enabled(false);
  ext_specifics->set_enabled(true);

  SyncChangeList list =
      MakeSyncChangeList(id, specifics, SyncChange::ACTION_UPDATE);

  extension_sync_service()->ProcessSyncChanges(FROM_HERE, list);

  EXPECT_TRUE(registry()->enabled_extensions().GetByID(id));
  EXPECT_FALSE(extensions::util::AllowedScriptingOnAllUrls(id, profile()));
  EXPECT_TRUE(extensions::util::HasSetAllowedScriptingOnAllUrls(id, profile()));
  EXPECT_FALSE(extensions::util::AllowedScriptingOnAllUrls(id, profile()));
}

#endif  // defined(ENABLE_SUPERVISED_USERS)
