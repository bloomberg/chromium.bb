// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/mock_download_file.h"

#include "content/browser/download/download_create_info.h"
#include "testing/gtest/include/gtest/gtest.h"

MockDownloadFile::StatisticsRecorder::StatisticsRecorder() {
}

MockDownloadFile::StatisticsRecorder::~StatisticsRecorder() {
}

void MockDownloadFile::StatisticsRecorder::Record(StatisticsIndex index) {
  Add(index, 1);
}

void MockDownloadFile::StatisticsRecorder::Add(StatisticsIndex index,
                                               int count) {
  map_[index] = map_[index] + count;
}

int MockDownloadFile::StatisticsRecorder::Count(StatisticsIndex index) {
  if (map_.find(index) == map_.end())
    return 0;
  return map_[index];
}

MockDownloadFile::MockDownloadFile(
    const DownloadCreateInfo* info,
    const DownloadRequestHandle& request_handle,
    DownloadManager* download_manager,
    StatisticsRecorder* recorder)
        : id_(info->download_id),
          request_handle_(request_handle),
          download_manager_(download_manager),
          recorder_(recorder),
          rename_count_(0),
          in_progress_(true) {
}

MockDownloadFile::~MockDownloadFile() {
}

net::Error MockDownloadFile::Initialize(bool calculate_hash) {
  in_progress_ = true;
  if (recorder_)
    recorder_->Record(StatisticsRecorder::STAT_INITIALIZE);
  return net::OK;
}

net::Error MockDownloadFile::AppendDataToFile(
    const char* data, size_t data_len) {
  data_.append(data, data_len);
  if (recorder_) {
    recorder_->Record(StatisticsRecorder::STAT_APPEND);
    recorder_->Add(StatisticsRecorder::STAT_BYTES, data_len);
  }
  return net::OK;
}

net::Error MockDownloadFile::Rename(const FilePath& full_path) {
    EXPECT_LT(rename_count_, expected_rename_path_list_.size());
    EXPECT_STREQ(expected_rename_path_list_[rename_count_].value().c_str(),
                 full_path.value().c_str());
    ++rename_count_;
    if (recorder_)
      recorder_->Record(StatisticsRecorder::STAT_RENAME);
    return net::OK;
}

void MockDownloadFile::Detach() {
  if (recorder_)
    recorder_->Record(StatisticsRecorder::STAT_DETACH);
}

void MockDownloadFile::Cancel() {
  in_progress_ = false;
  if (recorder_)
    recorder_->Record(StatisticsRecorder::STAT_CANCEL);
}

void MockDownloadFile::Finish() {
  in_progress_ = false;
  if (recorder_)
    recorder_->Record(StatisticsRecorder::STAT_FINISH);
}

void MockDownloadFile::AnnotateWithSourceInformation() {
}

FilePath MockDownloadFile::FullPath() const {
  return FilePath();
}

bool MockDownloadFile::InProgress() const {
  return in_progress_;
}

int64 MockDownloadFile::BytesSoFar() const {
  return data_.length();
}

int64 MockDownloadFile::CurrentSpeed() const {
  return 0;
}

bool MockDownloadFile::GetSha256Hash(std::string* hash) {
  return false;
}

// DownloadFileInterface implementation.
void MockDownloadFile::CancelDownloadRequest() {
}

int MockDownloadFile::Id() const {
  return id_.local();
}

DownloadManager* MockDownloadFile::GetDownloadManager() {
  return download_manager_;
}

const DownloadId& MockDownloadFile::GlobalId() const {
  return id_;
}

std::string MockDownloadFile::DebugString() const {
  return "";
}

void MockDownloadFile::SetExpectedPath(size_t index, const FilePath& path) {
  if (expected_rename_path_list_.size() < index + 1)
    expected_rename_path_list_.resize(index + 1);
  expected_rename_path_list_[index] = path;
}
