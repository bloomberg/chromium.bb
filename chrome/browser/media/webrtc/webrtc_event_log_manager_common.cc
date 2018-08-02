// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/webrtc_event_log_manager_common.h"

#include <limits>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

using BrowserContextId = WebRtcEventLogPeerConnectionKey::BrowserContextId;

const size_t kWebRtcEventLogManagerUnlimitedFileSize = 0;

const char kStartRemoteLoggingFailureFeatureDisabled[] = "Feature disabled.";
const char kStartRemoteLoggingFailureUnlimitedSizeDisallowed[] =
    "Unlimited size disallowed.";
const char kStartRemoteLoggingFailureMaxSizeTooSmall[] = "Max size too small.";
const char kStartRemoteLoggingFailureMaxSizeTooLarge[] =
    "Excessively large max log size.";
const char kStartRemoteLoggingFailureUnknownOrInactivePeerConnection[] =
    "Unknown or inactive peer connection.";
const char kStartRemoteLoggingFailureAlreadyLogging[] = "Already logging.";
const char kStartRemoteLoggingFailureGeneric[] = "Unspecified error.";

const BrowserContextId kNullBrowserContextId =
    reinterpret_cast<BrowserContextId>(nullptr);

namespace {

// Tracks budget over a resource (such as bytes allowed in a file, etc.).
// Allows an unlimited budget.
class Budget {
 public:
  // If !max.has_value(), the budget is unlimited.
  explicit Budget(base::Optional<size_t> max) : max_(max), current_(0) {}

  // Check whether the budget allows consuming an additional |consumed| of
  // the resource.
  bool ConsumeAllowed(size_t consumed) const {
    if (!max_.has_value()) {
      return true;
    }

    DCHECK_LE(current_, max_.value());

    const size_t after_consumption = current_ + consumed;

    if (after_consumption < current_) {
      return false;  // Wrap-around.
    } else if (after_consumption > max_.value()) {
      return false;  // Budget exceeded.
    } else {
      return true;
    }
  }

  // Checks whether the budget has been completely used up.
  bool Exhausted() const { return !ConsumeAllowed(0); }

  // Consume an additional |consumed| of the resource.
  void Consume(size_t consumed) {
    DCHECK(ConsumeAllowed(consumed));
    current_ += consumed;
  }

 private:
  const base::Optional<size_t> max_;
  size_t current_;
};

// Writes a log to a file while observing a maximum size.
class BaseLogFileWriter : public LogFileWriter {
 public:
  // If !max_file_size_bytes.has_value(), an unlimited writer is created.
  // If it has a value, it must be at least MinFileSizeBytes().
  BaseLogFileWriter(const base::FilePath& path,
                    base::Optional<size_t> max_file_size_bytes);

  ~BaseLogFileWriter() override;

  bool Init() override;

  const base::FilePath& path() const override;

  bool MaxSizeReached() const override;

  bool Write(const std::string& input) override;

  bool Close() override;

  void Delete() override;

 protected:
  // * Logs are created PRE_INIT.
  // * If Init() is successful (potentially writing some header to the log),
  //   the log becomes ACTIVE.
  // * Any error puts the log into an unrecoverable ERRORED state. When an
  //   errored file is Close()-ed, it is deleted.
  // * If Write() is ever denied because of budget constraintss, the file
  //   becomes FULL. Only metadata is then allowed (subject to its own budget).
  // * Closing an ACTIVE or FULL file puts it into CLOSED, at which point the
  //   file may be used. (Note that closing itself might also yield an error,
  //   which would put the file into ERRORED, then deleted.)
  // * Closed files may be DELETED.
  enum class State { PRE_INIT, ACTIVE, FULL, CLOSED, ERRORED, DELETED };

  // Setter/getter for |state_|.
  void SetState(State state);
  State state() const { return state_; }

  // Checks whether the budget allows writing an additional |bytes|.
  bool WithinBudget(size_t bytes) const;

  // Writes |input| to the file.
  // May only be called on ACTIVE or FULL files (for FULL files, only metadata
  // such as compression footers, etc., may be written; the budget must still
  // be respected).
  // It's up to the caller to respect the budget; this will DCHECK on it.
  // Returns |true| if writing was successful. |false| indicates an
  // unrecoverable error; the file must be discarded.
  bool WriteInternal(const std::string& input, bool metadata);

  // Finalizes the file (writes metadata such as compression footer, if any).
  // Reports whether the file was successfully finalized. Those which weren't
  // should be discarded.
  virtual bool Finalize();

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const base::FilePath path_;
  base::File file_;  // Populated by Init().
  State state_;
  Budget budget_;
};

BaseLogFileWriter::BaseLogFileWriter(const base::FilePath& path,
                                     base::Optional<size_t> max_file_size_bytes)
    : task_runner_(base::SequencedTaskRunnerHandle::Get()),
      path_(path),
      state_(State::PRE_INIT),
      budget_(max_file_size_bytes) {}

BaseLogFileWriter::~BaseLogFileWriter() {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    // Chrome shut-down. The original task_runner_ is no longer running, so
    // no risk of concurrent access or races.
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    task_runner_ = base::SequencedTaskRunnerHandle::Get();
  }

  if (state() != State::CLOSED && state() != State::DELETED) {
    Close();
  }
}

