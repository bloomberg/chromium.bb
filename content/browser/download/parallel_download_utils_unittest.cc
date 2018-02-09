// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/parallel_download_utils.h"

#include <map>
#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/download/public/common/download_save_info.h"
#include "content/browser/byte_stream.h"
#include "content/public/browser/download_manager.h"
#include "content/public/common/content_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const int kErrorStreamOffset = 100;

class MockByteStreamReader : public ByteStreamReader {
 public:
  MockByteStreamReader() {}
  ~MockByteStreamReader() override {}

  // ByteStream functions
  MOCK_METHOD2(Read,
               ByteStreamReader::StreamState(scoped_refptr<net::IOBuffer>*,
                                             size_t*));
  MOCK_CONST_METHOD0(GetStatus, int());
  MOCK_METHOD1(RegisterCallback, void(const base::Closure&));
};

// Creates a source stream to test.
std::unique_ptr<DownloadFileImpl::SourceStream> CreateSourceStream(
    int64_t offset,
    int64_t length) {
  auto input_stream = std::make_unique<DownloadManager::InputStream>(
      std::make_unique<MockByteStreamReader>());
  return std::make_unique<DownloadFileImpl::SourceStream>(
      offset, length, std::move(input_stream));
}

}  // namespace

class ParallelDownloadUtilsTest : public testing::Test {};

class ParallelDownloadUtilsRecoverErrorTest
    : public ::testing::TestWithParam<int64_t> {};

TEST_F(ParallelDownloadUtilsTest, FindSlicesToDownload) {
  std::vector<download::DownloadItem::ReceivedSlice> downloaded_slices;
  std::vector<download::DownloadItem::ReceivedSlice> slices_to_download =
      FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(1u, slices_to_download.size());
  EXPECT_EQ(0, slices_to_download[0].offset);
  EXPECT_EQ(download::DownloadSaveInfo::kLengthFullContent,
            slices_to_download[0].received_bytes);

  downloaded_slices.emplace_back(0, 500);
  slices_to_download = FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(1u, slices_to_download.size());
  EXPECT_EQ(500, slices_to_download[0].offset);
  EXPECT_EQ(download::DownloadSaveInfo::kLengthFullContent,
            slices_to_download[0].received_bytes);

  // Create a gap between slices.
  downloaded_slices.emplace_back(1000, 500);
  slices_to_download = FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(2u, slices_to_download.size());
  EXPECT_EQ(500, slices_to_download[0].offset);
  EXPECT_EQ(500, slices_to_download[0].received_bytes);
  EXPECT_EQ(1500, slices_to_download[1].offset);
  EXPECT_EQ(download::DownloadSaveInfo::kLengthFullContent,
            slices_to_download[1].received_bytes);

  // Fill the gap.
  downloaded_slices.emplace(
      downloaded_slices.begin() + 1, slices_to_download[0]);
  slices_to_download = FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(1u, slices_to_download.size());
  EXPECT_EQ(1500, slices_to_download[0].offset);
  EXPECT_EQ(download::DownloadSaveInfo::kLengthFullContent,
            slices_to_download[0].received_bytes);

  // Create a new gap at the beginning.
  downloaded_slices.erase(downloaded_slices.begin());
  slices_to_download = FindSlicesToDownload(downloaded_slices);
  EXPECT_EQ(2u, slices_to_download.size());
  EXPECT_EQ(0, slices_to_download[0].offset);
  EXPECT_EQ(500, slices_to_download[0].received_bytes);
  EXPECT_EQ(1500, slices_to_download[1].offset);
  EXPECT_EQ(download::DownloadSaveInfo::kLengthFullContent,
            slices_to_download[1].received_bytes);
}

