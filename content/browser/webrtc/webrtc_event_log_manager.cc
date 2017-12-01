// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webrtc/webrtc_event_log_manager.h"

#include <inttypes.h>

#include <iostream>
#include <limits>

#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/post_task.h"
#include "build/build_config.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_WIN)
#define IntToStringType base::IntToString16
#else
#define IntToStringType base::IntToString
#endif

namespace content {

namespace {
inline void MaybeReplyWithBool(const base::Location& from_here,
                               base::OnceCallback<void(bool)> reply,
                               bool value) {
  if (reply) {
    BrowserThread::PostTask(BrowserThread::UI, from_here,
                            base::BindOnce(std::move(reply), value));
  }
}

struct MaybeReplyWithBoolOnExit final {
  MaybeReplyWithBoolOnExit(base::OnceCallback<void(bool)> reply,
                           bool default_value)
      : reply(std::move(reply)), value(default_value) {}
  ~MaybeReplyWithBoolOnExit() {
    if (reply) {
      MaybeReplyWithBool(FROM_HERE, std::move(reply), value);
    }
  }
  base::OnceCallback<void(bool)> reply;
  bool value;
};

struct MaybeReplyWithFilePathOnExit final {
  explicit MaybeReplyWithFilePathOnExit(
      base::OnceCallback<void(base::FilePath)> reply)
      : reply(std::move(reply)) {}
  ~MaybeReplyWithFilePathOnExit() {
    if (reply) {
      BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                              base::BindOnce(std::move(reply), file_path));
    }
  }
  base::OnceCallback<void(base::FilePath)> reply;
  base::FilePath file_path;  // Empty path is the default value.
};
}  // namespace

base::LazyInstance<WebRtcEventLogManager>::Leaky g_webrtc_event_log_manager =
    LAZY_INSTANCE_INITIALIZER;

WebRtcEventLogManager* WebRtcEventLogManager::GetInstance() {
  return g_webrtc_event_log_manager.Pointer();
}

WebRtcEventLogManager::WebRtcEventLogManager()
    : clock_for_testing_(nullptr),
      file_task_runner_(base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

WebRtcEventLogManager::~WebRtcEventLogManager() {
  // This should never actually run, except in unit tests.
}

void WebRtcEventLogManager::LocalWebRtcEventLogStart(
    int render_process_id,
    int lid,
    const base::FilePath& base_path,
    size_t max_file_size_bytes,
    base::OnceCallback<void(base::FilePath)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebRtcEventLogManager::StartLocalLogFile,
                     base::Unretained(this), render_process_id, lid, base_path,
                     max_file_size_bytes, std::move(reply)));
}

void WebRtcEventLogManager::LocalWebRtcEventLogStop(
    int render_process_id,
    int lid,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcEventLogManager::StopLocalLogFile,
                                base::Unretained(this), render_process_id, lid,
                                std::move(reply)));
}

void WebRtcEventLogManager::OnWebRtcEventLogWrite(
    int render_process_id,
    int lid,
    const std::string& output,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  file_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WebRtcEventLogManager::WriteToLocalLogFile,
                                base::Unretained(this), render_process_id, lid,
                                output, std::move(reply)));
  // TODO(eladalon): Make sure that peer-connections that have neither
  // a remote-bound log nor a local-log-file associated, do not trigger
  // this callback, for efficiency's sake. (Files sometimes fail to be opened,
  // or reach their maximum size.) https://crbug.com/775415
}

void WebRtcEventLogManager::InjectClockForTesting(base::Clock* clock) {
  clock_for_testing_ = clock;
}

base::FilePath WebRtcEventLogManager::GetLocalFilePath(
    const base::FilePath& base_path,
    int render_process_id,
    int lid) {
  base::Time::Exploded now;
  if (clock_for_testing_) {
    clock_for_testing_->Now().LocalExplode(&now);
  } else {
    base::Time::Now().LocalExplode(&now);
  }

  // [user_defined]_[date]_[time]_[pid]_[lid].log
  char stamp[100];
  int written =
      base::snprintf(stamp, arraysize(stamp), "%04d%02d%02d_%02d%02d_%d_%d",
                     now.year, now.month, now.day_of_month, now.hour,
                     now.minute, render_process_id, lid);
  CHECK(0 < written);
  CHECK(static_cast<size_t>(written) < arraysize(stamp));

  return base_path.InsertBeforeExtension(FILE_PATH_LITERAL("_"))
      .InsertBeforeExtensionASCII(base::StringPiece(stamp))
      .AddExtension(FILE_PATH_LITERAL("log"));
}