bool BaseLogFileWriter::Init() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(state(), State::PRE_INIT);

  // TODO(crbug.com/775415): Use a temporary filename which will indicate
  // incompletion, and rename to something that is eligible for upload only
  // on an orderly and successful Close().

  // Attempt to create the file.
  constexpr int file_flags = base::File::FLAG_CREATE | base::File::FLAG_WRITE |
                             base::File::FLAG_EXCLUSIVE_WRITE;
  file_.Initialize(path_, file_flags);
  if (!file_.IsValid() || !file_.created()) {
    LOG(WARNING) << "Couldn't create remote-bound WebRTC event log file.";
    if (!base::DeleteFile(path_, /*recursive=*/false)) {
      LOG(ERROR) << "Failed to delete " << path_ << ".";
    }
    SetState(State::ERRORED);
    return false;
  }

  SetState(State::ACTIVE);

  return true;
}

const base::FilePath& BaseLogFileWriter::path() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return path_;
}

bool BaseLogFileWriter::MaxSizeReached() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  // Well-behaved code wouldn't check otherwise.
  DCHECK_EQ(state(), State::ACTIVE);
  return !WithinBudget(1);
}

bool BaseLogFileWriter::Write(const std::string& input) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(state(), State::ACTIVE);
  DCHECK(!MaxSizeReached());

  if (input.empty()) {
    return true;
  }

  if (!WithinBudget(input.length())) {
    SetState(State::FULL);
    return false;
  }

  const bool written = WriteInternal(input, /*metadata=*/false);
  if (!written) {
    SetState(State::ERRORED);
  }
  return written;
}

bool BaseLogFileWriter::Close() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(state(), State::CLOSED);
  DCHECK_NE(state(), State::DELETED);

  const bool result = ((state() != State::ERRORED) && Finalize());

  if (result) {
    file_.Flush();
    file_.Close();
    SetState(State::CLOSED);
  } else {
    Delete();  // Changes the state to DELETED.
  }

  return result;
}

void BaseLogFileWriter::Delete() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(state(), State::DELETED);

  // The file should be closed before deletion. However, we do not want to go
  // through Finalize() and any potential production of a compression footer,
  // etc., since we'll be discarding the file anyway.
  if (state() != State::CLOSED) {
    file_.Close();
  }

  if (!base::DeleteFile(path_, /*recursive=*/false)) {
    LOG(ERROR) << "Failed to delete " << path_ << ".";
  }

  SetState(State::DELETED);
}

void BaseLogFileWriter::SetState(State state) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  state_ = state;
}

bool BaseLogFileWriter::WithinBudget(size_t bytes) const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return budget_.ConsumeAllowed(bytes);
}

bool BaseLogFileWriter::WriteInternal(const std::string& input, bool metadata) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(state() == State::ACTIVE || (state() == State::FULL && metadata));
  DCHECK(WithinBudget(input.length()));

  // base::File's interface does not allow writing more than
  // numeric_limits<int>::max() bytes at a time.
  DCHECK_LE(input.length(),
            static_cast<size_t>(std::numeric_limits<int>::max()));
  const int input_len = static_cast<int>(input.length());

  int written = file_.WriteAtCurrentPos(input.c_str(), input_len);
  if (written != input_len) {
    LOG(WARNING) << "WebRTC event log couldn't be written to the "
                    "locally stored file in its entirety.";
    return false;
  }

  budget_.Consume(static_cast<size_t>(written));

  return true;
}

bool BaseLogFileWriter::Finalize() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(state(), State::CLOSED);
  DCHECK_NE(state(), State::DELETED);
  DCHECK_NE(state(), State::ERRORED);
  return true;
}

}  // namespace

const base::FilePath::CharType kWebRtcEventLogUncompressedExtension[] =
    FILE_PATH_LITERAL("log");

size_t BaseLogFileWriterFactory::MinFileSizeBytes() const {
  // No overhead incurred; data written straight to the file without metadata.
  return 0;
}

base::FilePath::StringPieceType BaseLogFileWriterFactory::Extension() const {
  return kWebRtcEventLogUncompressedExtension;
}

std::unique_ptr<LogFileWriter> BaseLogFileWriterFactory::Create(
    const base::FilePath& path,
    base::Optional<size_t> max_file_size_bytes) const {
  if (max_file_size_bytes.has_value() &&
      max_file_size_bytes.value() < MinFileSizeBytes()) {
    LOG(WARNING) << "Max size (" << max_file_size_bytes.value()
                 << ") below minimum size (" << MinFileSizeBytes() << ").";
    return nullptr;
  }

  auto result = std::make_unique<BaseLogFileWriter>(path, max_file_size_bytes);

  if (!result->Init()) {
    // Error logged by Init.
    result.reset();  // Destructor deletes errored files.
  }

  return result;
}

BrowserContextId GetBrowserContextId(
    const content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return reinterpret_cast<BrowserContextId>(browser_context);
}

BrowserContextId GetBrowserContextId(int render_process_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* const host =
      content::RenderProcessHost::FromID(render_process_id);

  content::BrowserContext* const browser_context =
      host ? host->GetBrowserContext() : nullptr;

  return GetBrowserContextId(browser_context);
}

base::FilePath GetRemoteBoundWebRtcEventLogsDir(
    const base::FilePath& browser_context_dir) {
  const base::FilePath::CharType kRemoteBoundLogSubDirectory[] =
      FILE_PATH_LITERAL("webrtc_event_logs");
  return browser_context_dir.Append(kRemoteBoundLogSubDirectory);
}
