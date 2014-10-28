// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/service.h"

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/registry_interface.h"
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
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
          base::File::Error error)
        : file_system_info_(file_system_info), error_(error) {}
    ~Event() {}

    const ProvidedFileSystemInfo& file_system_info() {
      return file_system_info_;
    }
    base::File::Error error() { return error_; }

   private:
    ProvidedFileSystemInfo file_system_info_;
    base::File::Error error_;
  };

  LoggingObserver() {}
  virtual ~LoggingObserver() {}

  // file_system_provider::Observer overrides.
  virtual void OnProvidedFileSystemMount(
      const ProvidedFileSystemInfo& file_system_info,
      base::File::Error error) override {
    mounts.push_back(Event(file_system_info, error));
  }

  virtual void OnProvidedFileSystemUnmount(
      const ProvidedFileSystemInfo& file_system_info,
      base::File::Error error) override {
    unmounts.push_back(Event(file_system_info, error));
  }

  std::vector<Event> mounts;
  std::vector<Event> unmounts;
};

// Fake implementation of the registry, since it's already tested separately.
// For simplicity it can remember at most only one file system.
class FakeRegistry : public RegistryInterface {
 public:
  FakeRegistry() {}
  virtual ~FakeRegistry() {}

  // RegistryInterface overrides.
  virtual void RememberFileSystem(
      const ProvidedFileSystemInfo& file_system_info,
      const ObservedEntries& observed_entries) override {
    file_system_info_.reset(new ProvidedFileSystemInfo(file_system_info));
    observed_entries_.reset(new ObservedEntries(observed_entries));
  }

  virtual void ForgetFileSystem(const std::string& extension_id,
                                const std::string& file_system_id) override {
    if (!file_system_info_.get() || !observed_entries_.get())
      return;
    if (file_system_info_->extension_id() == extension_id &&
        file_system_info_->file_system_id() == file_system_id) {
      file_system_info_.reset();
      observed_entries_.reset();
    }
  }

  virtual scoped_ptr<RestoredFileSystems> RestoreFileSystems(
      const std::string& extension_id) override {
    scoped_ptr<RestoredFileSystems> result(new RestoredFileSystems);

    if (file_system_info_.get() && observed_entries_.get()) {
      RestoredFileSystem restored_file_system;
      restored_file_system.extension_id = file_system_info_->extension_id();

      MountOptions options;
      options.file_system_id = file_system_info_->file_system_id();
      options.display_name = file_system_info_->display_name();
      options.writable = file_system_info_->writable();
      options.supports_notify_tag = file_system_info_->supports_notify_tag();
      restored_file_system.options = options;
      restored_file_system.observed_entries = *observed_entries_.get();

      result->push_back(restored_file_system);
    }

    return result;
  }

  virtual void UpdateObservedEntryTag(
      const ProvidedFileSystemInfo& file_system_info,
      const ObservedEntry& observed_entry) override {
    ASSERT_TRUE(observed_entries_.get());
    const ObservedEntries::iterator it = observed_entries_->find(
        ObservedEntryKey(observed_entry.entry_path, observed_entry.recursive));
    ASSERT_NE(observed_entries_->end(), it);
    it->second.last_tag = observed_entry.last_tag;
  }

  ProvidedFileSystemInfo* const file_system_info() const {
    return file_system_info_.get();
  }
  ObservedEntries* const observed_entries() const {
    return observed_entries_.get();
  }

 private:
  scoped_ptr<ProvidedFileSystemInfo> file_system_info_;
  scoped_ptr<ObservedEntries> observed_entries_;

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

  virtual ~FileSystemProviderServiceTest() {}

