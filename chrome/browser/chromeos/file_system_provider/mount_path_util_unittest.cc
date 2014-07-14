// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/mount_path_util.h"

#include <string>

#include "base/files/file.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/fake_provided_file_system.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_interface.h"
#include "chrome/browser/chromeos/file_system_provider/service.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/isolated_context.h"

namespace chromeos {
namespace file_system_provider {
namespace util {

namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kFileSystemId[] = "File/System/Id";
const char kDisplayName[] = "Camera Pictures";

// Creates a FileSystemURL for tests.
fileapi::FileSystemURL CreateFileSystemURL(
    Profile* profile,
    const ProvidedFileSystemInfo& file_system_info,
    const base::FilePath& file_path) {
  const std::string origin =
      std::string("chrome-extension://") + file_system_info.extension_id();
  const base::FilePath mount_path = file_system_info.mount_path();
  const fileapi::ExternalMountPoints* const mount_points =
      fileapi::ExternalMountPoints::GetSystemInstance();
  DCHECK(mount_points);
  DCHECK(file_path.IsAbsolute());
  base::FilePath relative_path(file_path.value().substr(1));
  return mount_points->CreateCrackedFileSystemURL(
      GURL(origin),
      fileapi::kFileSystemTypeExternal,
      base::FilePath(mount_path.BaseName().Append(relative_path)));
}

// Creates a Service instance. Used to be able to destroy the service in
// TearDown().
KeyedService* CreateService(content::BrowserContext* context) {
  return new Service(Profile::FromBrowserContext(context),
                     extensions::ExtensionRegistry::Get(context));
}

}  // namespace

class FileSystemProviderMountPathUtilTest : public testing::Test {
 protected:
  FileSystemProviderMountPathUtilTest() {}
  virtual ~FileSystemProviderMountPathUtilTest() {}

  virtual void SetUp() OVERRIDE {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("testing-profile");
    user_manager_ = new FakeUserManager();
    user_manager_enabler_.reset(new ScopedUserManagerEnabler(user_manager_));
    user_manager_->AddUser(profile_->GetProfileName());
    ServiceFactory::GetInstance()->SetTestingFactory(profile_, &CreateService);
    file_system_provider_service_ = Service::Get(profile_);
    file_system_provider_service_->SetFileSystemFactoryForTesting(
        base::Bind(&FakeProvidedFileSystem::Create));
  }

  virtual void TearDown() OVERRIDE {
    // Setting the testing factory to NULL will destroy the created service
    // associated with the testing profile.
    ServiceFactory::GetInstance()->SetTestingFactory(profile_, NULL);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;  // Owned by TestingProfileManager.
  scoped_ptr<ScopedUserManagerEnabler> user_manager_enabler_;
  FakeUserManager* user_manager_;
  Service* file_system_provider_service_;  // Owned by its factory.
};

TEST_F(FileSystemProviderMountPathUtilTest, GetMountPath) {
  const base::FilePath result =
      GetMountPath(profile_, kExtensionId, kFileSystemId);
  const std::string expected =
      "/provided/mbflcebpggnecokmikipoihdbecnjfoj:"
      "File%2FSystem%2FId:testing-profile-hash";
  EXPECT_EQ(expected, result.AsUTF8Unsafe());
}

TEST_F(FileSystemProviderMountPathUtilTest, IsFileSystemProviderLocalPath) {
  const base::FilePath mount_path =
      GetMountPath(profile_, kExtensionId, kFileSystemId);
  const base::FilePath file_path =
      base::FilePath::FromUTF8Unsafe("/hello/world.txt");
  const base::FilePath local_file_path =
      mount_path.Append(base::FilePath(file_path.value().substr(1)));

  EXPECT_TRUE(IsFileSystemProviderLocalPath(mount_path));
  EXPECT_TRUE(IsFileSystemProviderLocalPath(local_file_path));

  EXPECT_FALSE(IsFileSystemProviderLocalPath(
      base::FilePath::FromUTF8Unsafe("provided/hello-world/test.txt")));
  EXPECT_FALSE(IsFileSystemProviderLocalPath(
      base::FilePath::FromUTF8Unsafe("/provided")));
  EXPECT_FALSE(
      IsFileSystemProviderLocalPath(base::FilePath::FromUTF8Unsafe("/")));
  EXPECT_FALSE(IsFileSystemProviderLocalPath(base::FilePath()));
}

TEST_F(FileSystemProviderMountPathUtilTest, Parser) {
  const bool result = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemId, kDisplayName, false /* writable */);
  ASSERT_TRUE(result);
  const ProvidedFileSystemInfo file_system_info =
      file_system_provider_service_->GetProvidedFileSystem(kExtensionId,
                                                           kFileSystemId)
          ->GetFileSystemInfo();

