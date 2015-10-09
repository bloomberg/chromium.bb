// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/callback_forward.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/extensions/app_data_migrator.h"
#include "chrome/browser/extensions/extension_special_storage_policy.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/indexed_db_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/mock_blob_url_request_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
scoped_ptr<TestingProfile> GetTestingProfile() {
  TestingProfile::Builder profile_builder;
  return profile_builder.Build();
}
}

namespace extensions {

class AppDataMigratorTest : public testing::Test {
 public:
  AppDataMigratorTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    profile_ = GetTestingProfile();
    registry_ = ExtensionRegistry::Get(profile_.get());
    migrator_ = scoped_ptr<AppDataMigrator>(
        new AppDataMigrator(profile_.get(), registry_));

    default_partition_ =
        content::BrowserContext::GetDefaultStoragePartition(profile_.get());

    idb_context_ = default_partition_->GetIndexedDBContext();
    idb_context_->SetTaskRunnerForTesting(
        base::MessageLoop::current()->task_runner().get());

    default_fs_context_ = default_partition_->GetFileSystemContext();

    url_request_context_ = scoped_ptr<content::MockBlobURLRequestContext>(
        new content::MockBlobURLRequestContext(default_fs_context_));
  }

  void TearDown() override {}

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<AppDataMigrator> migrator_;
  content::StoragePartition* default_partition_;
  ExtensionRegistry* registry_;
  storage::FileSystemContext* default_fs_context_;
  content::IndexedDBContext* idb_context_;
  scoped_ptr<content::MockBlobURLRequestContext> url_request_context_;
};

scoped_refptr<const Extension> GetTestExtension(bool platform_app) {
  scoped_refptr<const Extension> app;
  if (platform_app) {
    app = ExtensionBuilder()
              .SetManifest(
                   DictionaryBuilder()
                       .Set("name", "test app")
                       .Set("version", "1")
                       .Set("app", DictionaryBuilder().Set(
                                       "background",
                                       DictionaryBuilder().Set(
                                           "scripts", ListBuilder().Append(
                                                          "background.js"))))
                       .Set("permissions",
                            ListBuilder().Append("unlimitedStorage")))
              .Build();
  } else {
    app = ExtensionBuilder()
              .SetManifest(DictionaryBuilder()
                               .Set("name", "test app")
                               .Set("version", "1")
                               .Set("app", DictionaryBuilder().Set(
                                               "launch",
                                               DictionaryBuilder().Set(
                                                   "local_path", "index.html")))
                               .Set("permissions",
                                    ListBuilder().Append("unlimitedStorage")))
              .Build();
  }
  return app;
}

void MigrationCallback() {
}

void DidWrite(base::File::Error status, int64 bytes, bool complete) {
  base::MessageLoop::current()->QuitWhenIdle();
}

void DidCreate(base::File::Error status) {
}

void DidOpenFileSystem(const GURL& root,
                       const std::string& name,
                       base::File::Error result) {
}

void OpenFileSystems(storage::FileSystemContext* fs_context,
                     GURL extension_url) {
  fs_context->OpenFileSystem(extension_url, storage::kFileSystemTypeTemporary,
                             storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                             base::Bind(&DidOpenFileSystem));

  fs_context->OpenFileSystem(extension_url, storage::kFileSystemTypePersistent,
                             storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                             base::Bind(&DidOpenFileSystem));
  base::MessageLoop::current()->RunUntilIdle();
}

void GenerateTestFiles(content::MockBlobURLRequestContext* url_request_context,
                       const Extension* ext,
                       storage::FileSystemContext* fs_context,
                       Profile* profile) {
  profile->GetExtensionSpecialStoragePolicy()->GrantRightsForExtension(ext,
                                                                       profile);

  base::FilePath path(FILE_PATH_LITERAL("test.txt"));
  GURL extension_url =
      extensions::Extension::GetBaseURLFromExtensionId(ext->id());

  OpenFileSystems(fs_context, extension_url);

  storage::FileSystemURL fs_temp_url = fs_context->CreateCrackedFileSystemURL(
      extension_url, storage::kFileSystemTypeTemporary, path);

  storage::FileSystemURL fs_persistent_url =
      fs_context->CreateCrackedFileSystemURL(
          extension_url, storage::kFileSystemTypePersistent, path);

  content::ScopedTextBlob blob1(*url_request_context, "blob-id:success1",
                                "Hello, world!\n");

  fs_context->operation_runner()->CreateFile(fs_temp_url, false,
                                             base::Bind(&DidCreate));

  fs_context->operation_runner()->CreateFile(fs_persistent_url, false,
                                             base::Bind(&DidCreate));
  base::MessageLoop::current()->RunUntilIdle();

  fs_context->operation_runner()->Write(url_request_context, fs_temp_url,
                                        blob1.GetBlobDataHandle(), 0,
                                        base::Bind(&DidWrite));
  base::MessageLoop::current()->Run();
  fs_context->operation_runner()->Write(url_request_context, fs_persistent_url,
                                        blob1.GetBlobDataHandle(), 0,
                                        base::Bind(&DidWrite));
  base::MessageLoop::current()->Run();
}

