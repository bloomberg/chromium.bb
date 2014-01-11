// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/quota_reservation.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/quota/quota_reservation.h"

using fileapi::FileSystemType;
using fileapi::QuotaReservationManager;

namespace content {

namespace {

const char kOrigin[] = "http://example.com";
const FileSystemType kType = fileapi::kFileSystemTypeTemporary;

const base::FilePath::StringType file1_name = FILE_PATH_LITERAL("file1");
const base::FilePath::StringType file2_name = FILE_PATH_LITERAL("file2");
const base::FilePath::StringType file3_name = FILE_PATH_LITERAL("file3");
const int kFile1ID = 1;
const int kFile2ID = 2;
const int kFile3ID = 3;

class FakeBackend : public QuotaReservationManager::QuotaBackend {
 public:
  FakeBackend() {}
  virtual ~FakeBackend() {}

  virtual void ReserveQuota(
      const GURL& origin,
      FileSystemType type,
      int64 delta,
      const QuotaReservationManager::ReserveQuotaCallback& callback) OVERRIDE {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(base::IgnoreResult(callback), base::PLATFORM_FILE_OK));
  }

  virtual void ReleaseReservedQuota(const GURL& origin,
                                    FileSystemType type,
                                    int64 size) OVERRIDE {
  }

  virtual void CommitQuotaUsage(const GURL& origin,
                                FileSystemType type,
                                int64 delta) OVERRIDE {
  }

  virtual void IncrementDirtyCount(const GURL& origin,
                                   FileSystemType type) OVERRIDE {}
  virtual void DecrementDirtyCount(const GURL& origin,
                                   FileSystemType type) OVERRIDE {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeBackend);
};

}  // namespace

class QuotaReservationTest : public testing::Test {
 public:
  QuotaReservationTest() {}
  virtual ~QuotaReservationTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(work_dir_.CreateUniqueTempDir());

    reservation_manager_.reset(new QuotaReservationManager(
        scoped_ptr<QuotaReservationManager::QuotaBackend>(new FakeBackend)));
  }

  virtual void TearDown() OVERRIDE {
    reservation_manager_.reset();
  }

  base::FilePath MakeFilePath(const base::FilePath::StringType& file_name) {
    return work_dir_.path().Append(file_name);
  }

  fileapi::FileSystemURL MakeFileSystemURL(
      const base::FilePath::StringType& file_name) {
    return fileapi::FileSystemURL::CreateForTest(
        GURL(kOrigin), kType, MakeFilePath(file_name));
  }

  scoped_refptr<QuotaReservation> CreateQuotaReservation(
      scoped_refptr<fileapi::QuotaReservation> reservation,
      const GURL& origin,
      FileSystemType type) {
    // Sets reservation_ as a side effect.
    return scoped_refptr<QuotaReservation>(
        new QuotaReservation(reservation, origin, type));
  }

  void SetFileSize(const base::FilePath::StringType& file_name, int64 size) {
    bool created = false;
    base::PlatformFileError error = base::PLATFORM_FILE_ERROR_FAILED;
    base::PlatformFile file = CreatePlatformFile(
        MakeFilePath(file_name),
        base::PLATFORM_FILE_OPEN_ALWAYS | base::PLATFORM_FILE_WRITE,
        &created, &error);
    ASSERT_EQ(base::PLATFORM_FILE_OK, error);
    ASSERT_TRUE(base::TruncatePlatformFile(file, size));
    ASSERT_TRUE(base::ClosePlatformFile(file));
  }

  QuotaReservationManager* reservation_manager() {
    return reservation_manager_.get();
  }

 private:
  base::MessageLoop message_loop_;
  base::ScopedTempDir work_dir_;
  scoped_ptr<fileapi::QuotaReservationManager> reservation_manager_;

  DISALLOW_COPY_AND_ASSIGN(QuotaReservationTest);
};

void GotReservedQuota(
    int64* reserved_quota_ptr,
    QuotaReservation::OffsetMap* maximum_written_offsets_ptr,
    int64 reserved_quota,
    const QuotaReservation::OffsetMap& maximum_written_offsets) {
  *reserved_quota_ptr = reserved_quota;
  *maximum_written_offsets_ptr = maximum_written_offsets;
}

void ReserveQuota(
    scoped_refptr<QuotaReservation> quota_reservation,
    int64 amount,
    int64* reserved_quota,
    QuotaReservation::OffsetMap* max_written_offsets) {
  quota_reservation->ReserveQuota(amount,
                                  *max_written_offsets,
                                  base::Bind(&GotReservedQuota,
                                             reserved_quota,
                                             max_written_offsets));
  base::RunLoop().RunUntilIdle();
}

