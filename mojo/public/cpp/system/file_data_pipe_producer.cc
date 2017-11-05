// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/file_data_pipe_producer.h"

#include <algorithm>
#include <limits>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "mojo/public/cpp/system/simple_watcher.h"

namespace mojo {

namespace {

// No good reason not to attempt very large pipe transactions in case the data
// pipe in use has a very large capacity available, so we default to trying
// 64 MB chunks whenever a producer is writable.
constexpr uint32_t kDefaultMaxReadSize = 64 * 1024 * 1024;

}  // namespace

class FileDataPipeProducer::FileSequenceState
    : public base::RefCountedDeleteOnSequence<FileSequenceState> {
 public:
  using CompletionCallback =
      base::OnceCallback<void(ScopedDataPipeProducerHandle producer,
                              MojoResult result)>;

  FileSequenceState(
      ScopedDataPipeProducerHandle producer_handle,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner,
      CompletionCallback callback,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner)
      : base::RefCountedDeleteOnSequence<FileSequenceState>(file_task_runner),
        file_task_runner_(std::move(file_task_runner)),
        callback_task_runner_(std::move(callback_task_runner)),
        producer_handle_(std::move(producer_handle)),
        callback_(std::move(callback)) {}

  void Cancel() {
    base::AutoLock lock(lock_);
    is_cancelled_ = true;
  }

  void StartFromFile(base::File file, size_t max_bytes) {
    file_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FileSequenceState::StartFromFileOnFileSequence, this,
                       std::move(file), max_bytes));
  }

  void StartFromPath(const base::FilePath& path) {
    file_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&FileSequenceState::StartFromPathOnFileSequence, this,
                       path));
  }

 private:
  friend class base::DeleteHelper<FileSequenceState>;
  friend class base::RefCountedDeleteOnSequence<FileSequenceState>;

  ~FileSequenceState() = default;

  void StartFromFileOnFileSequence(base::File file, size_t max_bytes) {
    file_ = std::move(file);
    max_bytes_ = max_bytes;
    TransferSomeBytes();
    if (producer_handle_.is_valid()) {
      // If we didn't nail it all on the first transaction attempt, setup a
      // watcher and complete the read asynchronously.
      watcher_ = base::MakeUnique<SimpleWatcher>(
          FROM_HERE, SimpleWatcher::ArmingPolicy::AUTOMATIC);
      watcher_->Watch(producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                      MOJO_WATCH_CONDITION_SATISFIED,
                      base::Bind(&FileSequenceState::OnHandleReady, this));
    }
  }

  void StartFromPathOnFileSequence(const base::FilePath& path) {
    StartFromFileOnFileSequence(
        base::File(path, base::File::FLAG_OPEN | base::File::FLAG_READ),
        std::numeric_limits<size_t>::max());
  }

  void OnHandleReady(MojoResult result, const HandleSignalsState& state) {
    {
      // Stop ourselves from doing redundant work if we've been cancelled from
      // another thread. Note that we do not rely on this for any kind of thread
      // safety concerns.
      base::AutoLock lock(lock_);
      if (is_cancelled_)
        return;
    }

    if (result != MOJO_RESULT_OK) {
      // Either the consumer pipe has been closed or something terrible
      // happened. In any case, we'll never be able to write more data.
      Finish(MOJO_RESULT_ABORTED);
      return;
    }

    TransferSomeBytes();
  }

  void TransferSomeBytes() {
    while (true) {
      // Lock as much of the pipe as we can.
      void* pipe_buffer;
      uint32_t size = kDefaultMaxReadSize;
      MojoResult result = producer_handle_->BeginWriteData(
          &pipe_buffer, &size, MOJO_WRITE_DATA_FLAG_NONE);
      if (result == MOJO_RESULT_SHOULD_WAIT)
        return;
      if (result != MOJO_RESULT_OK) {
        Finish(MOJO_RESULT_ABORTED);
        return;
      }

      // Attempt to read that many bytes from the file, directly into the data
      // pipe. Note that while |max_bytes_remaining| may be very large, the
      // length we attempt read is bounded by the much smaller
      // |kDefaultMaxReadSize| via |size|.
      DCHECK(base::IsValueInRangeForNumericType<int>(size));
      const size_t max_bytes_remaining = max_bytes_ - bytes_transferred_;
      int attempted_read_size = static_cast<int>(
          std::min(static_cast<size_t>(size), max_bytes_remaining));
      int read_size = file_.ReadAtCurrentPos(static_cast<char*>(pipe_buffer),
                                             attempted_read_size);
      producer_handle_->EndWriteData(
          read_size >= 0 ? static_cast<uint32_t>(read_size) : 0);

      if (read_size < 0) {
        Finish(MOJO_RESULT_ABORTED);
        return;
      }

      bytes_transferred_ += read_size;
      DCHECK_LE(bytes_transferred_, max_bytes_);

      if (read_size < attempted_read_size) {
        // ReadAtCurrentPos makes a best effort to read all requested bytes. We
        // reasonably assume if it fails to read what we ask for, we've hit EOF.
        Finish(MOJO_RESULT_OK);
        return;
      }

      if (bytes_transferred_ == max_bytes_) {
        // We've read as much as we were asked to read.
        Finish(MOJO_RESULT_OK);
        return;
      }
    }
  }

  void Finish(MojoResult result) {
    watcher_.reset();
    callback_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_),
                                  std::move(producer_handle_), result));
  }

  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  // State which is effectively owned and used only on the file sequence.
  ScopedDataPipeProducerHandle producer_handle_;
  base::File file_;
  size_t max_bytes_ = 0;
  size_t bytes_transferred_ = 0;
  CompletionCallback callback_;
  std::unique_ptr<SimpleWatcher> watcher_;

  // Guards |is_cancelled_|.
  base::Lock lock_;
  bool is_cancelled_ = false;

  DISALLOW_COPY_AND_ASSIGN(FileSequenceState);
};

