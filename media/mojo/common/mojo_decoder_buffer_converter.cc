// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/mojo_decoder_buffer_converter.h"

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/audio_buffer.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/mojo/common/media_type_converters.h"

namespace media {

namespace {

std::unique_ptr<mojo::DataPipe> CreateDataPipe(DemuxerStream::Type type) {
  uint32_t capacity = 0;

  if (type == DemuxerStream::AUDIO) {
    // TODO(timav): Consider capacity calculation based on AudioDecoderConfig.
    capacity = 512 * 1024;
  } else if (type == DemuxerStream::VIDEO) {
    // Video can get quite large; at 4K, VP9 delivers packets which are ~1MB in
    // size; so allow for some head room.
    // TODO(xhwang, sandersd): Provide a better way to customize this value.
    capacity = 2 * (1024 * 1024);
  } else {
    NOTREACHED() << "Unsupported type: " << type;
    // Choose an arbitrary size.
    capacity = 512 * 1024;
  }

  return base::MakeUnique<mojo::DataPipe>(capacity);
}

bool IsPipeReadWriteError(MojoResult result) {
  return result != MOJO_RESULT_OK && result != MOJO_RESULT_SHOULD_WAIT;
}

}  // namespace

// MojoDecoderBufferReader

// static
std::unique_ptr<MojoDecoderBufferReader> MojoDecoderBufferReader::Create(
    DemuxerStream::Type type,
    mojo::ScopedDataPipeProducerHandle* producer_handle) {
  DVLOG(1) << __func__;
  std::unique_ptr<mojo::DataPipe> data_pipe = CreateDataPipe(type);
  *producer_handle = std::move(data_pipe->producer_handle);
  return base::WrapUnique(
      new MojoDecoderBufferReader(std::move(data_pipe->consumer_handle)));
}

MojoDecoderBufferReader::MojoDecoderBufferReader(
    mojo::ScopedDataPipeConsumerHandle consumer_handle)
    : consumer_handle_(std::move(consumer_handle)),
      pipe_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      armed_(false),
      bytes_read_(0) {
  DVLOG(1) << __func__;

  MojoResult result =
      pipe_watcher_.Watch(consumer_handle_.get(), MOJO_HANDLE_SIGNAL_READABLE,
                          MOJO_WATCH_CONDITION_SATISFIED,
                          base::Bind(&MojoDecoderBufferReader::OnPipeReadable,
                                     base::Unretained(this)));
  if (result != MOJO_RESULT_OK) {
    DVLOG(1) << __func__
             << ": Failed to start watching the pipe. result=" << result;
    consumer_handle_.reset();
  }
}

MojoDecoderBufferReader::~MojoDecoderBufferReader() {
  DVLOG(1) << __func__;
}

void MojoDecoderBufferReader::CancelReadCB(ReadCB read_cb) {
  DVLOG(1) << "Failed to read DecoderBuffer becuase the pipe is already closed";
  std::move(read_cb).Run(nullptr);
}

void MojoDecoderBufferReader::CompleteCurrentRead() {
  DVLOG(4) << __func__;
  bytes_read_ = 0;
  ReadCB read_cb = std::move(pending_read_cbs_.front());
  pending_read_cbs_.pop_front();
  scoped_refptr<DecoderBuffer> buffer = std::move(pending_buffers_.front());
  pending_buffers_.pop_front();
  std::move(read_cb).Run(std::move(buffer));
}

void MojoDecoderBufferReader::ScheduleNextRead() {
  DVLOG(4) << __func__;

  // Do nothing if a read is already scheduled.
  if (armed_)
    return;

  // Immediately complete empty reads.
  // A non-EOS buffer can have zero size. See http://crbug.com/663438
  while (!pending_buffers_.empty() &&
         (pending_buffers_.front()->end_of_stream() ||
          pending_buffers_.front()->data_size() == 0)) {
    // TODO(sandersd): Make sure there are no possible re-entrancy issues here.
    // Perhaps read callbacks should be posted?
    CompleteCurrentRead();
  }

  // Request a callback to issue the DataPipe read.
  if (!pending_buffers_.empty()) {
    armed_ = true;
    pipe_watcher_.ArmOrNotify();
  }
}

void MojoDecoderBufferReader::ReadDecoderBuffer(
    mojom::DecoderBufferPtr mojo_buffer,
    ReadCB read_cb) {
  DVLOG(3) << __func__;

  if (!consumer_handle_.is_valid()) {
    DCHECK(pending_read_cbs_.empty());
    CancelReadCB(std::move(read_cb));
    return;
  }

  scoped_refptr<DecoderBuffer> media_buffer(
      mojo_buffer.To<scoped_refptr<DecoderBuffer>>());
  DCHECK(media_buffer);

  // We don't want reads to complete out of order, so we queue them even if they
  // are zero-sized.
  pending_read_cbs_.push_back(std::move(read_cb));
  pending_buffers_.push_back(std::move(media_buffer));
  ScheduleNextRead();
}

void MojoDecoderBufferReader::OnPipeReadable(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DVLOG(4) << __func__ << "(" << result << ", " << state.readable() << ")";

  armed_ = false;

  if (result != MOJO_RESULT_OK) {
    OnPipeError(result);
    return;
  }

  DCHECK(state.readable());
  ReadDecoderBufferData();
}

void MojoDecoderBufferReader::ReadDecoderBufferData() {
  DVLOG(4) << __func__;

  DCHECK(!pending_buffers_.empty());
  DecoderBuffer* buffer = pending_buffers_.front().get();
  uint32_t buffer_size = base::checked_cast<uint32_t>(buffer->data_size());
  DCHECK_GT(buffer_size, 0u);

  uint32_t num_bytes = buffer_size - bytes_read_;
  DCHECK_GT(num_bytes, 0u);

  MojoResult result =
      consumer_handle_->ReadData(buffer->writable_data() + bytes_read_,
                                 &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);

  if (IsPipeReadWriteError(result)) {
    OnPipeError(result);
  } else {
    if (result == MOJO_RESULT_OK) {
      DCHECK_GT(num_bytes, 0u);
      bytes_read_ += num_bytes;
      // TODO(sandersd): Make sure there are no possible re-entrancy issues
      // here.
      if (bytes_read_ == buffer_size)
        CompleteCurrentRead();
    }
    ScheduleNextRead();
  }
}

void MojoDecoderBufferReader::OnPipeError(MojoResult result) {
  DVLOG(1) << __func__ << "(" << result << ")";
  DCHECK(IsPipeReadWriteError(result));

  consumer_handle_.reset();

  if (!pending_buffers_.empty()) {
    DVLOG(1) << __func__ << ": reading from data pipe failed. result=" << result
             << ", buffer size=" << pending_buffers_.front()->data_size()
             << ", num_bytes(read)=" << bytes_read_;
    bytes_read_ = 0;
    pending_buffers_.clear();
    while (!pending_read_cbs_.empty()) {
      ReadCB read_cb = std::move(pending_read_cbs_.front());
      pending_read_cbs_.pop_front();
      // TODO(sandersd): Make sure there are no possible re-entrancy issues
      // here. Perhaps these should be posted, or merged into a single error
      // callback?
      CancelReadCB(std::move(read_cb));
    }
  }
}

// MojoDecoderBufferWriter

// static
std::unique_ptr<MojoDecoderBufferWriter> MojoDecoderBufferWriter::Create(
    DemuxerStream::Type type,
    mojo::ScopedDataPipeConsumerHandle* consumer_handle) {
  DVLOG(1) << __func__;
  std::unique_ptr<mojo::DataPipe> data_pipe = CreateDataPipe(type);
  *consumer_handle = std::move(data_pipe->consumer_handle);
  return base::WrapUnique(
      new MojoDecoderBufferWriter(std::move(data_pipe->producer_handle)));
}

MojoDecoderBufferWriter::MojoDecoderBufferWriter(
    mojo::ScopedDataPipeProducerHandle producer_handle)
    : producer_handle_(std::move(producer_handle)),
      pipe_watcher_(FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL),
      armed_(false),
      bytes_written_(0) {
  DVLOG(1) << __func__;

  MojoResult result =
      pipe_watcher_.Watch(producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
                          MOJO_WATCH_CONDITION_SATISFIED,
                          base::Bind(&MojoDecoderBufferWriter::OnPipeWritable,
                                     base::Unretained(this)));
  if (result != MOJO_RESULT_OK) {
    DVLOG(1) << __func__
             << ": Failed to start watching the pipe. result=" << result;
    producer_handle_.reset();
  }
}

MojoDecoderBufferWriter::~MojoDecoderBufferWriter() {
  DVLOG(1) << __func__;
}

void MojoDecoderBufferWriter::ScheduleNextWrite() {
  DVLOG(4) << __func__;

  // Do nothing if a write is already scheduled.
  if (armed_)
    return;

  // Request a callback to issue the DataPipe write.
  if (!pending_buffers_.empty()) {
    armed_ = true;
    pipe_watcher_.ArmOrNotify();
  }
}

mojom::DecoderBufferPtr MojoDecoderBufferWriter::WriteDecoderBuffer(
    const scoped_refptr<DecoderBuffer>& media_buffer) {
  DVLOG(3) << __func__;

  // DecoderBuffer cannot be written if the pipe is already closed.
  if (!producer_handle_.is_valid()) {
    DVLOG(1)
        << __func__
        << ": Failed to write DecoderBuffer becuase the pipe is already closed";
    return nullptr;
  }

  mojom::DecoderBufferPtr mojo_buffer =
      mojom::DecoderBuffer::From(media_buffer);

  // A non-EOS buffer can have zero size. See http://crbug.com/663438
  if (media_buffer->end_of_stream() || media_buffer->data_size() == 0)
    return mojo_buffer;

  // Queue writing the buffer's data into our DataPipe.
  pending_buffers_.push_back(media_buffer);
  ScheduleNextWrite();
  return mojo_buffer;
}

void MojoDecoderBufferWriter::OnPipeWritable(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  DVLOG(4) << __func__ << "(" << result << ", " << state.writable() << ")";

  armed_ = false;

  if (result != MOJO_RESULT_OK) {
    OnPipeError(result);
    return;
  }

  DCHECK(state.writable());
  WriteDecoderBufferData();
}

void MojoDecoderBufferWriter::WriteDecoderBufferData() {
  DVLOG(4) << __func__;

  DCHECK(!pending_buffers_.empty());
  DecoderBuffer* buffer = pending_buffers_.front().get();
  uint32_t buffer_size = base::checked_cast<uint32_t>(buffer->data_size());
  DCHECK_GT(buffer_size, 0u);

  uint32_t num_bytes = buffer_size - bytes_written_;
  DCHECK_GT(num_bytes, 0u);

  MojoResult result = producer_handle_->WriteData(
      buffer->data() + bytes_written_, &num_bytes, MOJO_WRITE_DATA_FLAG_NONE);

  if (IsPipeReadWriteError(result)) {
    OnPipeError(result);
  } else {
    if (result == MOJO_RESULT_OK) {
      DCHECK_GT(num_bytes, 0u);
      bytes_written_ += num_bytes;
      if (bytes_written_ == buffer_size) {
        pending_buffers_.pop_front();
        bytes_written_ = 0;
      }
    }
    ScheduleNextWrite();
  }
}

void MojoDecoderBufferWriter::OnPipeError(MojoResult result) {
  DVLOG(1) << __func__ << "(" << result << ")";
  DCHECK(IsPipeReadWriteError(result));

  producer_handle_.reset();

  if (!pending_buffers_.empty()) {
    DVLOG(1) << __func__ << ": writing to data pipe failed. result=" << result
             << ", buffer size=" << pending_buffers_.front()->data_size()
             << ", num_bytes(written)=" << bytes_written_;
    pending_buffers_.clear();
    bytes_written_ = 0;
  }
}

}  // namespace media
