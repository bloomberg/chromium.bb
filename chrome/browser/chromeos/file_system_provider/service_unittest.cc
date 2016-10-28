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
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/registry_interface.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/common/extensions/api/file_system_provider_capabilities/file_system_provider_capabilities_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
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

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kDisplayName[] = "Camera Pictures";

// The dot in the file system ID is there in order to check that saving to
// preferences works correctly. File System ID is used as a key in
// a base::DictionaryValue, so it has to be stored without path expansion.
const char kFileSystemId[] = "camera/pictures/id .!@#$%^&*()_+";

// Utility observer, logging events from file_system_provider::Service.
class LoggingObserver : public Observer {
 public:
  class Event {
   public:
    Event(const ProvidedFileSystemInfo& file_system_info,
          MountContext context,
          base::File::Error error)
        : file_system_info_(file_system_info),
          context_(context),
          error_(error) {}
    ~Event() {}

    const ProvidedFileSystemInfo& file_system_info() const {
      return file_system_info_;
    }
    MountContext context() const { return context_; }
    base::File::Error error() const { return error_; }

   private:
    ProvidedFileSystemInfo file_system_info_;
    MountContext context_;
    base::File::Error error_;
  };

  LoggingObserver() {}
  ~LoggingObserver() override {}

  // file_system_provider::Observer overrides.
  void OnProvidedFileSystemMount(const ProvidedFileSystemInfo& file_system_info,
                                 MountContext context,
                                 base::File::Error error) override {
    mounts.push_back(Event(file_system_info, context, error));
  }

  void OnProvidedFileSystemUnmount(
      const ProvidedFileSystemInfo& file_system_info,
      base::File::Error error) override {
    // TODO(mtomasz): Split these events, as mount context doesn't make sense
    // for unmounting.
    unmounts.push_back(Event(file_system_info, MOUNT_CONTEXT_USER, error));
  }

  std::vector<Event> mounts;
  std::vector<Event> unmounts;

  DISALLOW_COPY_AND_ASSIGN(LoggingObserver);
};

// Fake implementation of the registry, since it's already tested separately.
// For simplicity it can remember at most only one file system.
class FakeRegistry : public RegistryInterface {
 public:
  FakeRegistry() {}
  ~FakeRegistry() override {}

  // RegistryInterface overrides.
  void RememberFileSystem(const ProvidedFileSystemInfo& file_system_info,
                          const Watchers& watchers) override {
    file_system_info_.reset(new ProvidedFileSystemInfo(file_system_info));
    watchers_.reset(new Watchers(watchers));
  }

  void ForgetFileSystem(const std::string& extension_id,
                        const std::string& file_system_id) override {
    if (!file_system_info_.get() || !watchers_.get())
      return;
    if (file_system_info_->extension_id() == extension_id &&
        file_system_info_->file_system_id() == file_system_id) {
      file_system_info_.reset();
      watchers_.reset();
    }
  }

  std::unique_ptr<RestoredFileSystems> RestoreFileSystems(
      const std::string& extension_id) override {
    std::unique_ptr<RestoredFileSystems> result(new RestoredFileSystems);

    if (file_system_info_.get() && watchers_.get()) {
      RestoredFileSystem restored_file_system;
      restored_file_system.extension_id = file_system_info_->extension_id();

      MountOptions options;
      options.file_system_id = file_system_info_->file_system_id();
      options.display_name = file_system_info_->display_name();
      options.writable = file_system_info_->writable();
      options.supports_notify_tag = file_system_info_->supports_notify_tag();
      restored_file_system.options = options;
      restored_file_system.watchers = *watchers_.get();

      result->push_back(restored_file_system);
    }

    return result;
  }

  void UpdateWatcherTag(const ProvidedFileSystemInfo& file_system_info,
                        const Watcher& watcher) override {
    ASSERT_TRUE(watchers_.get());
    const Watchers::iterator it =
        watchers_->find(WatcherKey(watcher.entry_path, watcher.recursive));
    ASSERT_NE(watchers_->end(), it);
    it->second.last_tag = watcher.last_tag;
  }

  const ProvidedFileSystemInfo* file_system_info() const {
    return file_system_info_.get();
  }
  const Watchers* watchers() const { return watchers_.get(); }

 private:
  std::unique_ptr<ProvidedFileSystemInfo> file_system_info_;
  std::unique_ptr<Watchers> watchers_;

  DISALLOW_COPY_AND_ASSIGN(FakeRegistry);
};

// Creates a fake extension with the specified |extension_id|.
scoped_refptr<extensions::Extension> CreateFakeExtension(
    const std::string& extension_id) {
  base::DictionaryValue manifest;
  std::string error;
  manifest.SetStringWithoutPathExpansion(extensions::manifest_keys::kVersion,
                                         "1.0.0.0");
  manifest.SetStringWithoutPathExpansion(extensions::manifest_keys::kName,
                                         "unused");
  return extensions::Extension::Create(base::FilePath(),
                                       extensions::Manifest::UNPACKED,
                                       manifest,
                                       extensions::Extension::NO_FLAGS,
                                       extension_id,
                                       &error);
}

}  // namespace

