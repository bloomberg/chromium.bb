// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/service.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/fake_registry.h"
#include "chrome/browser/chromeos/file_system_provider/logging_observer.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/registry_interface.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace {

const char kProviderId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kDisplayName[] = "Camera Pictures";
const char kCustomProviderId[] = "custom_provider_id";

// The dot in the file system ID is there in order to check that saving to
// preferences works correctly. File System ID is used as a key in
// a base::DictionaryValue, so it has to be stored without path expansion.
const char kFileSystemId[] = "camera/pictures/id .!@#$%^&*()_+";

// Creates a fake extension with the specified |extension_id|.
scoped_refptr<extensions::Extension> CreateFakeExtension(
    const std::string& extension_id) {
  base::DictionaryValue manifest;
  std::string error;
  manifest.SetKey(extensions::manifest_keys::kVersion, base::Value("1.0.0.0"));
  manifest.SetKey(extensions::manifest_keys::kName, base::Value("unused"));
  return extensions::Extension::Create(base::FilePath(),
                                       extensions::Manifest::UNPACKED,
                                       manifest,
                                       extensions::Extension::NO_FLAGS,
                                       extension_id,
                                       &error);
}

}  // namespace

class FileSystemProviderServiceTest : public testing::Test {
 public:
  std::unique_ptr<ProvidedFileSystemInterface> CreateDefaultFakeFileSystem(
      Profile* profile,
      const ProvidedFileSystemInfo& file_system_info) {
    called_default_factory_ = true;
    return base::MakeUnique<FakeProvidedFileSystem>(file_system_info);
  }

  std::unique_ptr<ProvidedFileSystemInterface> CreateCustomFakeFileSystem(
      Profile* profile,
      const ProvidedFileSystemInfo& file_system_info) {
    called_custom_factory_ = true;
    return base::MakeUnique<FakeProvidedFileSystem>(file_system_info);
  }

 protected:
  FileSystemProviderServiceTest() : profile_(NULL) {}

  ~FileSystemProviderServiceTest() override {}

  void SetUp() override {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test-user@example.com");
    user_manager_ = new FakeChromeUserManager();
    user_manager_->AddUser(
        AccountId::FromUserEmail(profile_->GetProfileUserName()));
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(user_manager_));
    extension_registry_.reset(new extensions::ExtensionRegistry(profile_));

    service_.reset(new Service(profile_, extension_registry_.get()));
    service_->SetDefaultFileSystemFactoryForTesting(
        base::Bind(&FakeProvidedFileSystem::Create));
    extension_ = CreateFakeExtension(kProviderId);

    registry_ = new FakeRegistry;
    // Passes ownership to the service instance.
    service_->SetRegistryForTesting(base::WrapUnique(registry_));

    fake_watcher_.entry_path = base::FilePath(FILE_PATH_LITERAL("/a/b/c"));
    fake_watcher_.recursive = true;
    fake_watcher_.last_tag = "hello-world";
    called_default_factory_ = false;
    called_custom_factory_ = false;
  }

  void TearDown() override {
    service_->Shutdown();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  FakeChromeUserManager* user_manager_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
  std::unique_ptr<extensions::ExtensionRegistry> extension_registry_;
  std::unique_ptr<Service> service_;
  scoped_refptr<extensions::Extension> extension_;
  FakeRegistry* registry_;  // Owned by Service.
  Watcher fake_watcher_;
  bool called_default_factory_;
  bool called_custom_factory_;
};

