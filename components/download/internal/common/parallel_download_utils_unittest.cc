// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/public/common/parallel_download_utils.h"

#include "components/download/public/common/mock_input_stream.h"
#include "components/download/public/common/parallel_download_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Return;
using ::testing::StrictMock;

namespace download {

namespace {

const int kErrorStreamOffset = 100;

}  // namespace

class ParallelDownloadUtilsTest : public testing::Test {};

class ParallelDownloadUtilsRecoverErrorTest
    : public ::testing::TestWithParam<int64_t> {
 public:
  ParallelDownloadUtilsRecoverErrorTest() : input_stream_(nullptr) {}

  // Creates a source stream to test.
  std::unique_ptr<DownloadFileImpl::SourceStream> CreateSourceStream(
      int64_t offset,
      int64_t length) {
    input_stream_ = new StrictMock<MockInputStream>();
    EXPECT_CALL(*input_stream_, GetCompletionStatus())
        .WillRepeatedly(Return(DOWNLOAD_INTERRUPT_REASON_NONE));
    return std::make_unique<DownloadFileImpl::SourceStream>(
        offset, length, std::unique_ptr<MockInputStream>(input_stream_));
  }

 protected:
  // Stream for sending data into the SourceStream.
  StrictMock<MockInputStream>* input_stream_;
};

TEST_F(ParallelDownloadUtilsTest, FindSlicesToDownload) {
  std::vector<DownloadItem::ReceivedSlice> downloaded_slices;
  std::vector<DownloadItem::ReceivedSlice> slices_to_download =
      FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(1u, slices_to_download.size());
  EXPECT_EQ(0, slices_to_download[0].offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent,
            slices_to_download[0].received_bytes);

  downloaded_slices.emplace_back(0, 500);
  slices_to_download = FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(1u, slices_to_download.size());
  EXPECT_EQ(500, slices_to_download[0].offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent,
            slices_to_download[0].received_bytes);

  // Create a gap between slices.
  downloaded_slices.emplace_back(1000, 500);
  slices_to_download = FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(2u, slices_to_download.size());
  EXPECT_EQ(500, slices_to_download[0].offset);
  EXPECT_EQ(500, slices_to_download[0].received_bytes);
  EXPECT_EQ(1500, slices_to_download[1].offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent,
            slices_to_download[1].received_bytes);

  // Fill the gap.
  downloaded_slices.emplace(downloaded_slices.begin() + 1,
                            slices_to_download[0]);
  slices_to_download = FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(1u, slices_to_download.size());
  EXPECT_EQ(1500, slices_to_download[0].offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent,
            slices_to_download[0].received_bytes);

  // Create a new gap at the beginning.
  downloaded_slices.erase(downloaded_slices.begin());
  slices_to_download = FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(2u, slices_to_download.size());
  EXPECT_EQ(0, slices_to_download[0].offset);
  EXPECT_EQ(500, slices_to_download[0].received_bytes);
  EXPECT_EQ(1500, slices_to_download[1].offset);
  EXPECT_EQ(DownloadSaveInfo::kLengthFullContent,
            slices_to_download[1].received_bytes);
}

TEST_F(ParallelDownloadUtilsTest, AddOrMergeReceivedSliceIntoSortedArray) {
  std::vector<DownloadItem::ReceivedSlice> slices;
  DownloadItem::ReceivedSlice slice1(500, 500);
  EXPECT_EQ(0u, AddOrMergeReceivedSliceIntoSortedArray(slice1, slices));
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(slice1, slices[0]);

  // Adding a slice that can be merged with existing slice.
  DownloadItem::ReceivedSlice slice2(1000, 400);
  EXPECT_EQ(0u, AddOrMergeReceivedSliceIntoSortedArray(slice2, slices));
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(500, slices[0].offset);
  EXPECT_EQ(900, slices[0].received_bytes);

  DownloadItem::ReceivedSlice slice3(0, 50);
  EXPECT_EQ(0u, AddOrMergeReceivedSliceIntoSortedArray(slice3, slices));
  EXPECT_EQ(2u, slices.size());
  EXPECT_EQ(slice3, slices[0]);

  DownloadItem::ReceivedSlice slice4(100, 50);
  EXPECT_EQ(1u, AddOrMergeReceivedSliceIntoSortedArray(slice4, slices));
  EXPECT_EQ(3u, slices.size());
  EXPECT_EQ(slice3, slices[0]);
  EXPECT_EQ(slice4, slices[1]);

  // A new slice can only merge with an existing slice earlier in the file, not
  // later in the file.
  DownloadItem::ReceivedSlice slice5(50, 50);
  EXPECT_EQ(0u, AddOrMergeReceivedSliceIntoSortedArray(slice5, slices));
  EXPECT_EQ(3u, slices.size());
  EXPECT_EQ(0, slices[0].offset);
  EXPECT_EQ(100, slices[0].received_bytes);
  EXPECT_EQ(slice4, slices[1]);
}

// Verify if a preceding stream can recover the download for half open error
// stream(the current last stream).
TEST_P(ParallelDownloadUtilsRecoverErrorTest,
       RecoverErrorForHalfOpenErrorStream) {
  // Create a stream that will work on byte range "100-".
  const int kErrorStreamOffset = 100;

  auto error_stream = CreateSourceStream(kErrorStreamOffset,
                                         DownloadSaveInfo::kLengthFullContent);
  error_stream->set_finished(true);

  // Get starting offset of preceding stream.
  int64_t preceding_offset = GetParam();
  EXPECT_LT(preceding_offset, kErrorStreamOffset);
  auto preceding_stream = CreateSourceStream(
      preceding_offset, DownloadSaveInfo::kLengthFullContent);
  EXPECT_FALSE(preceding_stream->is_finished());
  EXPECT_EQ(0u, preceding_stream->bytes_written());
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // Half open finished preceding stream with 0 bytes written, if there is no
  // error, the download should be finished.
  preceding_stream->set_finished(true);
  EXPECT_EQ(DOWNLOAD_INTERRUPT_REASON_NONE,
            preceding_stream->GetCompletionStatus());
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // Half open finished preceding stream with error, should be treated as
  // failed.
  EXPECT_CALL(*input_stream_, GetCompletionStatus())
      .WillRepeatedly(Return(DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE));
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // Even if it has written some data.
  preceding_stream->OnWriteBytesToDisk(1000u);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // Now capped the length of preceding stream with different values.
  preceding_stream = CreateSourceStream(preceding_offset,
                                        kErrorStreamOffset - preceding_offset);
  // Since preceding stream can't reach the first byte of the error stream, it
  // will fail.
  preceding_stream->set_finished(false);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->set_finished(true);
  preceding_stream->OnWriteBytesToDisk(kErrorStreamOffset - preceding_offset);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // Inject an error results in failure, even if data written exceeds the first
  // byte of error stream.
  EXPECT_CALL(*input_stream_, GetCompletionStatus())
      .WillRepeatedly(Return(DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE));
  preceding_stream->OnWriteBytesToDisk(1000u);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // Make preceding stream can reach the first byte of error stream.
  preceding_stream = CreateSourceStream(
      preceding_offset, kErrorStreamOffset - preceding_offset + 1);
  // Since the error stream is half opened, no matter what it should fail.
  preceding_stream->set_finished(false);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->set_finished(true);
  preceding_stream->OnWriteBytesToDisk(kErrorStreamOffset - preceding_offset);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->OnWriteBytesToDisk(1);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // Preceding stream that never download data won't recover the error stream.
  preceding_stream = CreateSourceStream(preceding_offset, -1);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
}

// Verify recovery for length capped error stream.
// Since the error stream length is capped, assume the previous stream length
// is also capped or the previous stream is finished due to error like http
// 404.
TEST_P(ParallelDownloadUtilsRecoverErrorTest,
       RecoverErrorForLengthCappedErrorStream) {
  // Create a stream that will work on byte range "100-150".
  const int kErrorStreamLength = 50;
  auto error_stream =
      CreateSourceStream(kErrorStreamOffset, kErrorStreamLength);
  error_stream->set_finished(true);

  // Get starting offset of preceding stream.
  const int64_t preceding_offset = GetParam();
  EXPECT_LT(preceding_offset, kErrorStreamOffset);

  // Create preceding stream capped before starting offset of error stream.
  auto preceding_stream = CreateSourceStream(
      preceding_offset, kErrorStreamOffset - preceding_offset);
  EXPECT_FALSE(preceding_stream->is_finished());
  EXPECT_EQ(0u, preceding_stream->bytes_written());

  // Since the preceding stream can never reach the starting offset, for an
  // unfinished stream, we rely on length instead of bytes written.
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->OnWriteBytesToDisk(kErrorStreamOffset - preceding_offset);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->OnWriteBytesToDisk(kErrorStreamLength - 1);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->OnWriteBytesToDisk(1);

  // Create preceding stream that can reach the upper bound of error stream.
  // Since it's unfinished, it potentially can take over error stream's work
  // even if no data is written.
  preceding_stream = CreateSourceStream(
      preceding_offset,
      kErrorStreamOffset - preceding_offset + kErrorStreamLength);
  EXPECT_FALSE(preceding_stream->is_finished());
  EXPECT_EQ(0u, preceding_stream->bytes_written());
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // Finished preceding stream only checks data written.
  preceding_stream = CreateSourceStream(preceding_offset, 1);
  preceding_stream->set_finished(true);
  preceding_stream->OnWriteBytesToDisk(kErrorStreamOffset - preceding_offset);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->OnWriteBytesToDisk(kErrorStreamLength - 1);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
  preceding_stream->OnWriteBytesToDisk(1);
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // Even if inject an error, since data written has cover the upper bound of
  // the error stream, it should succeed.
  EXPECT_CALL(*input_stream_, GetCompletionStatus())
      .WillRepeatedly(Return(DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE));
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // Preceding stream that never download data won't recover the error stream.
  preceding_stream = CreateSourceStream(preceding_offset, -1);
  EXPECT_FALSE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));
}

// The testing value specified offset for preceding stream. The error stream
// offset is fixed value.
INSTANTIATE_TEST_CASE_P(ParallelDownloadUtilsTestSuite,
                        ParallelDownloadUtilsRecoverErrorTest,
                        ::testing::Values(0, 20, 80));

}  // namespace download
