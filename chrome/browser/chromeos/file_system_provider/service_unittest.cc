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
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/external_mount_points.h"

namespace chromeos {
namespace file_system_provider {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "camera/pictures/id !@#$%^&*()_+";
const char kFileSystemName[] = "Camera Pictures";

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
      base::File::Error error) OVERRIDE {
    mounts.push_back(Event(file_system_info, error));
  }

  virtual void OnProvidedFileSystemUnmount(
      const ProvidedFileSystemInfo& file_system_info,
      base::File::Error error) OVERRIDE {
    unmounts.push_back(Event(file_system_info, error));
  }

  std::vector<Event> mounts;
  std::vector<Event> unmounts;
};

// Creates a fake extension with the specified |extension_id|.
scoped_refptr<extensions::Extension> createFakeExtension(
    const std::string& extension_id) {
  base::DictionaryValue manifest;
  std::string error;
  manifest.SetString(extensions::manifest_keys::kVersion, "1.0.0.0");
  manifest.SetString(extensions::manifest_keys::kName, "unused");
  return extensions::Extension::Create(base::FilePath(),
                                       extensions::Manifest::UNPACKED,
                                       manifest,
                                       extensions::Extension::NO_FLAGS,
                                       extension_id,
                                       &error);
}

// Stores a provided file system information in preferences.
void RememberFakeFileSystem(TestingProfile* profile,
                            const std::string& extension_id,
                            const std::string& file_system_id,
                            const std::string& file_system_name) {
  TestingPrefServiceSyncable* pref_service = profile->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  base::DictionaryValue extensions;
  base::ListValue* file_systems = new base::ListValue();
  base::DictionaryValue* file_system = new base::DictionaryValue();
  file_system->SetString(kPrefKeyFileSystemId, kFileSystemId);
  file_system->SetString(kPrefKeyFileSystemName, kFileSystemName);
  file_systems->Append(file_system);
  extensions.Set(kExtensionId, file_systems);

  pref_service->Set(prefs::kFileSystemProviderMounted, extensions);
}

}  // namespace

class FileSystemProviderServiceTest : public testing::Test {
 protected:
  FileSystemProviderServiceTest() {}
  virtual ~FileSystemProviderServiceTest() {}

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);
    user_manager_ = new FakeUserManager();
    user_manager_->AddUser(profile_->GetProfileName());
    user_manager_enabler_.reset(new ScopedUserManagerEnabler(user_manager_));
    extension_registry_.reset(
        new extensions::ExtensionRegistry(profile_.get()));
    file_system_provider_service_.reset(
        new Service(profile_.get(), extension_registry_.get()));
    file_system_provider_service_->SetFileSystemFactoryForTesting(
        base::Bind(&FakeProvidedFileSystem::Create));
    extension_ = createFakeExtension(kExtensionId);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  FakeUserManager* user_manager_;
  scoped_ptr<ScopedUserManagerEnabler> user_manager_enabler_;
  scoped_ptr<extensions::ExtensionRegistry> extension_registry_;
  scoped_ptr<Service> file_system_provider_service_;
  scoped_refptr<extensions::Extension> extension_;
};

