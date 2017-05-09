// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/chromium/quic_chromium_client_stream.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_event_type.h"
#include "net/quic/chromium/quic_chromium_client_session.h"
#include "net/quic/chromium/quic_http_utils.h"
#include "net/quic/core/quic_spdy_session.h"
#include "net/quic/core/quic_write_blocked_list.h"
#include "net/quic/core/spdy_utils.h"

namespace net {

QuicChromiumClientStream::QuicChromiumClientStream(
    QuicStreamId id,
    QuicClientSessionBase* session,
    const NetLogWithSource& net_log)
    : QuicSpdyStream(id, session),
      net_log_(net_log),
      delegate_(nullptr),
      headers_delivered_(false),
      initial_headers_sent_(false),
      session_(session),
      can_migrate_(true),
      initial_headers_frame_len_(0),
      weak_factory_(this) {}

QuicChromiumClientStream::~QuicChromiumClientStream() {
  if (delegate_)
    delegate_->OnClose();
}

void QuicChromiumClientStream::OnInitialHeadersComplete(
    bool fin,
    size_t frame_len,
    const QuicHeaderList& header_list) {
  QuicSpdyStream::OnInitialHeadersComplete(fin, frame_len, header_list);

  SpdyHeaderBlock header_block;
  int64_t length = -1;
  if (!SpdyUtils::CopyAndValidateHeaders(header_list, &length, &header_block)) {
    DLOG(ERROR) << "Failed to parse header list: " << header_list.DebugString();
    ConsumeHeaderList();
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
    return;
  }

  ConsumeHeaderList();
  session_->OnInitialHeadersComplete(id(), header_block);

  if (delegate_) {
    // The delegate will receive the headers via a posted task.
    NotifyDelegateOfInitialHeadersAvailableLater(std::move(header_block),
                                                 frame_len);
    return;
  }

  // Buffer the headers and deliver them when the delegate arrives.
  initial_headers_ = std::move(header_block);
  initial_headers_frame_len_ = frame_len;
}

void QuicChromiumClientStream::OnTrailingHeadersComplete(
    bool fin,
    size_t frame_len,
    const QuicHeaderList& header_list) {
  QuicSpdyStream::OnTrailingHeadersComplete(fin, frame_len, header_list);
  NotifyDelegateOfTrailingHeadersAvailableLater(received_trailers().Clone(),
                                                frame_len);
}

void QuicChromiumClientStream::OnPromiseHeaderList(
    QuicStreamId promised_id,
    size_t frame_len,
    const QuicHeaderList& header_list) {
  SpdyHeaderBlock promise_headers;
  int64_t content_length = -1;
  if (!SpdyUtils::CopyAndValidateHeaders(header_list, &content_length,
                                         &promise_headers)) {
    DLOG(ERROR) << "Failed to parse header list: " << header_list.DebugString();
    ConsumeHeaderList();
    Reset(QUIC_BAD_APPLICATION_PAYLOAD);
    return;
  }
  ConsumeHeaderList();

  session_->HandlePromised(id(), promised_id, promise_headers);
}

void QuicChromiumClientStream::OnDataAvailable() {
  if (!FinishedReadingHeaders() || !headers_delivered_) {
    // Buffer the data in the sequencer until the headers have been read.
    return;
  }

  if (!sequencer()->HasBytesToRead() && !FinishedReadingTrailers()) {
    // If there is no data to read, wait until either FIN is received or
    // trailers are delivered.
    return;
  }

  // The delegate will read the data via a posted task, and
  // will be able to, potentially, read all data which has queued up.
  if (delegate_)
    NotifyDelegateOfDataAvailableLater();
}

void QuicChromiumClientStream::OnClose() {
  if (delegate_) {
    delegate_->OnClose();
    delegate_ = nullptr;
  }
  QuicStream::OnClose();
}

void QuicChromiumClientStream::OnCanWrite() {
  QuicStream::OnCanWrite();

  if (!HasBufferedData() && !write_callback_.is_null()) {
    base::ResetAndReturn(&write_callback_).Run(OK);
  }
}

size_t QuicChromiumClientStream::WriteHeaders(
    SpdyHeaderBlock header_block,
    bool fin,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
  if (!session()->IsCryptoHandshakeConfirmed()) {
    auto entry = header_block.find(":method");
    DCHECK(entry != header_block.end());
    DCHECK_NE("POST", entry->second);
  }
  net_log_.AddEvent(
      NetLogEventType::QUIC_CHROMIUM_CLIENT_STREAM_SEND_REQUEST_HEADERS,
      base::Bind(&QuicRequestNetLogCallback, id(), &header_block,
                 QuicSpdyStream::priority()));
  size_t len = QuicSpdyStream::WriteHeaders(std::move(header_block), fin,
                                            std::move(ack_listener));
  initial_headers_sent_ = true;
  return len;
}

SpdyPriority QuicChromiumClientStream::priority() const {
  return initial_headers_sent_ ? QuicSpdyStream::priority()
                               : kV3HighestPriority;
}

int QuicChromiumClientStream::WriteStreamData(
    QuicStringPiece data,
    bool fin,
    const CompletionCallback& callback) {
  // We should not have data buffered.
  DCHECK(!HasBufferedData());
  // Writes the data, or buffers it.
  WriteOrBufferData(data, fin, nullptr);
  if (!HasBufferedData()) {
    return OK;
  }

  write_callback_ = callback;
  return ERR_IO_PENDING;
}

int QuicChromiumClientStream::WritevStreamData(
    const std::vector<scoped_refptr<IOBuffer>>& buffers,
    const std::vector<int>& lengths,
    bool fin,
    const CompletionCallback& callback) {
  // Must not be called when data is buffered.
  DCHECK(!HasBufferedData());
  // Writes the data, or buffers it.
  for (size_t i = 0; i < buffers.size(); ++i) {
    bool is_fin = fin && (i == buffers.size() - 1);
    QuicStringPiece string_data(buffers[i]->data(), lengths[i]);
    WriteOrBufferData(string_data, is_fin, nullptr);
  }
  if (!HasBufferedData()) {
    return OK;
  }

  write_callback_ = callback;
  return ERR_IO_PENDING;
}

void QuicChromiumClientStream::SetDelegate(
    QuicChromiumClientStream::Delegate* delegate) {
  DCHECK(!(delegate_ && delegate));
  delegate_ = delegate;
  if (delegate == nullptr)
    return;

  // Should this perhaps be via PostTask to make reasoning simpler?
  if (!initial_headers_.empty()) {
    delegate_->OnInitialHeadersAvailable(std::move(initial_headers_),
                                         initial_headers_frame_len_);
  }
}

void QuicChromiumClientStream::OnError(int error) {
  if (delegate_) {
    QuicChromiumClientStream::Delegate* delegate = delegate_;
    delegate_ = nullptr;
    delegate->OnError(error);
  }
}

int QuicChromiumClientStream::Read(IOBuffer* buf, int buf_len) {
  if (IsDoneReading())
    return 0;  // EOF

  if (!HasBytesToRead())
    return ERR_IO_PENDING;

  iovec iov;
  iov.iov_base = buf->data();
  iov.iov_len = buf_len;
  size_t bytes_read = Readv(&iov, 1);
  // Since HasBytesToRead is true, Readv() must of read some data.
  DCHECK_NE(0u, bytes_read);
  return bytes_read;
}

void QuicChromiumClientStream::NotifyDelegateOfInitialHeadersAvailableLater(
    SpdyHeaderBlock headers,
    size_t frame_len) {
  DCHECK(delegate_);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(
          &QuicChromiumClientStream::NotifyDelegateOfInitialHeadersAvailable,
          weak_factory_.GetWeakPtr(), base::Passed(std::move(headers)),
          frame_len));
}