  virtual void SetUp() override {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test-user@example.com");
    user_manager_ = new FakeUserManager();
    user_manager_->AddUser(profile_->GetProfileName());
    user_manager_enabler_.reset(new ScopedUserManagerEnabler(user_manager_));
    extension_registry_.reset(new extensions::ExtensionRegistry(profile_));

    service_.reset(new Service(profile_, extension_registry_.get()));
    service_->SetFileSystemFactoryForTesting(
        base::Bind(&FakeProvidedFileSystem::Create));
    extension_ = CreateFakeExtension(kExtensionId);

    registry_ = new FakeRegistry;
    // Passes ownership to the service instance.
    service_->SetRegistryForTesting(make_scoped_ptr(registry_));

    fake_observed_entry_.entry_path =
        base::FilePath(FILE_PATH_LITERAL("/a/b/c"));
    fake_observed_entry_.recursive = true;
    fake_observed_entry_.last_tag = "hello-world";
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  FakeUserManager* user_manager_;
  scoped_ptr<ScopedUserManagerEnabler> user_manager_enabler_;
  scoped_ptr<extensions::ExtensionRegistry> extension_registry_;
  scoped_ptr<Service> service_;
  scoped_refptr<extensions::Extension> extension_;
  FakeRegistry* registry_;  // Owned by Service.
  ObservedEntry fake_observed_entry_;
};

TEST_F(FileSystemProviderServiceTest, MountFileSystem) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  EXPECT_TRUE(service_->MountFileSystem(
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
  EXPECT_TRUE(service_->MountFileSystem(kExtensionId, options));

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

  EXPECT_TRUE(service_->MountFileSystem(
      kExtensionId, MountOptions(kFileSystemId, kDisplayName)));
  EXPECT_FALSE(service_->MountFileSystem(
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
    EXPECT_TRUE(service_->MountFileSystem(
        kExtensionId, MountOptions(file_system_id, kDisplayName)));
  }
  ASSERT_EQ(kMaxFileSystems, observer.mounts.size());

  // The next file system is out of limit, and registering it should fail.
  EXPECT_FALSE(service_->MountFileSystem(
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

  EXPECT_TRUE(service_->MountFileSystem(
      kExtensionId, MountOptions(kFileSystemId, kDisplayName)));
  ASSERT_EQ(1u, observer.mounts.size());

  EXPECT_TRUE(service_->UnmountFileSystem(
      kExtensionId, kFileSystemId, Service::UNMOUNT_REASON_USER));
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

  EXPECT_TRUE(service_->MountFileSystem(
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

  EXPECT_TRUE(service_->MountFileSystem(
      kExtensionId, MountOptions(kFileSystemId, kDisplayName)));
  ASSERT_EQ(1u, observer.mounts.size());
  ASSERT_EQ(1u, service_->GetProvidedFileSystemInfoList().size());

  EXPECT_FALSE(service_->UnmountFileSystem(
      kWrongExtensionId, kFileSystemId, Service::UNMOUNT_REASON_USER));
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
      kExtensionId, options, base::FilePath(FILE_PATH_LITERAL("/a/b/c")));
  ObservedEntries fake_observed_entries;
  fake_observed_entries[ObservedEntryKey(fake_observed_entry_.entry_path,
                                         fake_observed_entry_.recursive)] =
      fake_observed_entry_;
  registry_->RememberFileSystem(file_system_info, fake_observed_entries);

  EXPECT_EQ(0u, observer.mounts.size());

  // Directly call the observer's method.
  service_->OnExtensionLoaded(profile_, extension_.get());

  ASSERT_EQ(1u, observer.mounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.mounts[0].error());

  EXPECT_EQ(file_system_info.extension_id(),
            observer.mounts[0].file_system_info().extension_id());
  EXPECT_EQ(file_system_info.file_system_id(),
            observer.mounts[0].file_system_info().file_system_id());
  EXPECT_EQ(file_system_info.writable(),
            observer.mounts[0].file_system_info().writable());
  EXPECT_EQ(file_system_info.supports_notify_tag(),
            observer.mounts[0].file_system_info().supports_notify_tag());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(1u, file_system_info_list.size());

  ProvidedFileSystemInterface* const file_system =
      service_->GetProvidedFileSystem(kExtensionId, kFileSystemId);
  ASSERT_TRUE(file_system);

  const ObservedEntries* const observed_entries =
      file_system->GetObservedEntries();
  ASSERT_TRUE(observed_entries);
  ASSERT_EQ(1u, observed_entries->size());

  const ObservedEntries::const_iterator restored_observed_entry_it =
      observed_entries->find(ObservedEntryKey(fake_observed_entry_.entry_path,
                                              fake_observed_entry_.recursive));
  ASSERT_NE(observed_entries->end(), restored_observed_entry_it);

  EXPECT_EQ(fake_observed_entry_.entry_path,
            restored_observed_entry_it->second.entry_path);
  EXPECT_EQ(fake_observed_entry_.recursive,
            restored_observed_entry_it->second.recursive);
  EXPECT_EQ(fake_observed_entry_.last_tag,
            restored_observed_entry_it->second.last_tag);

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, RememberFileSystem_OnMount) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  EXPECT_FALSE(registry_->file_system_info());
  EXPECT_FALSE(registry_->observed_entries());

  EXPECT_TRUE(service_->MountFileSystem(
      kExtensionId, MountOptions(kFileSystemId, kDisplayName)));
  ASSERT_EQ(1u, observer.mounts.size());

  ASSERT_TRUE(registry_->file_system_info());
  EXPECT_EQ(kExtensionId, registry_->file_system_info()->extension_id());
  EXPECT_EQ(kFileSystemId, registry_->file_system_info()->file_system_id());
  EXPECT_EQ(kDisplayName, registry_->file_system_info()->display_name());
  EXPECT_FALSE(registry_->file_system_info()->writable());
  EXPECT_FALSE(registry_->file_system_info()->supports_notify_tag());
  ASSERT_TRUE(registry_->observed_entries());

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, RememberFileSystem_OnUnmountOnShutdown) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  {
    EXPECT_FALSE(registry_->file_system_info());
    EXPECT_FALSE(registry_->observed_entries());
    EXPECT_TRUE(service_->MountFileSystem(
        kExtensionId, MountOptions(kFileSystemId, kDisplayName)));

    EXPECT_EQ(1u, observer.mounts.size());
    EXPECT_TRUE(registry_->file_system_info());
    EXPECT_TRUE(registry_->observed_entries());
  }

  {
    EXPECT_TRUE(service_->UnmountFileSystem(
        kExtensionId, kFileSystemId, Service::UNMOUNT_REASON_SHUTDOWN));

    EXPECT_EQ(1u, observer.unmounts.size());
    EXPECT_TRUE(registry_->file_system_info());
    EXPECT_TRUE(registry_->observed_entries());
  }

  service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, RememberFileSystem_OnUnmountByUser) {
  LoggingObserver observer;
  service_->AddObserver(&observer);

  {
    EXPECT_FALSE(registry_->file_system_info());
    EXPECT_FALSE(registry_->observed_entries());
    EXPECT_TRUE(service_->MountFileSystem(
        kExtensionId, MountOptions(kFileSystemId, kDisplayName)));

    EXPECT_EQ(1u, observer.mounts.size());
    EXPECT_TRUE(registry_->file_system_info());
    EXPECT_TRUE(registry_->observed_entries());
  }

  {
    EXPECT_TRUE(service_->UnmountFileSystem(
        kExtensionId, kFileSystemId, Service::UNMOUNT_REASON_USER));

    EXPECT_EQ(1u, observer.unmounts.size());
    EXPECT_FALSE(registry_->file_system_info());
    EXPECT_FALSE(registry_->observed_entries());
  }

  service_->RemoveObserver(&observer);
}

}  // namespace file_system_provider
}  // namespace chromeos