TEST_F(FileSystemProviderServiceTest, RegisterFileSystemFactory) {
  service_->RegisterFileSystemFactory(
      kCustomProviderId,
      base::Bind(&FileSystemProviderServiceTest::CreateCustomFakeFileSystem,
                 base::Unretained(this)));
  service_->SetDefaultFileSystemFactoryForTesting(
      base::Bind(&FileSystemProviderServiceTest::CreateDefaultFakeFileSystem,
                 base::Unretained(this)));

  EXPECT_FALSE(FileSystemProviderServiceTest::called_default_factory_);

  EXPECT_EQ(base::File::FILE_OK,
            service_->MountFileSystem(
                kProviderId, MountOptions(kFileSystemId, kDisplayName)));

  EXPECT_TRUE(FileSystemProviderServiceTest::called_default_factory_);

  EXPECT_FALSE(FileSystemProviderServiceTest::called_custom_factory_);

  EXPECT_EQ(base::File::FILE_OK,
            service_->MountFileSystem(
                kCustomProviderId, MountOptions(kFileSystemId, kDisplayName)));

  EXPECT_TRUE(FileSystemProviderServiceTest::called_custom_factory_);
}

TEST_F(FileSystemProviderServiceTest, MountFileSystem) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  EXPECT_EQ(base::File::FILE_OK,
            service_->MountFileSystem(
                kProviderId, MountOptions(kFileSystemId, kDisplayName)));

  ASSERT_EQ(1u, observer.mounts.size());
  EXPECT_EQ(kProviderId, observer.mounts[0].file_system_info().provider_id());
  EXPECT_EQ(kFileSystemId,
            observer.mounts[0].file_system_info().file_system_id());
  base::FilePath expected_mount_path =
      util::GetMountPath(profile_, kProviderId, kFileSystemId);
  EXPECT_EQ(expected_mount_path.AsUTF8Unsafe(),
            observer.mounts[0].file_system_info().mount_path().AsUTF8Unsafe());
  EXPECT_EQ(kDisplayName, observer.mounts[0].file_system_info().display_name());
  EXPECT_FALSE(observer.mounts[0].file_system_info().writable());
  EXPECT_FALSE(observer.mounts[0].file_system_info().supports_notify_tag());
  EXPECT_EQ(base::File::FILE_OK, observer.mounts[0].error());
  EXPECT_EQ(MOUNT_CONTEXT_USER, observer.mounts[0].context());
  ASSERT_EQ(0u, observer.unmounts.size());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(1u, file_system_info_list.size());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest,
       MountFileSystem_WritableAndSupportsNotifyTag) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  MountOptions options(kFileSystemId, kDisplayName);
  options.writable = true;
  options.supports_notify_tag = true;
  EXPECT_EQ(base::File::FILE_OK,
            service_->MountFileSystem(kProviderId, options));

  ASSERT_EQ(1u, observer.mounts.size());
  EXPECT_TRUE(observer.mounts[0].file_system_info().writable());
  EXPECT_TRUE(observer.mounts[0].file_system_info().supports_notify_tag());
  ASSERT_EQ(0u, observer.unmounts.size());
  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(1u, file_system_info_list.size());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, MountFileSystem_UniqueIds) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  EXPECT_EQ(base::File::FILE_OK,
            service_->MountFileSystem(
                kProviderId, MountOptions(kFileSystemId, kDisplayName)));
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS,
            service_->MountFileSystem(
                kProviderId, MountOptions(kFileSystemId, kDisplayName)));

  ASSERT_EQ(2u, observer.mounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.mounts[0].error());
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS, observer.mounts[1].error());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(1u, file_system_info_list.size());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, MountFileSystem_StressTest) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  const size_t kMaxFileSystems = 16;
  for (size_t i = 0; i < kMaxFileSystems; ++i) {
    const std::string file_system_id =
        std::string("test-") + base::IntToString(i);
    EXPECT_EQ(base::File::FILE_OK,
              service_->MountFileSystem(
                  kProviderId, MountOptions(file_system_id, kDisplayName)));
  }
  ASSERT_EQ(kMaxFileSystems, observer.mounts.size());

  // The next file system is out of limit, and registering it should fail.
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED,
            service_->MountFileSystem(
                kProviderId, MountOptions(kFileSystemId, kDisplayName)));

  ASSERT_EQ(kMaxFileSystems + 1, observer.mounts.size());
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED,
            observer.mounts[kMaxFileSystems].error());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(kMaxFileSystems, file_system_info_list.size());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, UnmountFileSystem) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  EXPECT_EQ(base::File::FILE_OK,
            service_->MountFileSystem(
                kProviderId, MountOptions(kFileSystemId, kDisplayName)));
  ASSERT_EQ(1u, observer.mounts.size());

  EXPECT_EQ(base::File::FILE_OK,
            service_->UnmountFileSystem(kProviderId, kFileSystemId,
                                        Service::UNMOUNT_REASON_USER));
  ASSERT_EQ(1u, observer.unmounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.unmounts[0].error());

  EXPECT_EQ(kProviderId, observer.unmounts[0].file_system_info().provider_id());
  EXPECT_EQ(kFileSystemId,
            observer.unmounts[0].file_system_info().file_system_id());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(0u, file_system_info_list.size());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, UnmountFileSystem_OnExtensionUnload) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  EXPECT_EQ(base::File::FILE_OK,
            service_->MountFileSystem(
                kProviderId, MountOptions(kFileSystemId, kDisplayName)));
  ASSERT_EQ(1u, observer.mounts.size());

  // Directly call the observer's method.
  service_->OnExtensionUnloaded(profile_, extension_.get(),
                                extensions::UnloadedExtensionReason::DISABLE);

  ASSERT_EQ(1u, observer.unmounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.unmounts[0].error());

  EXPECT_EQ(kProviderId, observer.unmounts[0].file_system_info().provider_id());
  EXPECT_EQ(kFileSystemId,
            observer.unmounts[0].file_system_info().file_system_id());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(0u, file_system_info_list.size());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, UnmountFileSystem_WrongExtensionId) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  const std::string kWrongExtensionId = "helloworldhelloworldhelloworldhe";

  EXPECT_EQ(base::File::FILE_OK,
            service_->MountFileSystem(
                kProviderId, MountOptions(kFileSystemId, kDisplayName)));
  ASSERT_EQ(1u, observer.mounts.size());
  ASSERT_EQ(1u, service_->GetProvidedFileSystemInfoList().size());

  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND,
            service_->UnmountFileSystem(kWrongExtensionId, kFileSystemId,
                                        Service::UNMOUNT_REASON_USER));
  ASSERT_EQ(1u, observer.unmounts.size());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, observer.unmounts[0].error());
  ASSERT_EQ(1u, service_->GetProvidedFileSystemInfoList().size());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(1u, file_system_info_list.size());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, RestoreFileSystem_OnExtensionLoad) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  // Remember a fake file system first in order to be able to restore it.
  MountOptions options(kFileSystemId, kDisplayName);
  options.supports_notify_tag = true;
  ProvidedFileSystemInfo file_system_info(
      kProviderId, options, base::FilePath(FILE_PATH_LITERAL("/a/b/c")),
      false /* configurable */, false /* watchable */, extensions::SOURCE_FILE);
  Watchers fake_watchers;
  fake_watchers[WatcherKey(fake_watcher_.entry_path, fake_watcher_.recursive)] =
      fake_watcher_;
  registry_->RememberFileSystem(file_system_info, fake_watchers);

  EXPECT_EQ(0u, observer.mounts.size());

  // Directly call the observer's method.
  service_->OnExtensionLoaded(profile_, extension_.get());

  ASSERT_EQ(1u, observer.mounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.mounts[0].error());
  EXPECT_EQ(MOUNT_CONTEXT_RESTORE, observer.mounts[0].context());

  EXPECT_EQ(file_system_info.provider_id(),
            observer.mounts[0].file_system_info().provider_id());
  EXPECT_EQ(file_system_info.file_system_id(),
            observer.mounts[0].file_system_info().file_system_id());
  EXPECT_EQ(file_system_info.writable(),
            observer.mounts[0].file_system_info().watchable());
  EXPECT_EQ(file_system_info.supports_notify_tag(),
            observer.mounts[0].file_system_info().supports_notify_tag());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(1u, file_system_info_list.size());

  ProvidedFileSystemInterface* const file_system =
      service_->GetProvidedFileSystem(kProviderId, kFileSystemId);
  ASSERT_TRUE(file_system);

  const Watchers* const watchers = file_system->GetWatchers();
  ASSERT_TRUE(watchers);
  ASSERT_EQ(1u, watchers->size());

  const Watchers::const_iterator restored_watcher_it = watchers->find(
      WatcherKey(fake_watcher_.entry_path, fake_watcher_.recursive));
  ASSERT_NE(watchers->end(), restored_watcher_it);

  EXPECT_EQ(fake_watcher_.entry_path, restored_watcher_it->second.entry_path);
  EXPECT_EQ(fake_watcher_.recursive, restored_watcher_it->second.recursive);
  EXPECT_EQ(fake_watcher_.last_tag, restored_watcher_it->second.last_tag);

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, RememberFileSystem_OnMount) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  EXPECT_FALSE(registry_->file_system_info());
  EXPECT_FALSE(registry_->watchers());

  EXPECT_EQ(base::File::FILE_OK,
            service_->MountFileSystem(
                kProviderId, MountOptions(kFileSystemId, kDisplayName)));
  ASSERT_EQ(1u, observer.mounts.size());

  ASSERT_TRUE(registry_->file_system_info());
  EXPECT_EQ(kProviderId, registry_->file_system_info()->provider_id());
  EXPECT_EQ(kFileSystemId, registry_->file_system_info()->file_system_id());
  EXPECT_EQ(kDisplayName, registry_->file_system_info()->display_name());
  EXPECT_FALSE(registry_->file_system_info()->writable());
  EXPECT_FALSE(registry_->file_system_info()->configurable());
  EXPECT_FALSE(registry_->file_system_info()->watchable());
  EXPECT_FALSE(registry_->file_system_info()->supports_notify_tag());
  ASSERT_TRUE(registry_->watchers());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, RememberFileSystem_OnUnmountOnShutdown) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  {
    EXPECT_FALSE(registry_->file_system_info());
    EXPECT_FALSE(registry_->watchers());
    EXPECT_EQ(base::File::FILE_OK,
              service_->MountFileSystem(
                  kProviderId, MountOptions(kFileSystemId, kDisplayName)));

    EXPECT_EQ(1u, observer.mounts.size());
    EXPECT_TRUE(registry_->file_system_info());
    EXPECT_TRUE(registry_->watchers());
  }

  {
    EXPECT_EQ(base::File::FILE_OK,
              service_->UnmountFileSystem(kProviderId, kFileSystemId,
                                          Service::UNMOUNT_REASON_SHUTDOWN));

    EXPECT_EQ(1u, observer.unmounts.size());
    EXPECT_TRUE(registry_->file_system_info());
    EXPECT_TRUE(registry_->watchers());
  }

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, RememberFileSystem_OnUnmountByUser) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  {
    EXPECT_FALSE(registry_->file_system_info());
    EXPECT_FALSE(registry_->watchers());
    EXPECT_EQ(base::File::FILE_OK,
              service_->MountFileSystem(
                  kProviderId, MountOptions(kFileSystemId, kDisplayName)));

    EXPECT_EQ(1u, observer.mounts.size());
    EXPECT_TRUE(registry_->file_system_info());
    EXPECT_TRUE(registry_->watchers());
  }

  {
    EXPECT_EQ(base::File::FILE_OK,
              service_->UnmountFileSystem(kProviderId, kFileSystemId,
                                          Service::UNMOUNT_REASON_USER));

    EXPECT_EQ(1u, observer.unmounts.size());
    EXPECT_FALSE(registry_->file_system_info());
    EXPECT_FALSE(registry_->watchers());
  }

  service_->RemoveObserver(&observer);
}

}  // namespace file_system_provider
}  // namespace chromeos
