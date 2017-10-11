// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_storage_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "media/mojo/interfaces/cdm_storage.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using media::mojom::CdmStorage;

namespace content {

namespace {

const char kTestFileSystemId[] = "test_file_system";
const char kTestOrigin[] = "http://www.test.com";

// Helper functions to manipulate RenderFrameHosts.

void SimulateNavigation(RenderFrameHost** rfh, const GURL& url) {
  auto navigation_simulator =
      NavigationSimulator::CreateRendererInitiated(url, *rfh);
  navigation_simulator->Commit();
  *rfh = navigation_simulator->GetFinalRenderFrameHost();
}

}  // namespace

class CdmStorageTest : public RenderViewHostTestHarness {
 protected:
  void SetUp() final {
    RenderViewHostTestHarness::SetUp();
    rfh_ = web_contents()->GetMainFrame();
    RenderFrameHostTester::For(rfh_)->InitializeRenderFrameIfNeeded();
    SimulateNavigation(&rfh_, GURL(kTestOrigin));
  }

  // Creates and initializes the CdmStorage object using |file_system_id|.
  // Returns true if successful, false otherwise.
  void Initialize(const std::string& file_system_id) {
    DVLOG(3) << __func__;

    // Most tests don't do any I/O, but Open() returns a base::File which needs
    // to be closed.
    base::ThreadRestrictions::SetIOAllowed(true);

    // Create the CdmStorageImpl object. |cdm_storage_| will own the resulting
    // object.
    CdmStorageImpl::Create(rfh_, file_system_id,
                           mojo::MakeRequest(&cdm_storage_));
  }

  // Open the file |name|. Returns true if the file returned is valid, false
  // otherwise. Updates |status|, |file|, and |releaser| with the values
  // returned by CdmStorage. If |status| = kSuccess, |file| should be valid to
  // access, and |releaser| should be reset when the file is closed.
  bool Open(const std::string& name,
            CdmStorage::Status* status,
            base::File* file,
            media::mojom::CdmFileReleaserAssociatedPtr* releaser) {
    DVLOG(3) << __func__;

    cdm_storage_->Open(
        name, base::Bind(&CdmStorageTest::OpenDone, base::Unretained(this),
                         status, file, releaser));
    RunAndWaitForResult();
    return file->IsValid();
  }

 private:
  void OpenDone(
      CdmStorage::Status* status,
      base::File* file,
      media::mojom::CdmFileReleaserAssociatedPtr* releaser,
      CdmStorage::Status actual_status,
      base::File actual_file,
      media::mojom::CdmFileReleaserAssociatedPtrInfo actual_releaser) {
    DVLOG(3) << __func__;
    *status = actual_status;
    *file = std::move(actual_file);

    // Open() returns a CdmFileReleaserAssociatedPtrInfo, so bind it to the
    // CdmFileReleaserAssociatedPtr provided.
    media::mojom::CdmFileReleaserAssociatedPtr releaser_ptr;
    releaser_ptr.Bind(std::move(actual_releaser));
    *releaser = std::move(releaser_ptr);

    run_loop_->Quit();
  }

  // Start running and allow the asynchronous IO operations to complete.
  void RunAndWaitForResult() {
    run_loop_.reset(new base::RunLoop());
    run_loop_->Run();
  }

