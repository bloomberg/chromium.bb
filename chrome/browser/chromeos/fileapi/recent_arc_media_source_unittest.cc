// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_mounter.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_file_system_operation_runner.h"
#include "chrome/browser/chromeos/fileapi/recent_arc_media_source.h"
#include "chrome/browser/chromeos/fileapi/recent_context.h"
#include "chrome/browser/chromeos/fileapi/recent_file.h"
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
    const std::string& mime_type,
    const base::Time& last_modified) {
  return arc::FakeFileSystemInstance::Document(
      kMediaDocumentsProviderAuthority,  // authority
      document_id,                       // document_id
      parent_document_id,                // parent_document_id
      display_name,                      // display_name
      mime_type,                         // mime_type
      0,                                 // size
      last_modified.ToJavaTime());       // last_modified
}

}  // namespace

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
        MakeDocument(kImagesRootId, "", "", arc::kAndroidDirectoryMimeType,
                     base::Time::FromJavaTime(1));
    auto cat_doc = MakeDocument("cat", kImagesRootId, "cat.png", "image/png",
                                base::Time::FromJavaTime(2));
    auto dog_doc = MakeDocument("dog", kImagesRootId, "dog.jpg", "image/jpeg",
                                base::Time::FromJavaTime(3));
    auto fox_doc = MakeDocument("fox", kImagesRootId, "fox.gif", "image/gif",
                                base::Time::FromJavaTime(4));
    auto elk_doc = MakeDocument("elk", kImagesRootId, "elk.tiff", "image/tiff",
                                base::Time::FromJavaTime(5));
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

  std::vector<RecentFile> GetRecentFiles() {
    std::vector<RecentFile> files;

    base::RunLoop run_loop;

    source_->GetRecentFiles(
        RecentContext(nullptr /* file_system_context */, GURL() /* origin */),
        base::BindOnce(
            [](base::RunLoop* run_loop, std::vector<RecentFile>* out_files,
               std::vector<RecentFile> files) {
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

  std::vector<RecentFile> files = GetRecentFiles();

  ASSERT_EQ(2u, files.size());
  EXPECT_EQ(arc::GetDocumentsProviderMountPath(kMediaDocumentsProviderAuthority,
                                               kImagesRootId)
                .Append("cat.png"),
            files[0].url().path());
  EXPECT_EQ(base::Time::FromJavaTime(2), files[0].last_modified());
  EXPECT_EQ(arc::GetDocumentsProviderMountPath(kMediaDocumentsProviderAuthority,
                                               kImagesRootId)
                .Append("dog.jpg"),
            files[1].url().path());
  EXPECT_EQ(base::Time::FromJavaTime(3), files[1].last_modified());
}

TEST_F(RecentArcMediaSourceTest, GetRecentFiles_ArcNotAvailable) {
  std::vector<RecentFile> files = GetRecentFiles();

  EXPECT_EQ(0u, files.size());
}

TEST_F(RecentArcMediaSourceTest, GetRecentFiles_UmaStats) {
  base::HistogramTester histogram_tester;

  GetRecentFiles();

  histogram_tester.ExpectTotalCount(RecentArcMediaSource::kLoadHistogramName,
                                    1);
}

}  // namespace chromeos
