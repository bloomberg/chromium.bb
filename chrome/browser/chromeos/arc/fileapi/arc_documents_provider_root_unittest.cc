// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_root.h"
#include "chrome/browser/chromeos/arc/fileapi/arc_documents_provider_util.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/file_system/test/fake_arc_file_system_operation_runner.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/common/fileapi/directory_entry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using storage::DirectoryEntry;
using EntryList = storage::AsyncFileUtil::EntryList;

namespace arc {

namespace {

struct DocumentSpec {
  const char* document_id;
  const char* display_name;
  const char* mime_type;
  int64_t size;
  uint64_t last_modified;
};

// Fake file system hierarchy:
//
// <path>            <type>      <ID>
// (root)/           dir         root-id
//   dir/            dir         dir-id
//     photo.jpg     image/jpeg  photo-id
//     music.bin     audio/mp3   music-id
//   dups/           dir         dups-id
//     dup.mp4       video/mp4   dup1-id
//     dup.mp4       video/mp4   dup2-id
//     dup.mp4       video/mp4   dup3-id
//     dup.mp4       video/mp4   dup4-id
constexpr char kAuthority[] = "org.chromium.test";
// NOTE: ArcDocumentsProviderRoot::GetFileInfo() returns hard-coded info
// for root documents.
constexpr DocumentSpec kRootSpec = {"root-id", nullptr /* not used */,
                                    kAndroidDirectoryMimeType, -1, 0};
constexpr DocumentSpec kDirSpec = {"dir-id", "dir", kAndroidDirectoryMimeType,
                                   -1, 22};
constexpr DocumentSpec kPhotoSpec = {"photo-id", "photo.jpg", "image/jpeg", 3,
                                     33};
constexpr DocumentSpec kMusicSpec = {"music-id", "music.bin", "audio/mp3", 4,
                                     44};
constexpr DocumentSpec kDupsSpec = {"dups-id", "dups",
                                    kAndroidDirectoryMimeType, -1, 55};
constexpr DocumentSpec kDup1Spec = {"dup1-id", "dup.mp4", "video/mp4", 6, 66};
constexpr DocumentSpec kDup2Spec = {"dup2-id", "dup.mp4", "video/mp4", 7, 77};
constexpr DocumentSpec kDup3Spec = {"dup3-id", "dup.mp4", "video/mp4", 8, 88};
constexpr DocumentSpec kDup4Spec = {"dup4-id", "dup.mp4", "video/mp4", 9, 99};
constexpr DocumentSpec kAllSpecs[] = {kRootSpec,  kDirSpec,  kPhotoSpec,
                                      kMusicSpec, kDupsSpec, kDup1Spec,
                                      kDup2Spec,  kDup3Spec, kDup4Spec};

mojom::DocumentPtr MakeDocument(const DocumentSpec& spec) {
  mojom::DocumentPtr document = mojom::Document::New();
  document->document_id = spec.document_id;
  document->display_name = spec.display_name;
  document->mime_type = spec.mime_type;
  document->size = spec.size;
  document->last_modified = spec.last_modified;
  return document;
}

// TODO(crbug.com/683049): Use a generic FakeArcFileSystemOperationRunner.
class ArcFileSystemOperationRunnerForTest
    : public FakeArcFileSystemOperationRunner {
 public:
  explicit ArcFileSystemOperationRunnerForTest(ArcBridgeService* bridge_service)
      : FakeArcFileSystemOperationRunner(bridge_service) {}
  ~ArcFileSystemOperationRunnerForTest() override = default;

  void GetChildDocuments(const std::string& authority,
                         const std::string& document_id,
                         const GetChildDocumentsCallback& callback) override {
    EXPECT_EQ(kAuthority, authority);
    base::Optional<std::vector<mojom::DocumentPtr>> result;
    if (document_id == kRootSpec.document_id) {
      result.emplace();
      result.value().emplace_back(MakeDocument(kDirSpec));
      result.value().emplace_back(MakeDocument(kDupsSpec));
    } else if (document_id == kDirSpec.document_id) {
      result.emplace();
      result.value().emplace_back(MakeDocument(kPhotoSpec));
      result.value().emplace_back(MakeDocument(kMusicSpec));
    } else if (document_id == kDupsSpec.document_id) {
      result.emplace();
      // The order is intentionally shuffled.
      result.value().emplace_back(MakeDocument(kDup2Spec));
      result.value().emplace_back(MakeDocument(kDup1Spec));
      result.value().emplace_back(MakeDocument(kDup4Spec));
      result.value().emplace_back(MakeDocument(kDup3Spec));
    }
    callback.Run(std::move(result));
  }

  void GetDocument(const std::string& authority,
                   const std::string& document_id,
                   const GetDocumentCallback& callback) override {
    EXPECT_EQ(kAuthority, authority);
    mojom::DocumentPtr result;
    for (const auto& spec : kAllSpecs) {
      if (document_id == spec.document_id) {
        result = MakeDocument(spec);
        break;
      }
    }
    callback.Run(std::move(result));
  }
};

void ExpectMatchesSpec(const base::File::Info& info, const DocumentSpec& spec) {
  EXPECT_EQ(spec.size, info.size);
  if (strcmp(spec.mime_type, kAndroidDirectoryMimeType) == 0) {
    EXPECT_TRUE(info.is_directory);
  } else {
    EXPECT_FALSE(info.is_directory);
  }
  EXPECT_FALSE(info.is_symbolic_link);
  EXPECT_EQ(spec.last_modified,
            static_cast<uint64_t>(info.last_modified.ToJavaTime()));
  EXPECT_EQ(spec.last_modified,
            static_cast<uint64_t>(info.last_accessed.ToJavaTime()));
  EXPECT_EQ(spec.last_modified,
            static_cast<uint64_t>(info.creation_time.ToJavaTime()));
}

class ArcDocumentsProviderRootTest : public testing::Test {
 public:
  ArcDocumentsProviderRootTest()
      : arc_service_manager_(base::MakeUnique<ArcServiceManager>(nullptr)),
        root_(
            base::MakeUnique<ArcDocumentsProviderRoot>(kAuthority,
                                                       kRootSpec.document_id)) {
    arc_service_manager_->AddService(
        base::MakeUnique<ArcFileSystemOperationRunnerForTest>(
            arc_service_manager_->arc_bridge_service()));
  }