  RenderFrameHost* rfh_ = nullptr;
  media::mojom::CdmStoragePtr cdm_storage_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

TEST_F(CdmStorageTest, InvalidFileSystemIdWithSlash) {
  Initialize("name/");

  const char kFileName[] = "valid_file_name";
  CdmStorage::Status status;
  base::File file;
  media::mojom::CdmFileReleaserAssociatedPtr releaser;
  EXPECT_FALSE(Open(kFileName, &status, &file, &releaser));
  EXPECT_EQ(status, CdmStorage::Status::kFailure);
  EXPECT_FALSE(file.IsValid());
  EXPECT_FALSE(releaser.is_bound());
}

TEST_F(CdmStorageTest, InvalidFileSystemIdWithBackSlash) {
  Initialize("name\\");

  const char kFileName[] = "valid_file_name";
  CdmStorage::Status status;
  base::File file;
  media::mojom::CdmFileReleaserAssociatedPtr releaser;
  EXPECT_FALSE(Open(kFileName, &status, &file, &releaser));
  EXPECT_EQ(status, CdmStorage::Status::kFailure);
  EXPECT_FALSE(file.IsValid());
  EXPECT_FALSE(releaser.is_bound());
}

TEST_F(CdmStorageTest, InvalidFileSystemIdEmpty) {
  Initialize("");

  const char kFileName[] = "valid_file_name";
  CdmStorage::Status status;
  base::File file;
  media::mojom::CdmFileReleaserAssociatedPtr releaser;
  EXPECT_FALSE(Open(kFileName, &status, &file, &releaser));
  EXPECT_EQ(status, CdmStorage::Status::kFailure);
  EXPECT_FALSE(file.IsValid());
  EXPECT_FALSE(releaser.is_bound());
}

TEST_F(CdmStorageTest, InvalidFileNameEmpty) {
  Initialize(kTestFileSystemId);

  const char kFileName[] = "";
  CdmStorage::Status status;
  base::File file;
  media::mojom::CdmFileReleaserAssociatedPtr releaser;
  EXPECT_FALSE(Open(kFileName, &status, &file, &releaser));
  EXPECT_EQ(status, CdmStorage::Status::kFailure);
  EXPECT_FALSE(file.IsValid());
  EXPECT_FALSE(releaser.is_bound());
}

TEST_F(CdmStorageTest, OpenFile) {
  Initialize(kTestFileSystemId);

  const char kFileName[] = "test_file_name";
  CdmStorage::Status status;
  base::File file;
  media::mojom::CdmFileReleaserAssociatedPtr releaser;
  EXPECT_TRUE(Open(kFileName, &status, &file, &releaser));
  EXPECT_EQ(status, CdmStorage::Status::kSuccess);
  EXPECT_TRUE(file.IsValid());
  EXPECT_TRUE(releaser.is_bound());
}

TEST_F(CdmStorageTest, OpenFileLocked) {
  Initialize(kTestFileSystemId);

  const char kFileName[] = "test_file_name";
  CdmStorage::Status status;
  base::File file1;
  media::mojom::CdmFileReleaserAssociatedPtr releaser1;
  EXPECT_TRUE(Open(kFileName, &status, &file1, &releaser1));
  EXPECT_EQ(status, CdmStorage::Status::kSuccess);
  EXPECT_TRUE(file1.IsValid());
  EXPECT_TRUE(releaser1.is_bound());

  // Second attempt on the same file should fail as the file is locked.
  base::File file2;
  media::mojom::CdmFileReleaserAssociatedPtr releaser2;
  EXPECT_FALSE(Open(kFileName, &status, &file2, &releaser2));
  EXPECT_EQ(status, CdmStorage::Status::kInUse);
  EXPECT_FALSE(file2.IsValid());
  EXPECT_FALSE(releaser2.is_bound());

  // Now close the first file and try again. It should be free now.
  file1.Close();
  releaser1.reset();

  base::File file3;
  media::mojom::CdmFileReleaserAssociatedPtr releaser3;
  EXPECT_TRUE(Open(kFileName, &status, &file3, &releaser3));
  EXPECT_EQ(status, CdmStorage::Status::kSuccess);
  EXPECT_TRUE(file3.IsValid());
  EXPECT_TRUE(releaser3.is_bound());
}

TEST_F(CdmStorageTest, MultipleFiles) {
  Initialize(kTestFileSystemId);

  const char kFileName1[] = "file1";
  CdmStorage::Status status;
  base::File file1;
  media::mojom::CdmFileReleaserAssociatedPtr releaser1;
  EXPECT_TRUE(Open(kFileName1, &status, &file1, &releaser1));
  EXPECT_EQ(status, CdmStorage::Status::kSuccess);
  EXPECT_TRUE(file1.IsValid());
  EXPECT_TRUE(releaser1.is_bound());

  const char kFileName2[] = "file2";
  base::File file2;
  media::mojom::CdmFileReleaserAssociatedPtr releaser2;
  EXPECT_TRUE(Open(kFileName2, &status, &file2, &releaser2));
  EXPECT_EQ(status, CdmStorage::Status::kSuccess);
  EXPECT_TRUE(file2.IsValid());
  EXPECT_TRUE(releaser2.is_bound());

  const char kFileName3[] = "file3";
  base::File file3;
  media::mojom::CdmFileReleaserAssociatedPtr releaser3;
  EXPECT_TRUE(Open(kFileName3, &status, &file3, &releaser3));
  EXPECT_EQ(status, CdmStorage::Status::kSuccess);
  EXPECT_TRUE(file3.IsValid());
  EXPECT_TRUE(releaser3.is_bound());
}

TEST_F(CdmStorageTest, ReadWriteFile) {
  Initialize(kTestFileSystemId);

  const char kFileName[] = "test_file_name";
  CdmStorage::Status status;
  base::File file;
  media::mojom::CdmFileReleaserAssociatedPtr releaser;
  EXPECT_TRUE(Open(kFileName, &status, &file, &releaser));
  EXPECT_EQ(status, CdmStorage::Status::kSuccess);
  EXPECT_TRUE(file.IsValid());
  EXPECT_TRUE(releaser.is_bound());

  // Write several bytes and read them back.
  const char kTestData[] = "random string";
  const int kTestDataSize = sizeof(kTestData);
  EXPECT_EQ(kTestDataSize, file.Write(0, kTestData, kTestDataSize));

  char data_read[32];
  const int kTestDataReadSize = sizeof(data_read);
  EXPECT_GT(kTestDataReadSize, kTestDataSize);
  EXPECT_EQ(kTestDataSize, file.Read(0, data_read, kTestDataReadSize));
  for (size_t i = 0; i < kTestDataSize; i++)
    EXPECT_EQ(kTestData[i], data_read[i]);
}

}  // namespace content
