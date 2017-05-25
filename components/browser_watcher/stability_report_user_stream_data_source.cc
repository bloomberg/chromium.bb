// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/stability_report_user_stream_data_source.h"

#include <string>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/browser_watcher/minidump_user_streams.h"
#include "components/browser_watcher/stability_paths.h"
#include "components/browser_watcher/stability_report_extractor.h"
#include "third_party/crashpad/crashpad/minidump/minidump_user_extension_stream_data_source.h"
#include "third_party/crashpad/crashpad/snapshot/process_snapshot.h"

namespace browser_watcher {

namespace {

base::FilePath GetStabilityFileName(
    const base::FilePath& user_data_dir,
    crashpad::ProcessSnapshot* process_snapshot) {
  DCHECK(process_snapshot);

  timeval creation_time = {};
  process_snapshot->ProcessStartTime(&creation_time);

  return GetStabilityFileForProcess(process_snapshot->ProcessID(),
                                    creation_time, user_data_dir);
}

class BufferExtensionStreamDataSource final
    : public crashpad::MinidumpUserExtensionStreamDataSource {
 public:
  explicit BufferExtensionStreamDataSource(uint32_t stream_type);

  bool Init(const StabilityReport& report);

  size_t StreamDataSize() override;
  bool ReadStreamData(Delegate* delegate) override;

 private:
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(BufferExtensionStreamDataSource);
};

BufferExtensionStreamDataSource::BufferExtensionStreamDataSource(
    uint32_t stream_type)
    : crashpad::MinidumpUserExtensionStreamDataSource(stream_type) {}

bool BufferExtensionStreamDataSource::Init(const StabilityReport& report) {
  if (report.SerializeToString(&data_))
    return true;
  data_.clear();
  return false;
}

size_t BufferExtensionStreamDataSource::StreamDataSize() {
  DCHECK(!data_.empty());
  return data_.size();
}

bool BufferExtensionStreamDataSource::ReadStreamData(Delegate* delegate) {
  DCHECK(!data_.empty());
  return delegate->ExtensionStreamDataSourceRead(
      data_.size() ? data_.data() : nullptr, data_.size());
}

std::unique_ptr<BufferExtensionStreamDataSource> CollectReport(
    const base::FilePath& path) {
  StabilityReport report;
  CollectionStatus status = Extract(path, &report);
  UMA_HISTOGRAM_ENUMERATION("ActivityTracker.CollectCrash.Status", status,
                            COLLECTION_STATUS_MAX);
  if (status != SUCCESS)
    return nullptr;

  // Open (with delete) and then immediately close the file by going out of
  // scope. This should cause the stability debugging file to be deleted prior
  // to the next execution.
  // TODO(manzagop): set the persistent allocator file's state to deleted in
  //     case the file can't be deleted.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_DELETE_ON_CLOSE);
  UMA_HISTOGRAM_BOOLEAN("ActivityTracker.CollectCrash.OpenForDeleteSuccess",
                        file.IsValid());

  std::unique_ptr<BufferExtensionStreamDataSource> source(
      new BufferExtensionStreamDataSource(kStabilityReportStreamType));
  return source->Init(report) ? std::move(source) : nullptr;
}

}  // namespace

StabilityReportUserStreamDataSource::StabilityReportUserStreamDataSource(
    const base::FilePath& user_data_dir)
    : user_data_dir_(user_data_dir) {}

std::unique_ptr<crashpad::MinidumpUserExtensionStreamDataSource>
StabilityReportUserStreamDataSource::ProduceStreamData(
    crashpad::ProcessSnapshot* process_snapshot) {
  DCHECK(process_snapshot);

  if (user_data_dir_.empty())
    return nullptr;

  base::FilePath stability_file =
      GetStabilityFileName(user_data_dir_, process_snapshot);
  if (!PathExists(stability_file)) {
    // Either this is not an instrumented process (currently only browser
    // processes can be instrumented), or the stability file cannot be found.
    return nullptr;
  }

  return CollectReport(stability_file);
}

}  // namespace browser_watcher
