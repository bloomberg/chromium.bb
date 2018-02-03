// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_ukm_helper.h"

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using UkmDownloadStarted = ukm::builders::Download_Started;
using UkmDownloadInterrupted = ukm::builders::Download_Interrupted;
using UkmDownloadResumed = ukm::builders::Download_Resumed;
using UkmDownloadCompleted = ukm::builders::Download_Completed;

namespace content {

class DownloadUkmHelperTest : public testing::Test {
 public:
  DownloadUkmHelperTest() : download_id_(123) { ResetUkmRecorder(); }
  ~DownloadUkmHelperTest() override = default;

  void ResetUkmRecorder() {
    test_recorder_.reset(new ukm::TestAutoSetUkmRecorder);
  }

  void ExpectUkmMetrics(const base::StringPiece entry_name,
                        const std::vector<base::StringPiece>& keys,
                        const std::vector<int>& values) {
    const auto& entries = test_recorder_->GetEntriesByName(entry_name);
    EXPECT_EQ(1u, entries.size());
    for (const auto* entry : entries) {
      const size_t keys_size = keys.size();
      EXPECT_EQ(keys_size, values.size());
      for (size_t i = 0; i < keys_size; ++i) {
        test_recorder_->ExpectEntryMetric(entry, keys[i], values[i]);
      }
    }
  }

 protected:
  int download_id_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_recorder_;

  DISALLOW_COPY_AND_ASSIGN(DownloadUkmHelperTest);
};

TEST_F(DownloadUkmHelperTest, TestBasicReporting) {
  // RecordDownloadStarted
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  DownloadContent file_type = DownloadContent::AUDIO;
  download::DownloadSource download_source = download::DownloadSource::UNKNOWN;
  DownloadUkmHelper::RecordDownloadStarted(download_id_, source_id, file_type,
                                           download_source);

  ExpectUkmMetrics(
      UkmDownloadStarted::kEntryName,
      {UkmDownloadStarted::kDownloadIdName, UkmDownloadStarted::kFileTypeName,
       UkmDownloadStarted::kDownloadSourceName},
      {download_id_, static_cast<int>(file_type),
       static_cast<int>(download_source)});

  // RecordDownloadInterrupted, has change in file size.
  int change_in_file_size = 1000;
  download::DownloadInterruptReason reason =
      download::DOWNLOAD_INTERRUPT_REASON_NONE;
  int resulting_file_size = 2000;
  int time_since_start = 250;
  DownloadUkmHelper::RecordDownloadInterrupted(
      download_id_, change_in_file_size, reason, resulting_file_size,
      base::TimeDelta::FromMilliseconds(time_since_start));

  ExpectUkmMetrics(
      UkmDownloadInterrupted::kEntryName,
      {UkmDownloadInterrupted::kDownloadIdName,
       UkmDownloadInterrupted::kChangeInFileSizeName,
       UkmDownloadInterrupted::kReasonName,
       UkmDownloadInterrupted::kResultingFileSizeName,
       UkmDownloadInterrupted::kTimeSinceStartName},
      {download_id_,
       DownloadUkmHelper::CalcExponentialBucket(change_in_file_size),
       static_cast<int>(reason),
       DownloadUkmHelper::CalcExponentialBucket(resulting_file_size),
       time_since_start});

  // RecordDownloadResumed.
  ResumeMode mode = ResumeMode::IMMEDIATE_RESTART;
  int time_since_start_resume = 300;
  DownloadUkmHelper::RecordDownloadResumed(
      download_id_, mode,
      base::TimeDelta::FromMilliseconds(time_since_start_resume));

  ExpectUkmMetrics(
      UkmDownloadResumed::kEntryName,
      {UkmDownloadResumed::kDownloadIdName, UkmDownloadResumed::kModeName,
       UkmDownloadResumed::kTimeSinceStartName},
      {download_id_, static_cast<int>(mode), time_since_start_resume});

  // RecordDownloadCompleted.
  int resulting_file_size_completed = 3000;
  int time_since_start_completed = 400;
  DownloadUkmHelper::RecordDownloadCompleted(
      download_id_, resulting_file_size_completed,
      base::TimeDelta::FromMilliseconds(time_since_start_completed));

  ExpectUkmMetrics(
      UkmDownloadCompleted::kEntryName,
      {UkmDownloadCompleted::kDownloadIdName,
       UkmDownloadCompleted::kResultingFileSizeName,
       UkmDownloadCompleted::kTimeSinceStartName},
      {download_id_,
       DownloadUkmHelper::CalcExponentialBucket(resulting_file_size_completed),
       time_since_start_completed});
}

}  // namespace content
