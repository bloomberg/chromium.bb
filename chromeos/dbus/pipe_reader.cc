// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/pipe_reader.h"

#include "base/bind.h"
#include "base/posix/eintr_wrapper.h"
#include "base/task_runner.h"
#include "net/base/file_stream.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace chromeos {

PipeReader::PipeReader(const scoped_refptr<base::TaskRunner>& task_runner,
                       const IOCompleteCallback& callback)
    : io_buffer_(new net::IOBufferWithSize(4096)),
      task_runner_(task_runner),
      callback_(callback),
      weak_ptr_factory_(this) {}

PipeReader::~PipeReader() {
}

base::ScopedFD PipeReader::StartIO() {
  // Use a pipe to collect data
  int pipe_fds[2];
  const int status = HANDLE_EINTR(pipe(pipe_fds));
  if (status < 0) {
    PLOG(ERROR) << "pipe";
    return base::ScopedFD();
  }
  base::ScopedFD pipe_write_end(pipe_fds[1]);
  // Pass ownership of pipe_fds[0] to data_stream_, which will close it.
  data_stream_.reset(new net::FileStream(
      base::File(pipe_fds[0]), task_runner_));

  // Post an initial async read to setup data collection
  int rv = data_stream_->Read(
      io_buffer_.get(), io_buffer_->size(),
      base::Bind(&PipeReader::OnDataReady, weak_ptr_factory_.GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING) {
    LOG(ERROR) << "Unable to post initial read";
    return base::ScopedFD();
  }
  return pipe_write_end;
}

void PipeReader::OnDataReady(int byte_count) {
  DVLOG(1) << "OnDataReady byte_count " << byte_count;
  if (byte_count <= 0) {
    callback_.Run();  // signal creator to take data and delete us
    return;
  }

  AcceptData(io_buffer_->data(), byte_count);

  // Post another read
  int rv = data_stream_->Read(
      io_buffer_.get(), io_buffer_->size(),
      base::Bind(&PipeReader::OnDataReady, weak_ptr_factory_.GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING) {
    LOG(ERROR) << "Unable to post another read";
    // TODO(sleffler) do something more intelligent?
  }
}

PipeReaderForString::PipeReaderForString(
    const scoped_refptr<base::TaskRunner>& task_runner,
    const IOCompleteCallback& callback)
    : PipeReader(task_runner, callback) {
}

void PipeReaderForString::AcceptData(const char *data, int byte_count) {
  data_.append(data, byte_count);
}

void PipeReaderForString::GetData(std::string* data) {
  data_.swap(*data);
}

}  // namespace chromeos
