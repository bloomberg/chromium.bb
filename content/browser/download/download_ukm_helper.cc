// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_ukm_helper.h"

#include "services/metrics/public/cpp/ukm_builders.h"

namespace content {

namespace {
// Return the "size" of the bucket based on the allowed percent_error.
double CalcBucketIncrement() {
  double percent_error = 10;
  return log10(1 + percent_error / 100);
}
}  // namespace

int DownloadUkmHelper::CalcExponentialBucket(int value) {
  return static_cast<int>(floor(log10(value + 1) / CalcBucketIncrement()));
}

void DownloadUkmHelper::RecordDownloadStarted(
    int download_id,
    ukm::SourceId source_id,
    DownloadContent file_type,
    download::DownloadSource download_source) {
  ukm::builders::Download_Started(source_id)
      .SetDownloadId(download_id)
      .SetFileType(static_cast<int>(file_type))
      .SetDownloadSource(static_cast<int>(download_source))
      .Record(ukm::UkmRecorder::Get());
}

void DownloadUkmHelper::RecordDownloadInterrupted(
    int download_id,
    base::Optional<int> change_in_file_size,
    download::DownloadInterruptReason reason,
    int resulting_file_size,
    const base::TimeDelta& time_since_start) {
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::builders::Download_Interrupted builder(source_id);
  builder.SetDownloadId(download_id)
      .SetReason(static_cast<int>(reason))
      .SetResultingFileSize(
          DownloadUkmHelper::CalcExponentialBucket(resulting_file_size))
      .SetTimeSinceStart(time_since_start.InMilliseconds());
  if (change_in_file_size.has_value()) {
    builder.SetChangeInFileSize(
        DownloadUkmHelper::CalcExponentialBucket(change_in_file_size.value()));
  }
  builder.Record(ukm::UkmRecorder::Get());
}

void DownloadUkmHelper::RecordDownloadResumed(
    int download_id,
    ResumeMode mode,
    const base::TimeDelta& time_since_start) {
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::builders::Download_Resumed(source_id)
      .SetDownloadId(download_id)
      .SetMode(static_cast<int>(mode))
      .SetTimeSinceStart(time_since_start.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

void DownloadUkmHelper::RecordDownloadCompleted(
    int download_id,
    int resulting_file_size,
    const base::TimeDelta& time_since_start) {
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::builders::Download_Completed(source_id)
      .SetDownloadId(download_id)
      .SetResultingFileSize(
          DownloadUkmHelper::CalcExponentialBucket(resulting_file_size))
      .SetTimeSinceStart(time_since_start.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}

void DownloadUkmHelper::UpdateSourceURL(ukm::UkmRecorder* ukm_recorder,
                                        ukm::SourceId source_id,
                                        WebContents* web_contents) {
  if (ukm_recorder) {
    ukm_recorder->UpdateSourceURL(source_id,
                                  web_contents->GetLastCommittedURL());
  }
}

}  // namespace content
