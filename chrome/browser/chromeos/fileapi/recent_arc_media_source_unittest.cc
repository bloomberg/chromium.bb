// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/recent_arc_media_source.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_mounter.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/chromeos/fileapi/recent_context.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/common/file_system.mojom.h"
#include "components/arc/test/fake_file_system_instance.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace {

const char kMediaDocumentsProviderAuthority[] =
    "com.android.providers.media.documents";
const char kImagesRootId[] = "images_root";

std::unique_ptr<KeyedService> CreateFileSystemOperationRunnerForTesting(
    content::BrowserContext* context) {
  return arc::ArcFileSystemOperationRunner::CreateForTesting(
      context, arc::ArcServiceManager::Get()->arc_bridge_service());
}

arc::FakeFileSystemInstance::Document MakeDocument(
    const std::string& document_id,
    const std::string& parent_document_id,
    const std::string& display_name,
    const std::string& mime_type) {
  return arc::FakeFileSystemInstance::Document(
      kMediaDocumentsProviderAuthority,  // authority
      document_id,                       // document_id
      parent_document_id,                // parent_document_id
      display_name,                      // display_name
      mime_type,                         // mime_type
      0,                                 // size
      0);                                // last_modified
}

class RecentArcMediaSourceTest : public testing::Test {
 public:
  RecentArcMediaSourceTest() = default;

  void SetUp() override {
    arc_service_manager_ = base::MakeUnique<arc::ArcServiceManager>();
    profile_ = base::MakeUnique<TestingProfile>();
    arc_service_manager_->set_browser_context(profile_.get());
    arc::ArcFileSystemOperationRunner::GetFactory()->SetTestingFactoryAndUse(
        profile_.get(), &CreateFileSystemOperationRunnerForTesting);

    // Mount ARC file systems.
    arc::ArcFileSystemMounter::GetForBrowserContext(profile_.get());

    // Add documents to FakeFileSystemInstance. Note that they are not available
    // until EnableFakeFileSystemInstance() is called.
    AddDocumentsToFakeFileSystemInstance();

    source_ = base::MakeUnique<RecentArcMediaSource>(profile_.get());
  }

 protected:
  void AddDocumentsToFakeFileSystemInstance() {
    auto root_doc =
        MakeDocument(kImagesRootId, "", "", arc::kAndroidDirectoryMimeType);
    auto cat_doc = MakeDocument("cat", kImagesRootId, "cat.png", "image/png");
    auto dog_doc = MakeDocument("dog", kImagesRootId, "dog.jpg", "image/jpeg");
    auto fox_doc = MakeDocument("fox", kImagesRootId, "fox.gif", "image/gif");
    auto elk_doc = MakeDocument("elk", kImagesRootId, "elk.tiff", "image/tiff");
    fake_file_system_.AddDocument(root_doc);
    fake_file_system_.AddDocument(cat_doc);
    fake_file_system_.AddDocument(dog_doc);
    fake_file_system_.AddDocument(fox_doc);
    fake_file_system_.AddRecentDocument(kImagesRootId, root_doc);
    fake_file_system_.AddRecentDocument(kImagesRootId, cat_doc);
    fake_file_system_.AddRecentDocument(kImagesRootId, dog_doc);
    fake_file_system_.AddRecentDocument(kImagesRootId, elk_doc);
  }

  void EnableFakeFileSystemInstance() {
    arc_service_manager_->arc_bridge_service()->file_system()->SetInstance(
        &fake_file_system_);
  }

  std::vector<storage::FileSystemURL> GetRecentFiles() {
    std::vector<storage::FileSystemURL> files;

    base::RunLoop run_loop;

    source_->GetRecentFiles(
        RecentContext(nullptr /* file_system_context */, GURL() /* origin */),
        base::BindOnce(
            [](base::RunLoop* run_loop,
               std::vector<storage::FileSystemURL>* out_files,
               std::vector<storage::FileSystemURL> files) {
              run_loop->Quit();
              *out_files = std::move(files);
            },
            &run_loop, &files));

    run_loop.Run();

    return files;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  arc::FakeFileSystemInstance fake_file_system_;

  // Use the same initialization/destruction order as
  // ChromeBrowserMainPartsChromeos.
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<RecentArcMediaSource> source_;
};

TEST_F(RecentArcMediaSourceTest, GetRecentFiles) {
  EnableFakeFileSystemInstance();

  std::vector<storage::FileSystemURL> files = GetRecentFiles();

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ(arc::GetDocumentsProviderMountPath(kMediaDocumentsProviderAuthority,
                                               kImagesRootId)
                .Append("cat.png"),
            files[0].path());
  EXPECT_EQ(arc::GetDocumentsProviderMountPath(kMediaDocumentsProviderAuthority,
                                               kImagesRootId)
                .Append("dog.jpg"),
            files[1].path());
}

TEST_F(RecentArcMediaSourceTest, GetRecentFiles_ArcNotAvailable) {
  std::vector<storage::FileSystemURL> files = GetRecentFiles();

  EXPECT_EQ(0u, files.size());
}

}  // namespace
}  // namespace chromeos