void QuicChromiumClientStream::NotifyDelegateOfInitialHeadersAvailable(
    SpdyHeaderBlock headers,
    size_t frame_len) {
  if (!delegate_)
    return;

  DCHECK(!headers_delivered_);
  headers_delivered_ = true;
  net_log_.AddEvent(
      NetLogEventType::QUIC_CHROMIUM_CLIENT_STREAM_READ_RESPONSE_HEADERS,
      base::Bind(&SpdyHeaderBlockNetLogCallback, &headers));

  delegate_->OnInitialHeadersAvailable(headers, frame_len);
}

void QuicChromiumClientStream::NotifyDelegateOfTrailingHeadersAvailableLater(
    SpdyHeaderBlock headers,
    size_t frame_len) {
  DCHECK(delegate_);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(
          &QuicChromiumClientStream::NotifyDelegateOfTrailingHeadersAvailable,
          weak_factory_.GetWeakPtr(), base::Passed(std::move(headers)),
          frame_len));
}

void QuicChromiumClientStream::NotifyDelegateOfTrailingHeadersAvailable(
    SpdyHeaderBlock headers,
    size_t frame_len) {
  if (!delegate_)
    return;

  DCHECK(headers_delivered_);
  // Only mark trailers consumed when we are about to notify delegate.
  MarkTrailersConsumed();
  // Post an async task to notify delegate of the FIN flag.
  NotifyDelegateOfDataAvailableLater();
  net_log_.AddEvent(
      NetLogEventType::QUIC_CHROMIUM_CLIENT_STREAM_READ_RESPONSE_TRAILERS,
      base::Bind(&SpdyHeaderBlockNetLogCallback, &headers));

  delegate_->OnTrailingHeadersAvailable(headers, frame_len);
}

void QuicChromiumClientStream::NotifyDelegateOfDataAvailableLater() {
  DCHECK(delegate_);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&QuicChromiumClientStream::NotifyDelegateOfDataAvailable,
                 weak_factory_.GetWeakPtr()));
}

void QuicChromiumClientStream::NotifyDelegateOfDataAvailable() {
  if (delegate_)
    delegate_->OnDataAvailable();
}

void QuicChromiumClientStream::DisableConnectionMigration() {
  can_migrate_ = false;
}

bool QuicChromiumClientStream::IsFirstStream() {
  return id() == kHeadersStreamId + 2;
}

}  // namespace net