// Tests that:
// 1) We can reserve quota with no files open.
// 2) Open a file, grow it, close it, and reserve quota with correct sizes.
TEST_F(QuotaReservationTest, DISABLED_ReserveQuota) {
  GURL origin(kOrigin);
  FileSystemType type = kType;

  scoped_refptr<fileapi::QuotaReservation> reservation(
     reservation_manager()->CreateReservation(origin, type));
  scoped_refptr<QuotaReservation> test =
      CreateQuotaReservation(reservation, origin, type);

  // Reserve quota with no files open.
  int64 amount = 100;
  int64 reserved_quota;
  QuotaReservation::OffsetMap max_written_offsets;
  ReserveQuota(test, amount, &reserved_quota, &max_written_offsets);
  EXPECT_EQ(amount, reserved_quota);
  EXPECT_EQ(0U, max_written_offsets.size());

  // Open a file, refresh the reservation, extend the file, and close it.
  int64 file_size = 10;
  SetFileSize(file1_name, file_size);
  int64 open_file_size = test->OpenFile(kFile1ID,
                                        MakeFileSystemURL(file1_name));
  EXPECT_EQ(file_size, open_file_size);

  max_written_offsets[kFile1ID] = file_size;  // 1 file open.
  ReserveQuota(test, amount, &reserved_quota, &max_written_offsets);
  EXPECT_EQ(amount, reserved_quota);
  EXPECT_EQ(1U, max_written_offsets.size());
  EXPECT_EQ(file_size, max_written_offsets[kFile1ID]);

  int64 new_file_size = 30;
  SetFileSize(file1_name, new_file_size);

  EXPECT_EQ(amount, reservation->remaining_quota());
  test->CloseFile(kFile1ID, new_file_size);
  EXPECT_EQ(amount - (new_file_size - file_size),
            reservation->remaining_quota());
}

// Tests that:
// 1) We can open and close multiple files.
TEST_F(QuotaReservationTest, DISABLED_MultipleFiles) {
  GURL origin(kOrigin);
  FileSystemType type = kType;

  scoped_refptr<fileapi::QuotaReservation> reservation(
     reservation_manager()->CreateReservation(origin, type));
  scoped_refptr<QuotaReservation> test =
      CreateQuotaReservation(reservation, origin, type);

  // Open some files of different sizes.
  int64 file1_size = 10;
  SetFileSize(file1_name, file1_size);
  int64 open_file1_size = test->OpenFile(kFile1ID,
                                         MakeFileSystemURL(file1_name));
  EXPECT_EQ(file1_size, open_file1_size);
  int64 file2_size = 20;
  SetFileSize(file2_name, file2_size);
  int64 open_file2_size = test->OpenFile(kFile2ID,
                                         MakeFileSystemURL(file2_name));
  EXPECT_EQ(file2_size, open_file2_size);
  int64 file3_size = 30;
  SetFileSize(file3_name, file3_size);
  int64 open_file3_size = test->OpenFile(kFile3ID,
                                         MakeFileSystemURL(file3_name));
  EXPECT_EQ(file3_size, open_file3_size);

  // Reserve quota.
  int64 amount = 100;
  int64 reserved_quota;
  QuotaReservation::OffsetMap max_written_offsets;
  max_written_offsets[kFile1ID] = file1_size;  // 3 files open.
  max_written_offsets[kFile2ID] = file2_size;
  max_written_offsets[kFile3ID] = file3_size;

  ReserveQuota(test, amount, &reserved_quota, &max_written_offsets);
  EXPECT_EQ(amount, reserved_quota);
  EXPECT_EQ(3U, max_written_offsets.size());
  EXPECT_EQ(file1_size, max_written_offsets[kFile1ID]);
  EXPECT_EQ(file2_size, max_written_offsets[kFile2ID]);
  EXPECT_EQ(file3_size, max_written_offsets[kFile3ID]);

  test->CloseFile(kFile2ID, file2_size);

  max_written_offsets.erase(max_written_offsets.find(kFile2ID));
  ReserveQuota(test, amount, &reserved_quota, &max_written_offsets);
  EXPECT_EQ(amount, reserved_quota);
  EXPECT_EQ(2U, max_written_offsets.size());
  EXPECT_EQ(file1_size, max_written_offsets[kFile1ID]);
  EXPECT_EQ(file3_size, max_written_offsets[kFile3ID]);

  test->CloseFile(kFile1ID, file1_size);
  test->CloseFile(kFile3ID, file3_size);
}

}  // namespace content