FileDataPipeProducer::FileDataPipeProducer(
    ScopedDataPipeProducerHandle producer)
    : producer_(std::move(producer)), weak_factory_(this) {}

FileDataPipeProducer::~FileDataPipeProducer() {
  if (file_sequence_state_)
    file_sequence_state_->Cancel();
}

void FileDataPipeProducer::WriteFromFile(base::File file,
                                         CompletionCallback callback) {
  WriteFromFile(std::move(file), std::numeric_limits<size_t>::max(),
                std::move(callback));
}

void FileDataPipeProducer::WriteFromFile(base::File file,
                                         size_t max_bytes,
                                         CompletionCallback callback) {
  InitializeNewRequest(std::move(callback));
  file_sequence_state_->StartFromFile(std::move(file), max_bytes);
}

void FileDataPipeProducer::WriteFromPath(const base::FilePath& path,
                                         CompletionCallback callback) {
  InitializeNewRequest(std::move(callback));
  file_sequence_state_->StartFromPath(path);
}

void FileDataPipeProducer::InitializeNewRequest(CompletionCallback callback) {
  DCHECK(!file_sequence_state_);
  auto file_task_runner = base::CreateSequencedTaskRunnerWithTraits(
      {base::MayBlock(), base::TaskPriority::BACKGROUND});
  file_sequence_state_ = new FileSequenceState(
      std::move(producer_), file_task_runner,
      base::BindOnce(&FileDataPipeProducer::OnWriteComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      base::SequencedTaskRunnerHandle::Get());
}

void FileDataPipeProducer::OnWriteComplete(
    CompletionCallback callback,
    ScopedDataPipeProducerHandle producer,
    MojoResult ready_result) {
  producer_ = std::move(producer);
  file_sequence_state_ = nullptr;
  std::move(callback).Run(ready_result);
}

}  // namespace mojo
