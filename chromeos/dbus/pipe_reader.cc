// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/pipe_reader.h"

#include "base/bind.h"
#include "base/platform_file.h"
#include "base/posix/eintr_wrapper.h"
#include "net/base/net_errors.h"

namespace chromeos {

PipeReader::PipeReader(PipeReader::IOCompleteCallback callback)
    : write_fd_(-1),
      io_buffer_(new net::IOBufferWithSize(4096)),
      callback_(callback),
      weak_ptr_factory_(this) {}

PipeReader::~PipeReader() {
  CloseWriteFD();
}

void PipeReader::CloseWriteFD() {
  if (write_fd_ == -1)
    return;
  if (IGNORE_EINTR(close(write_fd_)) < 0)
    PLOG(ERROR) << "close";
  write_fd_ = -1;
}

bool PipeReader::StartIO() {
  // Use a pipe to collect data
  int pipe_fds[2];
  const int status = HANDLE_EINTR(pipe(pipe_fds));
  if (status < 0) {
    PLOG(ERROR) << "pipe";
    return false;
  }
  write_fd_ = pipe_fds[1];
  base::PlatformFile data_file_ = pipe_fds[0];
  // Pass ownership of pipe_fds[0] to data_stream_, which will will close it.
  data_stream_.reset(new net::FileStream(
      data_file_,
      base::PLATFORM_FILE_READ | base::PLATFORM_FILE_ASYNC,
      NULL));

  // Post an initial async read to setup data collection
  int rv = data_stream_->Read(
      io_buffer_.get(), io_buffer_->size(),
      base::Bind(&PipeReader::OnDataReady, weak_ptr_factory_.GetWeakPtr()));
  if (rv != net::ERR_IO_PENDING) {
    LOG(ERROR) << "Unable to post initial read";
    return false;
  }
  return true;
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
    PipeReader::IOCompleteCallback callback) : PipeReader(callback) {}

void PipeReaderForString::AcceptData(const char *data, int byte_count) {
  data_.append(data, byte_count);
}


void PipeReaderForString::GetData(std::string* data) {
  data_.swap(*data);
}

}  // namespace chromeos
