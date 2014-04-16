// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/login/fake_user_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/external_mount_points.h"

namespace chromeos {
namespace file_system_provider {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
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

}  // namespace

class FileSystemProviderServiceTest : public testing::Test {
 protected:
  FileSystemProviderServiceTest() {}
  virtual ~FileSystemProviderServiceTest() {}

  virtual void SetUp() OVERRIDE {
    user_manager_ = new FakeUserManager();
    user_manager_enabler_.reset(new ScopedUserManagerEnabler(user_manager_));
    profile_.reset(new TestingProfile);
    user_manager_->AddUser(profile_->GetProfileName());
    file_system_provider_service_.reset(new Service(profile_.get()));
    file_system_provider_service_->SetFileSystemFactoryForTests(
        base::Bind(&FakeProvidedFileSystem::Create));
  }

  virtual void TearDown() {
    fileapi::ExternalMountPoints::GetSystemInstance()->RevokeAllFileSystems();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<ScopedUserManagerEnabler> user_manager_enabler_;
  FakeUserManager* user_manager_;
  scoped_ptr<Service> file_system_provider_service_;
};

TEST_F(FileSystemProviderServiceTest, MountFileSystem) {
  LoggingObserver observer;
  file_system_provider_service_->AddObserver(&observer);

  int file_system_id = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemName);

  EXPECT_LT(0, file_system_id);
  ASSERT_EQ(1u, observer.mounts.size());
  EXPECT_EQ(kExtensionId, observer.mounts[0].file_system_info().extension_id());
  EXPECT_EQ(1, observer.mounts[0].file_system_info().file_system_id());
  base::FilePath expected_mount_path =
      util::GetMountPointPath(profile_.get(), kExtensionId, file_system_id);
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

  int file_system_first_id = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemName);
  EXPECT_LT(0, file_system_first_id);

  int file_system_second_id = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemName);
  EXPECT_LT(0, file_system_second_id);

  EXPECT_NE(file_system_first_id, file_system_second_id);
  ASSERT_EQ(2u, observer.mounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.mounts[0].error());
  EXPECT_EQ(base::File::FILE_OK, observer.mounts[1].error());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      file_system_provider_service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(2u, file_system_info_list.size());

  file_system_provider_service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, MountFileSystem_StressTest) {
  LoggingObserver observer;
  file_system_provider_service_->AddObserver(&observer);

  const size_t kMaxFileSystems = 16;
  for (size_t i = 0; i < kMaxFileSystems; ++i) {
    int file_system_id = file_system_provider_service_->MountFileSystem(
        kExtensionId, kFileSystemName);
    EXPECT_LT(0, file_system_id);
  }
  ASSERT_EQ(kMaxFileSystems, observer.mounts.size());

  // The next file system is out of limit, and registering it should fail.
  int file_system_id = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemName);
  EXPECT_EQ(0, file_system_id);

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

  int file_system_id = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemName);
  EXPECT_LT(0, file_system_id);
  ASSERT_EQ(1u, observer.mounts.size());

  const bool result = file_system_provider_service_->UnmountFileSystem(
      kExtensionId, file_system_id);
  EXPECT_TRUE(result);
  ASSERT_EQ(1u, observer.unmounts.size());
  EXPECT_EQ(base::File::FILE_OK, observer.unmounts[0].error());

  EXPECT_EQ(kExtensionId,
            observer.unmounts[0].file_system_info().extension_id());
  EXPECT_EQ(1, observer.unmounts[0].file_system_info().file_system_id());
  base::FilePath expected_mount_path =
      util::GetMountPointPath(profile_.get(), kExtensionId, file_system_id);
  EXPECT_EQ(
      expected_mount_path.AsUTF8Unsafe(),
      observer.unmounts[0].file_system_info().mount_path().AsUTF8Unsafe());
  EXPECT_EQ(kFileSystemName,
            observer.unmounts[0].file_system_info().file_system_name());

  std::vector<ProvidedFileSystemInfo> file_system_info_list =
      file_system_provider_service_->GetProvidedFileSystemInfoList();
  ASSERT_EQ(0u, file_system_info_list.size());

  file_system_provider_service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, UnmountFileSystem_WrongExtensionId) {
  LoggingObserver observer;
  file_system_provider_service_->AddObserver(&observer);

  const std::string kWrongExtensionId = "helloworldhelloworldhelloworldhe";

  int file_system_id = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemName);
  EXPECT_LT(0, file_system_id);
  ASSERT_EQ(1u, observer.mounts.size());
  ASSERT_EQ(
      1u,
      file_system_provider_service_->GetProvidedFileSystemInfoList().size());

  const bool result = file_system_provider_service_->UnmountFileSystem(
      kWrongExtensionId, file_system_id);
  EXPECT_FALSE(result);
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

}  // namespace file_system_provider
}  // namespace chromeos
