// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/chromeos/file_system_provider/observer.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system.h"
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
  LoggingObserver() {}
  virtual ~LoggingObserver() {}

  // file_system_provider::Observer overrides.
  virtual void OnProvidedFileSystemRegistered(
      const ProvidedFileSystem& file_system) OVERRIDE {
    registered.push_back(new const ProvidedFileSystem(file_system));
  }

  virtual void OnProvidedFileSystemUnregistered(
      const ProvidedFileSystem& file_system) OVERRIDE {
    unregistered.push_back(new const ProvidedFileSystem(file_system));
  }

  ScopedVector<const ProvidedFileSystem> registered;
  ScopedVector<const ProvidedFileSystem> unregistered;
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

TEST_F(FileSystemProviderServiceTest, RegisterFileSystem) {
  LoggingObserver observer;
  file_system_provider_service_->AddObserver(&observer);

  int file_system_id = file_system_provider_service_->RegisterFileSystem(
      kExtensionId, kFileSystemName);

  EXPECT_LT(0, file_system_id);
  ASSERT_EQ(1u, observer.registered.size());
  EXPECT_EQ(kExtensionId, observer.registered[0]->extension_id());
  EXPECT_EQ(1, observer.registered[0]->file_system_id());
  EXPECT_EQ("/provided/mbflcebpggnecokmikipoihdbecnjfoj-1-testing_profile-hash",
            observer.registered[0]->mount_path().AsUTF8Unsafe());
  EXPECT_EQ(kFileSystemName, observer.registered[0]->file_system_name());
  ASSERT_EQ(0u, observer.unregistered.size());

  std::vector<ProvidedFileSystem> provided_file_systems =
      file_system_provider_service_->GetRegisteredFileSystems();
  ASSERT_EQ(1u, provided_file_systems.size());

  file_system_provider_service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, RegisterFileSystem_UniqueIds) {
  LoggingObserver observer;
  file_system_provider_service_->AddObserver(&observer);

  int file_system_first_id = file_system_provider_service_->RegisterFileSystem(
      kExtensionId, kFileSystemName);
  ASSERT_LT(0, file_system_first_id);

  int file_system_second_id = file_system_provider_service_->RegisterFileSystem(
      kExtensionId, kFileSystemName);
  ASSERT_LT(0, file_system_second_id);

  ASSERT_NE(file_system_first_id, file_system_second_id);
  ASSERT_EQ(2u, observer.registered.size());

  std::vector<ProvidedFileSystem> provided_file_systems =
      file_system_provider_service_->GetRegisteredFileSystems();
  ASSERT_EQ(2u, provided_file_systems.size());

  file_system_provider_service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, RegisterFileSystem_StressTest) {
  LoggingObserver observer;
  file_system_provider_service_->AddObserver(&observer);

  const size_t kMaxFileSystems = 16;
  for (size_t i = 0; i < kMaxFileSystems; ++i) {
    int file_system_id = file_system_provider_service_->RegisterFileSystem(
        kExtensionId, kFileSystemName);
    ASSERT_LT(0, file_system_id);
  }
  ASSERT_EQ(kMaxFileSystems, observer.registered.size());

  // The next file system is out of limit, and registering it should fail.
  int file_system_id = file_system_provider_service_->RegisterFileSystem(
      kExtensionId, kFileSystemName);
  ASSERT_EQ(0, file_system_id);
  ASSERT_EQ(kMaxFileSystems, observer.registered.size());

  std::vector<ProvidedFileSystem> provided_file_systems =
      file_system_provider_service_->GetRegisteredFileSystems();
  ASSERT_EQ(kMaxFileSystems, provided_file_systems.size());

  file_system_provider_service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, UnregisterFileSystem) {
  LoggingObserver observer;
  file_system_provider_service_->AddObserver(&observer);

  int file_system_id = file_system_provider_service_->RegisterFileSystem(
      kExtensionId, kFileSystemName);
  ASSERT_LT(0, file_system_id);
  ASSERT_EQ(1u, observer.registered.size());

  const bool result = file_system_provider_service_->UnregisterFileSystem(
      kExtensionId, file_system_id);
  ASSERT_TRUE(result);
  ASSERT_EQ(1u, observer.unregistered.size());

  EXPECT_EQ(kExtensionId, observer.unregistered[0]->extension_id());
  EXPECT_EQ(1, observer.unregistered[0]->file_system_id());
  EXPECT_EQ("/provided/mbflcebpggnecokmikipoihdbecnjfoj-1-testing_profile-hash",
            observer.unregistered[0]->mount_path().AsUTF8Unsafe());
  EXPECT_EQ(kFileSystemName, observer.unregistered[0]->file_system_name());

  std::vector<ProvidedFileSystem> provided_file_systems =
      file_system_provider_service_->GetRegisteredFileSystems();
  ASSERT_EQ(0u, provided_file_systems.size());

  file_system_provider_service_->RemoveObserver(&observer);
}

TEST_F(FileSystemProviderServiceTest, UnregisterFileSystem_WrongExtensionId) {
  LoggingObserver observer;
  file_system_provider_service_->AddObserver(&observer);

  const std::string kWrongExtensionId = "helloworldhelloworldhelloworldhe";

  int file_system_id = file_system_provider_service_->RegisterFileSystem(
      kExtensionId, kFileSystemName);
  ASSERT_LT(0, file_system_id);
  ASSERT_EQ(1u, observer.registered.size());

  const bool result = file_system_provider_service_->UnregisterFileSystem(
      kWrongExtensionId, file_system_id);
  ASSERT_FALSE(result);
  ASSERT_EQ(0u, observer.unregistered.size());

  std::vector<ProvidedFileSystem> provided_file_systems =
      file_system_provider_service_->GetRegisteredFileSystems();
  ASSERT_EQ(1u, provided_file_systems.size());

  file_system_provider_service_->RemoveObserver(&observer);
}

}  // namespace file_system_provider
}  // namespace chromeos
