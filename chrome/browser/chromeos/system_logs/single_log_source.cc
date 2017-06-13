// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/single_log_source.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/task_scheduler/post_task.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

namespace {

const char kDefaultSystemLogDirPath[] = "/var/log";

// Converts a logs source type to the corresponding file path, relative to the
// base system log directory path. In the future, if non-file source types are
// added, this function should return an empty file path.
base::FilePath GetLogFileSourceRelativeFilePath(
    SingleLogSource::SupportedSource source) {
  switch (source) {
    case SingleLogSource::SupportedSource::kMessages:
      return base::FilePath("messages");
    case SingleLogSource::SupportedSource::kUiLatest:
      return base::FilePath("ui/ui.LATEST");
  }
  NOTREACHED();
  return base::FilePath();
}

// Attempts to store a string |value| in |*response| under |key|. If there is
// already a string in |*response| under |key|, appends |value| to the existing
// string value.
void AppendToSystemLogsResponse(SystemLogsResponse* response,
                                const std::string& key,
                                const std::string& value) {
  auto iter = response->find(key);
  if (iter == response->end())
    response->emplace(key, value);
  else
    iter->second += value;
}

}  // namespace

SingleLogSource::SingleLogSource(SupportedSource source)
    : SystemLogsSource(GetLogFileSourceRelativeFilePath(source).value()),
      log_file_dir_path_(kDefaultSystemLogDirPath),
      num_bytes_read_(0),
      weak_ptr_factory_(this) {}

SingleLogSource::~SingleLogSource() {}

void SingleLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  SystemLogsResponse* response = new SystemLogsResponse;
  base::PostTaskWithTraitsAndReply(
      FROM_HERE,
      base::TaskTraits(base::MayBlock(), base::TaskPriority::BACKGROUND),
      base::Bind(&SingleLogSource::ReadFile, weak_ptr_factory_.GetWeakPtr(),
                 response),
      base::Bind(callback, base::Owned(response)));
}

void SingleLogSource::ReadFile(SystemLogsResponse* result) {
  // Attempt to open the file if it was not previously opened.
  if (!file_.IsValid()) {
    file_.Initialize(base::FilePath(log_file_dir_path_).Append(source_name()),
                     base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!file_.IsValid())
      return;
  }

  // Check for file size reset.
  const size_t length = file_.GetLength();
  if (length < num_bytes_read_) {
    num_bytes_read_ = 0;
    file_.Seek(base::File::FROM_BEGIN, 0);
  }

  // Read from file until end.
  const size_t size_to_read = length - num_bytes_read_;
  std::string result_string;
  result_string.resize(size_to_read);
  size_t size_read = file_.ReadAtCurrentPos(&result_string[0], size_to_read);
  result_string.resize(size_read);

  // The reader may only read complete lines.
  if (result_string.empty() || result_string.back() != '\n') {
    // If an incomplete line was read, return only the part that includes whole
    // lines.
    size_t last_newline_pos = result_string.find_last_of('\n');
    if (last_newline_pos == std::string::npos) {
      file_.Seek(base::File::FROM_CURRENT, -size_read);
      AppendToSystemLogsResponse(result, source_name(), "");
      return;
    }
    // The part of the string that will be returned includes the newline itself.
    size_t adjusted_size_read = last_newline_pos + 1;
    file_.Seek(base::File::FROM_CURRENT, -size_read + adjusted_size_read);
    result_string.resize(adjusted_size_read);

    // Update |size_read| to reflect that the read was only up to the last
    // newline.
    size_read = adjusted_size_read;
  }

  num_bytes_read_ += size_read;

  // Pass it back to the callback.
  AppendToSystemLogsResponse(result, source_name(),
                             anonymizer_.Anonymize(result_string));
}

}  // namespace system_logs