TEST_F(ParallelDownloadUtilsTest, AddOrMergeReceivedSliceIntoSortedArray) {
  std::vector<download::DownloadItem::ReceivedSlice> slices;
  download::DownloadItem::ReceivedSlice slice1(500, 500);
  EXPECT_EQ(0u, AddOrMergeReceivedSliceIntoSortedArray(slice1, slices));
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(slice1, slices[0]);

  // Adding a slice that can be merged with existing slice.
  download::DownloadItem::ReceivedSlice slice2(1000, 400);
  EXPECT_EQ(0u, AddOrMergeReceivedSliceIntoSortedArray(slice2, slices));
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(500, slices[0].offset);
  EXPECT_EQ(900, slices[0].received_bytes);

  download::DownloadItem::ReceivedSlice slice3(0, 50);
  EXPECT_EQ(0u, AddOrMergeReceivedSliceIntoSortedArray(slice3, slices));
  EXPECT_EQ(2u, slices.size());
  EXPECT_EQ(slice3, slices[0]);

  download::DownloadItem::ReceivedSlice slice4(100, 50);
  EXPECT_EQ(1u, AddOrMergeReceivedSliceIntoSortedArray(slice4, slices));
  EXPECT_EQ(3u, slices.size());
  EXPECT_EQ(slice3, slices[0]);
  EXPECT_EQ(slice4, slices[1]);

  // A new slice can only merge with an existing slice earlier in the file, not
  // later in the file.
  download::DownloadItem::ReceivedSlice slice5(50, 50);
  EXPECT_EQ(0u, AddOrMergeReceivedSliceIntoSortedArray(slice5, slices));
  EXPECT_EQ(3u, slices.size());
  EXPECT_EQ(0, slices[0].offset);
  EXPECT_EQ(100, slices[0].received_bytes);
  EXPECT_EQ(slice4, slices[1]);
}

// Ensure the minimum slice size is correctly applied.
TEST_F(ParallelDownloadUtilsTest, FindSlicesForRemainingContentMinSliceSize) {
  // Minimum slice size is smaller than total length, only one slice returned.
  download::DownloadItem::ReceivedSlices slices =
      FindSlicesForRemainingContent(0, 100, 3, 150);
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(0, slices[0].offset);
  EXPECT_EQ(0, slices[0].received_bytes);

  // Request count is large, the minimum slice size should limit the number of
  // slices returned.
  slices = FindSlicesForRemainingContent(0, 100, 33, 50);
  EXPECT_EQ(2u, slices.size());
  EXPECT_EQ(0, slices[0].offset);
  EXPECT_EQ(50, slices[0].received_bytes);
  EXPECT_EQ(50, slices[1].offset);
  EXPECT_EQ(0, slices[1].received_bytes);

  // Can chunk 2 slices under minimum slice size, but request count is only 1,
  // request count should win.
  slices = FindSlicesForRemainingContent(0, 100, 1, 50);
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(0, slices[0].offset);
  EXPECT_EQ(0, slices[0].received_bytes);

  // A total 100 bytes data and a 51 bytes minimum slice size, only one slice is
  // returned.
  slices = FindSlicesForRemainingContent(0, 100, 3, 51);
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(0, slices[0].offset);
  EXPECT_EQ(0, slices[0].received_bytes);

  // Extreme case where size is smaller than request number.
  slices = FindSlicesForRemainingContent(0, 1, 3, 1);
  EXPECT_EQ(1u, slices.size());
  EXPECT_EQ(download::DownloadItem::ReceivedSlice(0, 0), slices[0]);

  // Normal case.
  slices = FindSlicesForRemainingContent(0, 100, 3, 5);
  EXPECT_EQ(3u, slices.size());
  EXPECT_EQ(download::DownloadItem::ReceivedSlice(0, 33), slices[0]);
  EXPECT_EQ(download::DownloadItem::ReceivedSlice(33, 33), slices[1]);
  EXPECT_EQ(download::DownloadItem::ReceivedSlice(66, 0), slices[2]);
}

TEST_F(ParallelDownloadUtilsTest, GetMaxContiguousDataBlockSizeFromBeginning) {
  std::vector<download::DownloadItem::ReceivedSlice> slices;
  slices.emplace_back(500, 500);
  EXPECT_EQ(0, GetMaxContiguousDataBlockSizeFromBeginning(slices));

  download::DownloadItem::ReceivedSlice slice1(0, 200);
  AddOrMergeReceivedSliceIntoSortedArray(slice1, slices);
  EXPECT_EQ(200, GetMaxContiguousDataBlockSizeFromBeginning(slices));

  download::DownloadItem::ReceivedSlice slice2(200, 300);
  AddOrMergeReceivedSliceIntoSortedArray(slice2, slices);
  EXPECT_EQ(1000, GetMaxContiguousDataBlockSizeFromBeginning(slices));
}

