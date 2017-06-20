// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_logs/single_log_source.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/process_info.h"
#include "base/strings/string_split.h"
#include "base/task_scheduler/post_task.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

namespace {

constexpr char kDefaultSystemLogDirPath[] = "/var/log";
constexpr int kMaxNumAllowedLogRotationsDuringFileRead = 3;

// For log files that contain old logging, start reading from the first
// timestamp that is less than this amount of time before the current session of
// Chrome started.
constexpr base::TimeDelta kLogCutoffTimeBeforeChromeStart =
    base::TimeDelta::FromMinutes(10);

// A custom timestamp for when the current Chrome session started. Used during
// testing to override the actual time.
const base::Time* g_chrome_start_time_for_test = nullptr;

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

// Returns the inode value of file at |path|, or 0 if it doesn't exist or is
// otherwise unable to be accessed for file system info.
ino_t GetInodeValue(const base::FilePath& path) {
  struct stat file_stats;
  if (stat(path.value().c_str(), &file_stats) != 0)
    return 0;
  return file_stats.st_ino;
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

// Returns the time that the current Chrome process started. Will instead return
// |*g_chrome_start_time_for_test| if it is set.
base::Time GetChromeStartTime() {
  if (g_chrome_start_time_for_test)
    return *g_chrome_start_time_for_test;
  return base::CurrentProcessInfo::CreationTime();
}

// Returns the file offset into |path| of the first line that starts with a
// timestamp no earlier than |time|. Returns 0 if no such offset could be
// determined (e.g. can't open file, no timestamps present).
size_t GetFirstFileOffsetWithTime(const base::FilePath& path,
                                  const base::Time& time) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return 0;

  const size_t file_size = file.GetLength();
  if (file_size == 0)
    return 0;

  std::string file_contents;
  file_contents.resize(file_size);
  size_t size_read = file.ReadAtCurrentPos(&file_contents[0], file_size);

  if (size_read < file_size)
    return 0;

  std::vector<base::StringPiece> lines = base::SplitStringPiece(
      file_contents, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  bool any_timestamp_found = false;

  // Find the first line with timestamp >= |time|. If a line has no timestamp,
  // just advance to the next line.
  size_t offset = 0;
  base::Time timestamp;
  for (const auto& line : lines) {
    if (base::Time::FromString(line.as_string().c_str(), &timestamp)) {
      any_timestamp_found = true;

      if (timestamp >= time)
        break;
    }

    // Include the newline in the offset.
    offset += line.length() + 1;
  }

  // If the file does not have any timestamps at all, don't skip any contents.
  if (!any_timestamp_found)
    return 0;

  if (offset > 0 && offset >= file_size && lines.back().as_string().empty()) {
    // The last line may or may not have ended with a newline. If it ended with
    // a newline, |lines| would end with an extra empty line after the newline.
    // This would have resulted in an extra nonexistent newline being counted
    // during the computation of |offset|.
    --offset;
  }
  return offset;
}

}  // namespace

SingleLogSource::SingleLogSource(SupportedSource source_type)
    : SystemLogsSource(GetLogFileSourceRelativeFilePath(source_type).value()),
      source_type_(source_type),
      log_file_dir_path_(kDefaultSystemLogDirPath),
      num_bytes_read_(0),
      file_inode_(0),
      weak_ptr_factory_(this) {}

SingleLogSource::~SingleLogSource() {}

// static
void SingleLogSource::SetChromeStartTimeForTesting(
    const base::Time* start_time) {
  g_chrome_start_time_for_test = start_time;
}

void SingleLogSource::Fetch(const SysLogsSourceCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  SystemLogsResponse* response = new SystemLogsResponse;
  base::PostTaskWithTraitsAndReply(
      FROM_HERE,
      base::TaskTraits(base::MayBlock(), base::TaskPriority::BACKGROUND),
      base::Bind(&SingleLogSource::ReadFile, weak_ptr_factory_.GetWeakPtr(),
                 kMaxNumAllowedLogRotationsDuringFileRead, response),
      base::Bind(callback, base::Owned(response)));
}

base::FilePath SingleLogSource::GetLogFilePath() const {
  return base::FilePath(log_file_dir_path_).Append(source_name());
}

void SingleLogSource::ReadFile(size_t num_rotations_allowed,
                               SystemLogsResponse* result) {
  // Attempt to open the file if it was not previously opened.
  if (!file_.IsValid()) {
    file_.Initialize(GetLogFilePath(),
                     base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!file_.IsValid())
      return;

    // Determine actual offset from which to start reading.
    if (source_type_ == SupportedSource::kMessages) {
      const base::Time earliest_log_time =
          GetChromeStartTime() - kLogCutoffTimeBeforeChromeStart;

      num_bytes_read_ =
          GetFirstFileOffsetWithTime(GetLogFilePath(), earliest_log_time);
    } else {
      num_bytes_read_ = 0;
    }
    file_.Seek(base::File::FROM_BEGIN, num_bytes_read_);

    file_inode_ = GetInodeValue(GetLogFilePath());
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

  const bool file_was_rotated = file_inode_ != GetInodeValue(GetLogFilePath());
  const bool should_handle_file_rotation =
      file_was_rotated && num_rotations_allowed > 0;

  // The reader may only read complete lines. The exception is when there is a
  // rotation, in which case all the remaining contents of the old log file
  // should be read before moving on to read the new log file.
  if ((result_string.empty() || result_string.back() != '\n') &&
      !should_handle_file_rotation) {
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

  // If the file was rotated, close the file handle and call this function
  // again, to read from the new file.
  if (should_handle_file_rotation) {
    file_.Close();
    num_bytes_read_ = 0;
    file_inode_ = 0;
    ReadFile(num_rotations_allowed - 1, result);
  }
}

}  // namespace system_logs