TEST_F(FileSystemProviderServiceTest, MountFileSystem) {
  LoggingObserver observer;
  file_system_provider_service_->AddObserver(&observer);

  const bool result = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemId, kFileSystemName);
  EXPECT_TRUE(result);

  ASSERT_EQ(1u, observer.mounts.size());
  EXPECT_EQ(kExtensionId, observer.mounts[0].file_system_info().extension_id());
  EXPECT_EQ(kFileSystemId,
            observer.mounts[0].file_system_info().file_system_id());
  base::FilePath expected_mount_path =
      util::GetMountPath(profile_.get(), kExtensionId, kFileSystemId);
  EXPECT_EQ(expected_mount_path.AsUTF8Unsafe(),
            observer.mounts[0].file_system_info().mount_path().AsUTF8Unsafe());
  EXPECT_EQ(kFileSystemName,
            observer.mounts[0].file_system_info().file_system_name());
  EXPECT_EQ(base::File::FILE_OK, observer.mounts[0].error());
  ASSERT_EQ(0u, observer.unmounts.size());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      file_system_provider_service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(1u, file_system_info_list.size());

  file_system_provider_service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, MountFileSystem_UniqueIds) {
  LoggingObserver observer;
  file_system_provider_service_->AddObserver(&observer);

  const bool result = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemId, kFileSystemName);
  EXPECT_TRUE(result);

  const bool second_result = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemId, kFileSystemName);
  EXPECT_FALSE(second_result);

  ASSERT_EQ(2u, observer.mounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.mounts[0].error());
  EXPECT_EQ(base::File::FILE_ERROR_EXISTS, observer.mounts[1].error());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      file_system_provider_service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(1u, file_system_info_list.size());

  file_system_provider_service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, MountFileSystem_StressTest) {
  LoggingObserver observer;
  file_system_provider_service_->AddObserver(&observer);

  const size_t kMaxFileSystems = 16;
  for (size_t i = 0; i < kMaxFileSystems; ++i) {
    const std::string file_system_id =
        std::string("test-") + base::IntToString(i);
    const bool result = file_system_provider_service_->MountFileSystem(
        kExtensionId, file_system_id, kFileSystemName);
    EXPECT_TRUE(result);
  }
  ASSERT_EQ(kMaxFileSystems, observer.mounts.size());

  // The next file system is out of limit, and registering it should fail.
  const bool result = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemId, kFileSystemName);
  EXPECT_FALSE(result);

  ASSERT_EQ(kMaxFileSystems + 1, observer.mounts.size());
  EXPECT_EQ(base::File::FILE_ERROR_TOO_MANY_OPENED,
            observer.mounts[kMaxFileSystems].error());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      file_system_provider_service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(kMaxFileSystems, file_system_info_list.size());

  file_system_provider_service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, UnmountFileSystem) {
  LoggingObserver observer;
  file_system_provider_service_->AddObserver(&observer);

  const bool result = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemId, kFileSystemName);
  EXPECT_TRUE(result);
  ASSERT_EQ(1u, observer.mounts.size());

  const bool unmount_result = file_system_provider_service_->UnmountFileSystem(
      kExtensionId, kFileSystemId);
  EXPECT_TRUE(unmount_result);
  ASSERT_EQ(1u, observer.unmounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.unmounts[0].error());

  EXPECT_EQ(kExtensionId,
            observer.unmounts[0].file_system_info().extension_id());
  EXPECT_EQ(kFileSystemId,
            observer.unmounts[0].file_system_info().file_system_id());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      file_system_provider_service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(0u, file_system_info_list.size());

  file_system_provider_service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, UnmountFileSystem_OnExtensionUnload) {
  LoggingObserver observer;
  file_system_provider_service_->AddObserver(&observer);

  const bool result = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemId, kFileSystemName);
  EXPECT_TRUE(result);
  ASSERT_EQ(1u, observer.mounts.size());

  // Directly call the observer's method.
  file_system_provider_service_->OnExtensionUnloaded(
      profile_.get(),
      extension_.get(),
      extensions::UnloadedExtensionInfo::REASON_DISABLE);

  ASSERT_EQ(1u, observer.unmounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.unmounts[0].error());

  EXPECT_EQ(kExtensionId,
            observer.unmounts[0].file_system_info().extension_id());
  EXPECT_EQ(kFileSystemId,
            observer.unmounts[0].file_system_info().file_system_id());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      file_system_provider_service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(0u, file_system_info_list.size());

  file_system_provider_service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, UnmountFileSystem_WrongExtensionId) {
  LoggingObserver observer;
  file_system_provider_service_->AddObserver(&observer);

  const std::string kWrongExtensionId = "helloworldhelloworldhelloworldhe";

  const bool result = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemId, kFileSystemName);
  EXPECT_TRUE(result);
  ASSERT_EQ(1u, observer.mounts.size());
  ASSERT_EQ(
      1u,
      file_system_provider_service_->GetProvidedFileSystemInfoList().size());

  const bool unmount_result = file_system_provider_service_->UnmountFileSystem(
      kWrongExtensionId, kFileSystemId);
  EXPECT_FALSE(unmount_result);
  ASSERT_EQ(1u, observer.unmounts.size());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, observer.unmounts[0].error());
  ASSERT_EQ(
      1u,
      file_system_provider_service_->GetProvidedFileSystemInfoList().size());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      file_system_provider_service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(1u, file_system_info_list.size());

  file_system_provider_service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, RestoreFileSystem_OnExtensionLoad) {
  LoggingObserver observer;
  file_system_provider_service_->AddObserver(&observer);

  // Create a fake entry in the preferences.
  RememberFakeFileSystem(
      profile_.get(), kExtensionId, kFileSystemId, kFileSystemName);

  EXPECT_EQ(0u, observer.mounts.size());

  // Directly call the observer's method.
  file_system_provider_service_->OnExtensionLoaded(profile_.get(),
                                                   extension_.get());

  ASSERT_EQ(1u, observer.mounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.mounts[0].error());

  EXPECT_EQ(kExtensionId, observer.mounts[0].file_system_info().extension_id());
  EXPECT_EQ(kFileSystemId,
            observer.mounts[0].file_system_info().file_system_id());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      file_system_provider_service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(1u, file_system_info_list.size());

  file_system_provider_service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, ForgetFileSystem_OnExtensionUnload) {
  LoggingObserver observer;
  file_system_provider_service_->AddObserver(&observer);

  // Create a fake entry in the preferences.
  RememberFakeFileSystem(
      profile_.get(), kExtensionId, kFileSystemId, kFileSystemName);

  // Directly call the observer's methods.
  file_system_provider_service_->OnExtensionLoaded(profile_.get(),
                                                   extension_.get());

  file_system_provider_service_->OnExtensionUnloaded(
      profile_.get(),
      extension_.get(),
      extensions::UnloadedExtensionInfo::REASON_DISABLE);

  ASSERT_EQ(1u, observer.mounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.mounts[0].error());
  ASSERT_EQ(1u, observer.unmounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.unmounts[0].error());

  TestingPrefServiceSyncable* pref_service = profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  const base::DictionaryValue* extensions =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  ASSERT_TRUE(extensions);

  const base::ListValue* file_systems;
  EXPECT_FALSE(extensions->GetList(kExtensionId, &file_systems));

  file_system_provider_service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, RememberFileSystem_OnShutdown) {
  {
    scoped_ptr<Service> service(
        new Service(profile_.get(), extension_registry_.get()));
    service->SetFileSystemFactoryForTesting(
        base::Bind(&FakeProvidedFileSystem::Create));

    LoggingObserver observer;
    service->AddObserver(&observer);

    const bool result =
        service->MountFileSystem(kExtensionId, kFileSystemId, kFileSystemName);
    EXPECT_TRUE(result);
    ASSERT_EQ(1u, observer.mounts.size());

    service->RemoveObserver(&observer);
  }

  TestingPrefServiceSyncable* pref_service = profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  const base::DictionaryValue* extensions =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  ASSERT_TRUE(extensions);

  const base::ListValue* file_systems;
  ASSERT_TRUE(extensions->GetList(kExtensionId, &file_systems));
  ASSERT_EQ(1u, file_systems->GetSize());

  const base::DictionaryValue* file_system = NULL;
  ASSERT_TRUE(file_systems->GetDictionary(0, &file_system));

  std::string file_system_id;
  file_system->GetString(kPrefKeyFileSystemId, &file_system_id);
  EXPECT_EQ(kFileSystemId, file_system_id);

  std::string file_system_name;
  file_system->GetString(kPrefKeyFileSystemName, &file_system_name);
  EXPECT_EQ(kFileSystemName, file_system_name);
}

}  // namespace file_system_provider
}  // namespace chromeos