void VerifyFileContents(base::File file,
                        const base::Closure& on_close_callback) {
  ASSERT_EQ(14, file.GetLength());
  scoped_ptr<char[]> buffer(new char[15]);

  file.Read(0, buffer.get(), 14);
  buffer.get()[14] = 0;

  std::string expected = "Hello, world!\n";
  std::string actual = buffer.get();
  EXPECT_EQ(expected, actual);

  file.Close();
  if (!on_close_callback.is_null())
    on_close_callback.Run();
  base::MessageLoop::current()->QuitWhenIdle();
}

void VerifyTestFilesMigrated(content::StoragePartition* new_partition,
                             const Extension* new_ext) {
  GURL extension_url =
      extensions::Extension::GetBaseURLFromExtensionId(new_ext->id());
  storage::FileSystemContext* new_fs_context =
      new_partition->GetFileSystemContext();

  OpenFileSystems(new_fs_context, extension_url);

  base::FilePath path(FILE_PATH_LITERAL("test.txt"));

  storage::FileSystemURL fs_temp_url =
      new_fs_context->CreateCrackedFileSystemURL(
          extension_url, storage::kFileSystemTypeTemporary, path);
  storage::FileSystemURL fs_persistent_url =
      new_fs_context->CreateCrackedFileSystemURL(
          extension_url, storage::kFileSystemTypePersistent, path);

  new_fs_context->operation_runner()->OpenFile(
      fs_temp_url, base::File::FLAG_READ | base::File::FLAG_OPEN,
      base::Bind(&VerifyFileContents));
  base::MessageLoop::current()->Run();
  new_fs_context->operation_runner()->OpenFile(
      fs_persistent_url, base::File::FLAG_READ | base::File::FLAG_OPEN,
      base::Bind(&VerifyFileContents));
  base::MessageLoop::current()->Run();
}

TEST_F(AppDataMigratorTest, ShouldMigrate) {
  scoped_refptr<const Extension> old_ext = GetTestExtension(false);
  scoped_refptr<const Extension> new_ext = GetTestExtension(true);

  EXPECT_TRUE(AppDataMigrator::NeedsMigration(old_ext.get(), new_ext.get()));
}

TEST_F(AppDataMigratorTest, ShouldNotMigratePlatformApp) {
  scoped_refptr<const Extension> old_ext = GetTestExtension(true);
  scoped_refptr<const Extension> new_ext = GetTestExtension(true);

  EXPECT_FALSE(AppDataMigrator::NeedsMigration(old_ext.get(), new_ext.get()));
}

TEST_F(AppDataMigratorTest, ShouldNotMigrateLegacyApp) {
  scoped_refptr<const Extension> old_ext = GetTestExtension(false);
  scoped_refptr<const Extension> new_ext = GetTestExtension(false);

  EXPECT_FALSE(AppDataMigrator::NeedsMigration(old_ext.get(), new_ext.get()));
}

TEST_F(AppDataMigratorTest, NoOpMigration) {
  scoped_refptr<const Extension> old_ext = GetTestExtension(false);
  scoped_refptr<const Extension> new_ext = GetTestExtension(true);

  // Nothing to migrate. Basically this should just not cause an error
  migrator_->DoMigrationAndReply(old_ext.get(), new_ext.get(),
                                 base::Bind(&MigrationCallback));
}

TEST_F(AppDataMigratorTest, FileSystemMigration) {
  scoped_refptr<const Extension> old_ext = GetTestExtension(false);
  scoped_refptr<const Extension> new_ext = GetTestExtension(true);

  GenerateTestFiles(url_request_context_.get(), old_ext.get(),
                    default_fs_context_, profile_.get());

  migrator_->DoMigrationAndReply(old_ext.get(), new_ext.get(),
                                 base::Bind(&MigrationCallback));

  base::MessageLoop::current()->RunUntilIdle();

  registry_->AddEnabled(new_ext);
  GURL extension_url =
      extensions::Extension::GetBaseURLFromExtensionId(new_ext->id());

  content::StoragePartition* new_partition =
      content::BrowserContext::GetStoragePartitionForSite(profile_.get(),
                                                          extension_url);

  ASSERT_NE(new_partition->GetPath(), default_partition_->GetPath());

  VerifyTestFilesMigrated(new_partition, new_ext.get());
}

}  // namespace extensions