void WebRtcEventLogManager::StartLocalLogFile(
    int render_process_id,
    int lid,
    const base::FilePath& base_path,
    size_t max_file_size_bytes,
    base::OnceCallback<void(base::FilePath)> reply) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());

  // This object's destructor will ensure a reply (if |reply| is non-empty),
  // regardless of which path is taken out of the function.
  MaybeReplyWithFilePathOnExit on_exit(std::move(reply));

  // Add some information to the name given by the caller.
  base::FilePath file_path =
      GetLocalFilePath(base_path, render_process_id, lid);
  CHECK(!file_path.empty()) << "Couldn't set path for local WebRTC log file.";

  // In the unlikely case that this filename is already taken, find a unique
  // number to append to the filename, if possible.
  int unique_number =
      base::GetUniquePathNumber(file_path, base::FilePath::StringType());
  if (unique_number < 0) {
    return;  // No available file path was found.
  } else if (unique_number != 0) {
    // The filename is taken, but a unique number was found.
    // TODO(eladalon): Fix the way the unique number is used.
    // https://crbug.com/785333
    file_path = file_path.InsertBeforeExtension(FILE_PATH_LITERAL(" (") +
                                                IntToStringType(unique_number) +
                                                FILE_PATH_LITERAL(")"));
  }

  // Attempt to create the file.
  constexpr int file_flags = base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                             base::File::FLAG_EXCLUSIVE_WRITE;
  base::File file(file_path, file_flags);
  if (!file.IsValid() || !file.created()) {
    LOG(WARNING) << "Couldn't create and/or open local WebRTC event log file.";
    return;
  }

  // If the file was successfully created, it's now ready to be written to.
  DCHECK(local_logs_.find({render_process_id, lid}) == local_logs_.end());
  local_logs_.emplace(PeerConnectionKey{render_process_id, lid},
                      LogFile(std::move(file), max_file_size_bytes));

  on_exit.file_path = file_path;  // Report success (if reply nonempty).
}

void WebRtcEventLogManager::StopLocalLogFile(
    int render_process_id,
    int lid,
    base::OnceCallback<void(bool)> reply) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  auto it = local_logs_.find(PeerConnectionKey{render_process_id, lid});
  if (it != local_logs_.end()) {
    CloseLocalLogFile(it);
    MaybeReplyWithBool(FROM_HERE, std::move(reply), true);
  } else {
    MaybeReplyWithBool(FROM_HERE, std::move(reply), false);
  }
}

void WebRtcEventLogManager::WriteToLocalLogFile(
    int render_process_id,
    int lid,
    const std::string& output,
    base::OnceCallback<void(bool)> reply) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  DCHECK_LE(output.length(),
            static_cast<size_t>(std::numeric_limits<int>::max()));

  // This object's destructor will ensure a reply (if |reply| is non-empty),
  // regardless of which path is taken out of the function.
  MaybeReplyWithBoolOnExit on_exit(std::move(reply), false);

  auto it = local_logs_.find(PeerConnectionKey{render_process_id, lid});
  if (it == local_logs_.end()) {
    return;
  }

  // Observe the file size limit, if any. Note that base::File's interface does
  // not allow writing more than numeric_limits<int>::max() bytes at a time.
  int output_len = static_cast<int>(output.length());  // DCHECKed above.
  LogFile& log_file = it->second;
  if (log_file.max_file_size_bytes != kUnlimitedFileSize) {
    DCHECK_LT(log_file.file_size_bytes, log_file.max_file_size_bytes);
    if (log_file.file_size_bytes + output.length() < log_file.file_size_bytes ||
        log_file.file_size_bytes + output.length() >
            log_file.max_file_size_bytes) {
      output_len = log_file.max_file_size_bytes - log_file.file_size_bytes;
    }
  }

  int written = log_file.file.WriteAtCurrentPos(output.c_str(), output_len);
  if (written < 0 || written != output_len) {  // Error
    LOG(WARNING) << "WebRTC event log output couldn't be written to local "
                    "file in its entirety.";
    CloseLocalLogFile(it);
    return;
  }

  // Truncated output due to exceeding the maximum is reported as an error - the
  // caller is interested to know that not all of its output was written,
  // regardless of the reason.
  on_exit.value = (static_cast<size_t>(written) == output.length());

  log_file.file_size_bytes += static_cast<size_t>(written);
  if (log_file.max_file_size_bytes != kUnlimitedFileSize) {
    DCHECK_LE(log_file.file_size_bytes, log_file.max_file_size_bytes);
    if (log_file.file_size_bytes >= log_file.max_file_size_bytes) {
      CloseLocalLogFile(it);
    }
  }
}

void WebRtcEventLogManager::CloseLocalLogFile(LocalLogFilesMap::iterator it) {
  DCHECK(file_task_runner_->RunsTasksInCurrentSequence());
  it->second.file.Flush();
  local_logs_.erase(it);  // file.Close() called by destructor.
}

}  // namespace content