  ~ArcDocumentsProviderRootTest() override = default;

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<ArcServiceManager> arc_service_manager_;
  std::unique_ptr<ArcDocumentsProviderRoot> root_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcDocumentsProviderRootTest);
};

}  // namespace

TEST_F(ArcDocumentsProviderRootTest, GetFileInfo) {
  base::RunLoop run_loop;
  root_->GetFileInfo(base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
                     base::Bind(
                         [](base::RunLoop* run_loop, base::File::Error error,
                            const base::File::Info& info) {
                           EXPECT_EQ(base::File::FILE_OK, error);
                           ExpectMatchesSpec(info, kPhotoSpec);
                           run_loop->Quit();
                         },
                         &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, GetFileInfoDirectory) {
  base::RunLoop run_loop;
  root_->GetFileInfo(base::FilePath(FILE_PATH_LITERAL("dir")),
                     base::Bind(
                         [](base::RunLoop* run_loop, base::File::Error error,
                            const base::File::Info& info) {
                           EXPECT_EQ(base::File::FILE_OK, error);
                           ExpectMatchesSpec(info, kDirSpec);
                           run_loop->Quit();
                         },
                         &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, GetFileInfoRoot) {
  base::RunLoop run_loop;
  root_->GetFileInfo(base::FilePath(FILE_PATH_LITERAL("")),
                     base::Bind(
                         [](base::RunLoop* run_loop, base::File::Error error,
                            const base::File::Info& info) {
                           EXPECT_EQ(base::File::FILE_OK, error);
                           ExpectMatchesSpec(info, kRootSpec);
                           run_loop->Quit();
                         },
                         &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, GetFileInfoNoSuchFile) {
  base::RunLoop run_loop;
  root_->GetFileInfo(base::FilePath(FILE_PATH_LITERAL("dir/missing.jpg")),
                     base::Bind(
                         [](base::RunLoop* run_loop, base::File::Error error,
                            const base::File::Info& info) {
                           EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, error);
                           run_loop->Quit();
                         },
                         &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, GetFileInfoDups) {
  base::RunLoop run_loop;
  // "dup (2).mp4" should map to the 3rd instance of "dup.mp4" regardless of the
  // order returned from FileSystemInstance.
  root_->GetFileInfo(base::FilePath(FILE_PATH_LITERAL("dups/dup (2).mp4")),
                     base::Bind(
                         [](base::RunLoop* run_loop, base::File::Error error,
                            const base::File::Info& info) {
                           EXPECT_EQ(base::File::FILE_OK, error);
                           ExpectMatchesSpec(info, kDup3Spec);
                           run_loop->Quit();
                         },
                         &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, ReadDirectory) {
  base::RunLoop run_loop;
  root_->ReadDirectory(
      base::FilePath(FILE_PATH_LITERAL("dir")),
      base::Bind(
          [](base::RunLoop* run_loop, base::File::Error error,
             const EntryList& file_list, bool has_more) {
            EXPECT_EQ(base::File::FILE_OK, error);
            ASSERT_EQ(2u, file_list.size());
            EXPECT_EQ(FILE_PATH_LITERAL("music.bin.mp3"), file_list[0].name);
            EXPECT_FALSE(file_list[0].is_directory);
            EXPECT_EQ(FILE_PATH_LITERAL("photo.jpg"), file_list[1].name);
            EXPECT_FALSE(file_list[1].is_directory);
            EXPECT_FALSE(has_more);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, ReadDirectoryRoot) {
  base::RunLoop run_loop;
  root_->ReadDirectory(
      base::FilePath(FILE_PATH_LITERAL("")),
      base::Bind(
          [](base::RunLoop* run_loop, base::File::Error error,
             const EntryList& file_list, bool has_more) {
            EXPECT_EQ(base::File::FILE_OK, error);
            ASSERT_EQ(2u, file_list.size());
            EXPECT_EQ(FILE_PATH_LITERAL("dir"), file_list[0].name);
            EXPECT_TRUE(file_list[0].is_directory);
            EXPECT_EQ(FILE_PATH_LITERAL("dups"), file_list[1].name);
            EXPECT_TRUE(file_list[1].is_directory);
            EXPECT_FALSE(has_more);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, ReadDirectoryNoSuchDirectory) {
  base::RunLoop run_loop;
  root_->ReadDirectory(base::FilePath(FILE_PATH_LITERAL("missing")),
                       base::Bind(
                           [](base::RunLoop* run_loop, base::File::Error error,
                              const EntryList& file_list, bool has_more) {
                             EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, error);
                             EXPECT_EQ(0u, file_list.size());
                             EXPECT_FALSE(has_more);
                             run_loop->Quit();
                           },
                           &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, ReadDirectoryDups) {
  base::RunLoop run_loop;
  root_->ReadDirectory(
      base::FilePath(FILE_PATH_LITERAL("dups")),
      base::Bind(
          [](base::RunLoop* run_loop, base::File::Error error,
             const EntryList& file_list, bool has_more) {
            EXPECT_EQ(base::File::FILE_OK, error);
            ASSERT_EQ(4u, file_list.size());
            // FiIles are sorted lexicographically.
            EXPECT_EQ(FILE_PATH_LITERAL("dup (1).mp4"), file_list[0].name);
            EXPECT_FALSE(file_list[0].is_directory);
            EXPECT_EQ(FILE_PATH_LITERAL("dup (2).mp4"), file_list[1].name);
            EXPECT_FALSE(file_list[1].is_directory);
            EXPECT_EQ(FILE_PATH_LITERAL("dup (3).mp4"), file_list[2].name);
            EXPECT_FALSE(file_list[2].is_directory);
            EXPECT_EQ(FILE_PATH_LITERAL("dup.mp4"), file_list[3].name);
            EXPECT_FALSE(file_list[3].is_directory);
            EXPECT_FALSE(has_more);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, ResolveToContentUrl) {
  base::RunLoop run_loop;
  root_->ResolveToContentUrl(
      base::FilePath(FILE_PATH_LITERAL("dir/photo.jpg")),
      base::Bind(
          [](base::RunLoop* run_loop, const GURL& url) {
            EXPECT_EQ(GURL("content://org.chromium.test/document/photo-id"),
                      url);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, ResolveToContentUrlRoot) {
  base::RunLoop run_loop;
  root_->ResolveToContentUrl(
      base::FilePath(FILE_PATH_LITERAL("")),
      base::Bind(
          [](base::RunLoop* run_loop, const GURL& url) {
            EXPECT_EQ(GURL("content://org.chromium.test/document/root-id"),
                      url);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, ResolveToContentUrlNoSuchFile) {
  base::RunLoop run_loop;
  root_->ResolveToContentUrl(base::FilePath(FILE_PATH_LITERAL("missing")),
                             base::Bind(
                                 [](base::RunLoop* run_loop, const GURL& url) {
                                   EXPECT_EQ(GURL(), url);
                                   run_loop->Quit();
                                 },
                                 &run_loop));
  run_loop.Run();
}

TEST_F(ArcDocumentsProviderRootTest, ResolveToContentUrlDups) {
  base::RunLoop run_loop;
  // "dup 2.mp4" should map to the 3rd instance of "dup.mp4" regardless of the
  // order returned from FileSystemInstance.
  root_->ResolveToContentUrl(
      base::FilePath(FILE_PATH_LITERAL("dups/dup (2).mp4")),
      base::Bind(
          [](base::RunLoop* run_loop, const GURL& url) {
            EXPECT_EQ(GURL("content://org.chromium.test/document/dup3-id"),
                      url);
            run_loop->Quit();
          },
          &run_loop));
  run_loop.Run();
}

}  // namespace arc