  const base::FilePath kFilePath =
      base::FilePath::FromUTF8Unsafe("/hello/world.txt");
  const fileapi::FileSystemURL url =
      CreateFileSystemURL(profile_, file_system_info, kFilePath);
  EXPECT_TRUE(url.is_valid());

  FileSystemURLParser parser(url);
  EXPECT_TRUE(parser.Parse());

  ProvidedFileSystemInterface* file_system = parser.file_system();
  ASSERT_TRUE(file_system);
  EXPECT_EQ(kFileSystemId, file_system->GetFileSystemInfo().file_system_id());
  EXPECT_EQ(kFilePath.AsUTF8Unsafe(), parser.file_path().AsUTF8Unsafe());
}

TEST_F(FileSystemProviderMountPathUtilTest, Parser_RootPath) {
  const bool result = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemId, kDisplayName, false /* writable */);
  ASSERT_TRUE(result);
  const ProvidedFileSystemInfo file_system_info =
      file_system_provider_service_->GetProvidedFileSystem(kExtensionId,
                                                           kFileSystemId)
          ->GetFileSystemInfo();

  const base::FilePath kFilePath = base::FilePath::FromUTF8Unsafe("/");
  const fileapi::FileSystemURL url =
      CreateFileSystemURL(profile_, file_system_info, kFilePath);
  EXPECT_TRUE(url.is_valid());

  FileSystemURLParser parser(url);
  EXPECT_TRUE(parser.Parse());

  ProvidedFileSystemInterface* file_system = parser.file_system();
  ASSERT_TRUE(file_system);
  EXPECT_EQ(kFileSystemId, file_system->GetFileSystemInfo().file_system_id());
  EXPECT_EQ(kFilePath.AsUTF8Unsafe(), parser.file_path().AsUTF8Unsafe());
}

TEST_F(FileSystemProviderMountPathUtilTest, Parser_WrongUrl) {
  const ProvidedFileSystemInfo file_system_info(
      kExtensionId,
      kFileSystemId,
      kDisplayName,
      false /* writable */,
      GetMountPath(profile_, kExtensionId, kFileSystemId));

  const base::FilePath kFilePath = base::FilePath::FromUTF8Unsafe("/hello");
  const fileapi::FileSystemURL url =
      CreateFileSystemURL(profile_, file_system_info, kFilePath);
  // It is impossible to create a cracked URL for a mount point which doesn't
  // exist, therefore is will always be invalid, and empty.
  EXPECT_FALSE(url.is_valid());

  FileSystemURLParser parser(url);
  EXPECT_FALSE(parser.Parse());
}

TEST_F(FileSystemProviderMountPathUtilTest, Parser_IsolatedURL) {
  const bool result = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemId, kDisplayName, false /* writable */);
  ASSERT_TRUE(result);
  const ProvidedFileSystemInfo file_system_info =
      file_system_provider_service_->GetProvidedFileSystem(kExtensionId,
                                                           kFileSystemId)
          ->GetFileSystemInfo();

  const base::FilePath kFilePath =
      base::FilePath::FromUTF8Unsafe("/hello/world.txt");
  const fileapi::FileSystemURL url =
      CreateFileSystemURL(profile_, file_system_info, kFilePath);
  EXPECT_TRUE(url.is_valid());

  // Create an isolated URL for the original one.
  fileapi::IsolatedContext* const isolated_context =
      fileapi::IsolatedContext::GetInstance();
  const std::string isolated_file_system_id =
      isolated_context->RegisterFileSystemForPath(
          fileapi::kFileSystemTypeProvided,
          url.filesystem_id(),
          url.path(),
          NULL);

