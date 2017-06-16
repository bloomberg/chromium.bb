// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/devtools_io_context.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task_scheduler/post_task.h"
#include "base/third_party/icu/icu_utf.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_thread.h"

namespace content {

namespace {
unsigned s_last_stream_handle = 0;
}

using Stream = DevToolsIOContext::Stream;

Stream::Stream(base::SequencedTaskRunner* task_runner)
    : base::RefCountedDeleteOnSequence<Stream>(task_runner),
      handle_(base::UintToString(++s_last_stream_handle)),
      task_runner_(task_runner),
      had_errors_(false),
      last_read_pos_(0) {}

Stream::~Stream() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

bool Stream::InitOnFileSequenceIfNeeded() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  base::ThreadRestrictions::AssertIOAllowed();
  if (had_errors_)
    return false;
  if (file_.IsValid())
    return true;
  base::FilePath temp_path;
  if (!base::CreateTemporaryFile(&temp_path)) {
    LOG(ERROR) << "Failed to create temporary file";
    had_errors_ = true;
    return false;
  }
  const unsigned flags = base::File::FLAG_OPEN_TRUNCATED |
                         base::File::FLAG_WRITE | base::File::FLAG_READ |
                         base::File::FLAG_DELETE_ON_CLOSE;
  file_.Initialize(temp_path, flags);
  if (!file_.IsValid()) {
    LOG(ERROR) << "Failed to open temporary file: " << temp_path.value()
        << ", " << base::File::ErrorToString(file_.error_details());
    had_errors_ = true;
    DeleteFile(temp_path, false);
    return false;
  }
  return true;
}

void Stream::Read(off_t position, size_t max_size, ReadCallback callback) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Stream::ReadOnFileSequence, this, position,
                                max_size, std::move(callback)));
}

void Stream::Append(std::unique_ptr<std::string> data) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&Stream::AppendOnFileSequence, this, std::move(data)));
}

void Stream::ReadOnFileSequence(off_t position,
                                size_t max_size,
                                ReadCallback callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  Status status = StatusFailure;
  std::unique_ptr<std::string> data;

  if (file_.IsValid()) {
    std::string buffer;
    buffer.resize(max_size);
    if (position < 0)
      position = last_read_pos_;
    int size_got = file_.ReadNoBestEffort(position, &*buffer.begin(), max_size);
    if (size_got < 0) {
      LOG(ERROR) << "Failed to read temporary file";
      had_errors_ = true;
      file_.Close();
    } else {
      // Provided client has requested sufficient large block, make their
      // life easier by not truncating in the middle of a UTF-8 character.
      if (size_got > 6 && !CBU8_IS_SINGLE(buffer[size_got - 1])) {
        base::TruncateUTF8ToByteSize(buffer, size_got, &buffer);
        size_got = buffer.size();
      } else {
        buffer.resize(size_got);
      }
      data.reset(new std::string(std::move(buffer)));
      status = size_got ? StatusSuccess : StatusEOF;
      last_read_pos_ = position + size_got;
    }
  }
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::BindOnce(std::move(callback), std::move(data), status));
}

void Stream::AppendOnFileSequence(std::unique_ptr<std::string> data) {
  if (!InitOnFileSequenceIfNeeded())
    return;
  int size_written = file_.WriteAtCurrentPos(&*data->begin(), data->length());
  if (size_written != static_cast<int>(data->length())) {
    LOG(ERROR) << "Failed to write temporary file";
    had_errors_ = true;
    file_.Close();
  }
}

DevToolsIOContext::DevToolsIOContext() = default;

DevToolsIOContext::~DevToolsIOContext() = default;

scoped_refptr<Stream> DevToolsIOContext::CreateTempFileBackedStream() {
  if (!task_runner_) {
    task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
        {base::MayBlock(), base::TaskPriority::BACKGROUND});
  }
  scoped_refptr<Stream> result = new Stream(task_runner_.get());
  bool inserted =
      streams_.insert(std::make_pair(result->handle(), result)).second;
  DCHECK(inserted);
  return result;
}

scoped_refptr<Stream>
    DevToolsIOContext::GetByHandle(const std::string& handle) {
  StreamsMap::const_iterator it = streams_.find(handle);
  return it == streams_.end() ? scoped_refptr<Stream>() : it->second;
}

bool DevToolsIOContext::Close(const std::string& handle) {
  return streams_.erase(handle) == 1;
}

void DevToolsIOContext::DiscardAllStreams() {
  return streams_.clear();
}

}  // namespace content