// Test to verify Finch parameters for enabled experiment group is read
// correctly.
TEST_F(ParallelDownloadUtilsTest, FinchConfigEnabled) {
  base::test::ScopedFeatureList feature_list;
  std::map<std::string, std::string> params = {
      {content::kMinSliceSizeFinchKey, "1234"},
      {content::kParallelRequestCountFinchKey, "6"},
      {content::kParallelRequestDelayFinchKey, "2000"},
      {content::kParallelRequestRemainingTimeFinchKey, "3"}};
  feature_list.InitAndEnableFeatureWithParameters(
      features::kParallelDownloading, params);
  EXPECT_TRUE(IsParallelDownloadEnabled());
  EXPECT_EQ(GetMinSliceSizeConfig(), 1234);
  EXPECT_EQ(GetParallelRequestCountConfig(), 6);
  EXPECT_EQ(GetParallelRequestDelayConfig(), base::TimeDelta::FromSeconds(2));
  EXPECT_EQ(GetParallelRequestRemainingTimeConfig(),
            base::TimeDelta::FromSeconds(3));
}

// Test to verify the disable experiment group will actually disable the
// feature.
TEST_F(ParallelDownloadUtilsTest, FinchConfigDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kParallelDownloading);
  EXPECT_FALSE(IsParallelDownloadEnabled());
}

// Test to verify that the Finch parameter |enable_parallel_download| works
// correctly.
TEST_F(ParallelDownloadUtilsTest, FinchConfigDisabledWithParameter) {
  {
    base::test::ScopedFeatureList feature_list;
    std::map<std::string, std::string> params = {
        {content::kMinSliceSizeFinchKey, "4321"},
        {content::kEnableParallelDownloadFinchKey, "false"}};
    feature_list.InitAndEnableFeatureWithParameters(
        features::kParallelDownloading, params);
    // Use |enable_parallel_download| to disable parallel download in enabled
    // experiment group.
    EXPECT_FALSE(IsParallelDownloadEnabled());
    EXPECT_EQ(GetMinSliceSizeConfig(), 4321);
  }
  {
    base::test::ScopedFeatureList feature_list;
    std::map<std::string, std::string> params = {
        {content::kMinSliceSizeFinchKey, "4321"},
        {content::kEnableParallelDownloadFinchKey, "true"}};
    feature_list.InitAndEnableFeatureWithParameters(
        features::kParallelDownloading, params);
    // Disable only if |enable_parallel_download| sets to false.
    EXPECT_TRUE(IsParallelDownloadEnabled());
    EXPECT_EQ(GetMinSliceSizeConfig(), 4321);
  }
  {
    base::test::ScopedFeatureList feature_list;
    std::map<std::string, std::string> params = {
        {content::kMinSliceSizeFinchKey, "4321"}};
    feature_list.InitAndEnableFeatureWithParameters(
        features::kParallelDownloading, params);
    // Empty |enable_parallel_download| in an enabled experiment group will have
    // no impact.
    EXPECT_TRUE(IsParallelDownloadEnabled());
    EXPECT_EQ(GetMinSliceSizeConfig(), 4321);
  }
}

// Verify if a preceding stream can recover the download for half open error
// stream(the current last stream).
TEST_P(ParallelDownloadUtilsRecoverErrorTest,
       RecoverErrorForHalfOpenErrorStream) {
  // Create a stream that will work on byte range "100-".
  const int kErrorStreamOffset = 100;
  auto error_stream = CreateSourceStream(
      kErrorStreamOffset, download::DownloadSaveInfo::kLengthFullContent);
  error_stream->set_finished(true);

  // Get starting offset of preceding stream.
  int64_t preceding_offset = GetParam();
  EXPECT_LT(preceding_offset, kErrorStreamOffset);
  auto preceding_stream = CreateSourceStream(
      preceding_offset, download::DownloadSaveInfo::kLengthFullContent);
  EXPECT_FALSE(preceding_stream->is_finished());
  EXPECT_EQ(0u, preceding_stream->bytes_written());
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // Half open finished preceding stream with 0 bytes written, if there is no
  // error, the download should be finished.
  preceding_stream->set_finished(true);
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NONE,
            preceding_stream->GetCompletionStatus());
  EXPECT_TRUE(CanRecoverFromError(error_stream.get(), preceding_stream.get()));

  // Half open finished preceding stream with error, should be treated as
  // failed.
  preceding_stream->OnResponseCompleted(
      download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE);
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
  preceding_stream->OnResponseCompleted(
      download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE);
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
  preceding_stream->OnResponseCompleted(
      download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE);
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

}  // namespace content