  const base::FilePath isolated_virtual_path =
      isolated_context->CreateVirtualRootPath(isolated_file_system_id)
          .Append(kFilePath.BaseName().value());

  const fileapi::FileSystemURL isolated_url =
      isolated_context->CreateCrackedFileSystemURL(
          url.origin(),
          fileapi::kFileSystemTypeIsolated,
          isolated_virtual_path);

  EXPECT_TRUE(isolated_url.is_valid());

  FileSystemURLParser parser(isolated_url);
  EXPECT_TRUE(parser.Parse());

  ProvidedFileSystemInterface* file_system = parser.file_system();
  ASSERT_TRUE(file_system);
  EXPECT_EQ(kFileSystemId, file_system->GetFileSystemInfo().file_system_id());
  EXPECT_EQ(kFilePath.AsUTF8Unsafe(), parser.file_path().AsUTF8Unsafe());
}

TEST_F(FileSystemProviderMountPathUtilTest, LocalPathParser) {
  const bool result = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemId, kDisplayName, false /* writable */);
  ASSERT_TRUE(result);
  const ProvidedFileSystemInfo file_system_info =
      file_system_provider_service_->GetProvidedFileSystem(kExtensionId,
                                                           kFileSystemId)
          ->GetFileSystemInfo();

  const base::FilePath kFilePath =
      base::FilePath::FromUTF8Unsafe("/hello/world.txt");
  const base::FilePath kLocalFilePath = file_system_info.mount_path().Append(
      base::FilePath(kFilePath.value().substr(1)));

  LOG(ERROR) << kLocalFilePath.value();
  LocalPathParser parser(profile_, kLocalFilePath);
  EXPECT_TRUE(parser.Parse());

  ProvidedFileSystemInterface* file_system = parser.file_system();
  ASSERT_TRUE(file_system);
  EXPECT_EQ(kFileSystemId, file_system->GetFileSystemInfo().file_system_id());
  EXPECT_EQ(kFilePath.AsUTF8Unsafe(), parser.file_path().AsUTF8Unsafe());
}

TEST_F(FileSystemProviderMountPathUtilTest, LocalPathParser_RootPath) {
  const bool result = file_system_provider_service_->MountFileSystem(
      kExtensionId, kFileSystemId, kDisplayName, false /* writable */);
  ASSERT_TRUE(result);
  const ProvidedFileSystemInfo file_system_info =
      file_system_provider_service_->GetProvidedFileSystem(kExtensionId,
                                                           kFileSystemId)
          ->GetFileSystemInfo();

  const base::FilePath kFilePath = base::FilePath::FromUTF8Unsafe("/");
  const base::FilePath kLocalFilePath = file_system_info.mount_path();

  LocalPathParser parser(profile_, kLocalFilePath);
  EXPECT_TRUE(parser.Parse());

  ProvidedFileSystemInterface* file_system = parser.file_system();
  ASSERT_TRUE(file_system);
  EXPECT_EQ(kFileSystemId, file_system->GetFileSystemInfo().file_system_id());
  EXPECT_EQ(kFilePath.AsUTF8Unsafe(), parser.file_path().AsUTF8Unsafe());
}

TEST_F(FileSystemProviderMountPathUtilTest, LocalPathParser_WrongPath) {
  {
    const base::FilePath kFilePath = base::FilePath::FromUTF8Unsafe("/hello");
    LocalPathParser parser(profile_, kFilePath);
    EXPECT_FALSE(parser.Parse());
  }

  {
    const base::FilePath kFilePath =
        base::FilePath::FromUTF8Unsafe("/provided");
    LocalPathParser parser(profile_, kFilePath);
    EXPECT_FALSE(parser.Parse());
  }

  {
    const base::FilePath kFilePath =
        base::FilePath::FromUTF8Unsafe("provided/hello/world");
    LocalPathParser parser(profile_, kFilePath);
    EXPECT_FALSE(parser.Parse());
  }
}

}  // namespace util
}  // namespace file_system_provider
}  // namespace chromeos
