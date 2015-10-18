// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_reliable_client_stream.h"

#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/quic/quic_spdy_session.h"
#include "net/quic/quic_write_blocked_list.h"
#include "net/quic/spdy_utils.h"

namespace net {

QuicReliableClientStream::QuicReliableClientStream(QuicStreamId id,
                                                   QuicSpdySession* session,
                                                   const BoundNetLog& net_log)
    : QuicDataStream(id, session),
      net_log_(net_log),
      delegate_(nullptr),
      headers_delivered_(false),
      weak_factory_(this) {}

QuicReliableClientStream::~QuicReliableClientStream() {
  if (delegate_)
    delegate_->OnClose(connection_error());
}

void QuicReliableClientStream::OnStreamHeadersComplete(bool fin,
                                                       size_t frame_len) {
  QuicDataStream::OnStreamHeadersComplete(fin, frame_len);
  // The delegate will read the headers via a posted task.
  NotifyDelegateOfHeadersCompleteLater(frame_len);
}

void QuicReliableClientStream::OnDataAvailable() {
  // TODO(rch): buffer data if we don't have a delegate.
  if (!delegate_) {
    DLOG(ERROR) << "Missing delegate";
    Reset(QUIC_STREAM_CANCELLED);
    return;
  }

  if (!FinishedReadingHeaders() || !headers_delivered_) {
    // Buffer the data in the sequencer until the headers have been read.
    return;
  }

  // The delegate will read the data via a posted task, and
  // will be able to, potentially, read all data which has queued up.
  NotifyDelegateOfDataAvailableLater();
}

void QuicReliableClientStream::OnClose() {
  if (delegate_) {
    delegate_->OnClose(connection_error());
    delegate_ = nullptr;
  }
  ReliableQuicStream::OnClose();
}

void QuicReliableClientStream::OnCanWrite() {
  ReliableQuicStream::OnCanWrite();

  if (!HasBufferedData() && !callback_.is_null()) {
    base::ResetAndReturn(&callback_).Run(OK);
  }
}

QuicPriority QuicReliableClientStream::EffectivePriority() const {
  if (delegate_ && delegate_->HasSendHeadersComplete()) {
    return QuicDataStream::EffectivePriority();
  }
  return QuicWriteBlockedList::kHighestPriority;
}

int QuicReliableClientStream::WriteStreamData(
    base::StringPiece data,
    bool fin,
    const CompletionCallback& callback) {
  // We should not have data buffered.
  DCHECK(!HasBufferedData());
  // Writes the data, or buffers it.
  WriteOrBufferData(data, fin, nullptr);
  if (!HasBufferedData()) {
    return OK;
  }

  callback_ = callback;
  return ERR_IO_PENDING;
}

void QuicReliableClientStream::SetDelegate(
    QuicReliableClientStream::Delegate* delegate) {
  DCHECK(!(delegate_ && delegate));
  delegate_ = delegate;
  if (delegate == nullptr && sequencer()->IsClosed()) {
    OnFinRead();
  }
}

void QuicReliableClientStream::OnError(int error) {
  if (delegate_) {
    QuicReliableClientStream::Delegate* delegate = delegate_;
    delegate_ = nullptr;
    delegate->OnError(error);
  }
}

int QuicReliableClientStream::Read(IOBuffer* buf, int buf_len) {
  if (sequencer()->IsClosed())
    return 0;  // EOF

  if (!HasBytesToRead())
    return ERR_IO_PENDING;

  iovec iov;
  iov.iov_base = buf->data();
  iov.iov_len = buf_len;
  return Readv(&iov, 1);
}

bool QuicReliableClientStream::CanWrite(const CompletionCallback& callback) {
  bool can_write =  session()->connection()->CanWrite(HAS_RETRANSMITTABLE_DATA);
  if (!can_write) {
    session()->MarkConnectionLevelWriteBlocked(id(), EffectivePriority());
    DCHECK(callback_.is_null());
    callback_ = callback;
  }
  return can_write;
}

void QuicReliableClientStream::NotifyDelegateOfHeadersCompleteLater(
    size_t frame_len) {
  DCHECK(delegate_);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&QuicReliableClientStream::NotifyDelegateOfHeadersComplete,
                 weak_factory_.GetWeakPtr(), frame_len));
}

void QuicReliableClientStream::NotifyDelegateOfHeadersComplete(
    size_t frame_len) {
  if (!delegate_)
    return;

  size_t headers_len = decompressed_headers().length();
  SpdyHeaderBlock headers;
  SpdyFramer framer(HTTP2);
  size_t len = framer.ParseHeaderBlockInBuffer(decompressed_headers().data(),
                                               headers_len, &headers);
  MarkHeadersConsumed(headers_len);
  headers_delivered_ = true;
  if (len == 0 || len != headers_len) {
    DLOG(WARNING) << "Invalid headers";
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
    return;
  }

  delegate_->OnHeadersAvailable(headers, frame_len);
}

void QuicReliableClientStream::NotifyDelegateOfDataAvailableLater() {
  DCHECK(delegate_);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&QuicReliableClientStream::NotifyDelegateOfDataAvailable,
                 weak_factory_.GetWeakPtr()));
}

void QuicReliableClientStream::NotifyDelegateOfDataAvailable() {
  if (delegate_)
    delegate_->OnDataAvailable();
}

}  // namespace net