class FileSystemProviderServiceTest : public testing::Test {
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
    user_manager_enabler_.reset(new ScopedUserManagerEnabler(user_manager_));
    extension_registry_.reset(new extensions::ExtensionRegistry(profile_));

    service_.reset(new Service(profile_, extension_registry_.get()));
    service_->SetFileSystemFactoryForTesting(
        base::Bind(&FakeProvidedFileSystem::Create));
    extension_ = CreateFakeExtension(kExtensionId);

    registry_ = new FakeRegistry;
    // Passes ownership to the service instance.
    service_->SetRegistryForTesting(base::WrapUnique(registry_));

    fake_watcher_.entry_path = base::FilePath(FILE_PATH_LITERAL("/a/b/c"));
    fake_watcher_.recursive = true;
    fake_watcher_.last_tag = "hello-world";
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  FakeChromeUserManager* user_manager_;
  std::unique_ptr<ScopedUserManagerEnabler> user_manager_enabler_;
  std::unique_ptr<extensions::ExtensionRegistry> extension_registry_;
  std::unique_ptr<Service> service_;
  scoped_refptr<extensions::Extension> extension_;
  FakeRegistry* registry_;  // Owned by Service.
  Watcher fake_watcher_;
};

TEST_F(FileSystemProviderServiceTest, MountFileSystem) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  EXPECT_EQ(base::File::FILE_OK,
            service_->MountFileSystem(
                kExtensionId, MountOptions(kFileSystemId, kDisplayName)));

  ASSERT_EQ(1u, observer.mounts.size());
  EXPECT_EQ(kExtensionId, observer.mounts[0].file_system_info().extension_id());
  EXPECT_EQ(kFileSystemId,
            observer.mounts[0].file_system_info().file_system_id());
  base::FilePath expected_mount_path =
      util::GetMountPath(profile_, kExtensionId, kFileSystemId);
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
            service_->MountFileSystem(kExtensionId, options));

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
                kExtensionId, MountOptions(kFileSystemId, kDisplayName)));
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS,
            service_->MountFileSystem(
                kExtensionId, MountOptions(kFileSystemId, kDisplayName)));

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
                  kExtensionId, MountOptions(file_system_id, kDisplayName)));
  }
  ASSERT_EQ(kMaxFileSystems, observer.mounts.size());

  // The next file system is out of limit, and registering it should fail.
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED,
            service_->MountFileSystem(
                kExtensionId, MountOptions(kFileSystemId, kDisplayName)));

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
                kExtensionId, MountOptions(kFileSystemId, kDisplayName)));
  ASSERT_EQ(1u, observer.mounts.size());

  EXPECT_EQ(base::File::FILE_OK,
            service_->UnmountFileSystem(kExtensionId, kFileSystemId,
                                        Service::UNMOUNT_REASON_USER));
  ASSERT_EQ(1u, observer.unmounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.unmounts[0].error());

  EXPECT_EQ(kExtensionId,
            observer.unmounts[0].file_system_info().extension_id());
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
                kExtensionId, MountOptions(kFileSystemId, kDisplayName)));
  ASSERT_EQ(1u, observer.mounts.size());

  // Directly call the observer's method.
  service_->OnExtensionUnloaded(
      profile_,
      extension_.get(),
      extensions::UnloadedExtensionInfo::REASON_DISABLE);

  ASSERT_EQ(1u, observer.unmounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.unmounts[0].error());

  EXPECT_EQ(kExtensionId,
            observer.unmounts[0].file_system_info().extension_id());
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
                kExtensionId, MountOptions(kFileSystemId, kDisplayName)));
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
      kExtensionId, options, base::FilePath(FILE_PATH_LITERAL("/a/b/c")),
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

  EXPECT_EQ(file_system_info.extension_id(),
            observer.mounts[0].file_system_info().extension_id());
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
      service_->GetProvidedFileSystem(kExtensionId, kFileSystemId);
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
                kExtensionId, MountOptions(kFileSystemId, kDisplayName)));
  ASSERT_EQ(1u, observer.mounts.size());

  ASSERT_TRUE(registry_->file_system_info());
  EXPECT_EQ(kExtensionId, registry_->file_system_info()->extension_id());
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
                  kExtensionId, MountOptions(kFileSystemId, kDisplayName)));

    EXPECT_EQ(1u, observer.mounts.size());
    EXPECT_TRUE(registry_->file_system_info());
    EXPECT_TRUE(registry_->watchers());
  }

  {
    EXPECT_EQ(base::File::FILE_OK,
              service_->UnmountFileSystem(kExtensionId, kFileSystemId,
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
                  kExtensionId, MountOptions(kFileSystemId, kDisplayName)));

    EXPECT_EQ(1u, observer.mounts.size());
    EXPECT_TRUE(registry_->file_system_info());
    EXPECT_TRUE(registry_->watchers());
  }

  {
    EXPECT_EQ(base::File::FILE_OK,
              service_->UnmountFileSystem(kExtensionId, kFileSystemId,
                                          Service::UNMOUNT_REASON_USER));

    EXPECT_EQ(1u, observer.unmounts.size());
    EXPECT_FALSE(registry_->file_system_info());
    EXPECT_FALSE(registry_->watchers());
  }

  service_->RemoveObserver(&observer);
}

}  // namespace file_system_provider
}  // namespace chromeos
