// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_stream.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "net/spdy/spdy_buffer_producer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"

namespace net {

namespace {

base::Value* NetLogSpdyStreamErrorCallback(SpdyStreamId stream_id,
                                           int status,
                                           const std::string* description,
                                           NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("stream_id", static_cast<int>(stream_id));
  dict->SetInteger("status", status);
  dict->SetString("description", *description);
  return dict;
}

base::Value* NetLogSpdyStreamWindowUpdateCallback(
    SpdyStreamId stream_id,
    int32 delta,
    int32 window_size,
    NetLog::LogLevel /* log_level */) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetInteger("stream_id", stream_id);
  dict->SetInteger("delta", delta);
  dict->SetInteger("window_size", window_size);
  return dict;
}

bool ContainsUppercaseAscii(const std::string& str) {
  for (std::string::const_iterator i(str.begin()); i != str.end(); ++i) {
    if (*i >= 'A' && *i <= 'Z') {
      return true;
    }
  }
  return false;
}

}  // namespace

// A wrapper around a stream that calls into ProduceSynStreamFrame().
class SpdyStream::SynStreamBufferProducer : public SpdyBufferProducer {
 public:
  SynStreamBufferProducer(const base::WeakPtr<SpdyStream>& stream)
      : stream_(stream) {
    DCHECK(stream_.get());
  }

  virtual ~SynStreamBufferProducer() {}

  virtual scoped_ptr<SpdyBuffer> ProduceBuffer() OVERRIDE {
    if (!stream_.get()) {
      NOTREACHED();
      return scoped_ptr<SpdyBuffer>();
    }
    DCHECK_GT(stream_->stream_id(), 0u);
    return scoped_ptr<SpdyBuffer>(
        new SpdyBuffer(stream_->ProduceSynStreamFrame()));
  }

 private:
  const base::WeakPtr<SpdyStream> stream_;
};

SpdyStream::SpdyStream(SpdyStreamType type,
                       const base::WeakPtr<SpdySession>& session,
                       const GURL& url,
                       RequestPriority priority,
                       int32 initial_send_window_size,
                       int32 initial_recv_window_size,
                       const BoundNetLog& net_log)
    : type_(type),
      weak_ptr_factory_(this),
      in_do_loop_(false),
      continue_buffering_data_(type_ == SPDY_PUSH_STREAM),
      stream_id_(0),
      url_(url),
      priority_(priority),
      slot_(0),
      send_stalled_by_flow_control_(false),
      send_window_size_(initial_send_window_size),
      recv_window_size_(initial_recv_window_size),
      unacked_recv_window_bytes_(0),
      session_(session),
      delegate_(NULL),
      send_status_(
          (type_ == SPDY_PUSH_STREAM) ?
          NO_MORE_DATA_TO_SEND : MORE_DATA_TO_SEND),
      request_time_(base::Time::Now()),
      response_headers_status_(RESPONSE_HEADERS_ARE_INCOMPLETE),
      io_state_((type_ == SPDY_PUSH_STREAM) ? STATE_IDLE : STATE_NONE),
      response_status_(OK),
      net_log_(net_log),
      raw_received_bytes_(0),
      send_bytes_(0),
      recv_bytes_(0),
      just_completed_frame_type_(DATA),
      just_completed_frame_size_(0) {
  CHECK(type_ == SPDY_BIDIRECTIONAL_STREAM ||
        type_ == SPDY_REQUEST_RESPONSE_STREAM ||
        type_ == SPDY_PUSH_STREAM);
  CHECK_GE(priority_, MINIMUM_PRIORITY);
  CHECK_LE(priority_, MAXIMUM_PRIORITY);
}

SpdyStream::~SpdyStream() {
  CHECK(!in_do_loop_);
  UpdateHistograms();
}

void SpdyStream::SetDelegate(Delegate* delegate) {
  CHECK(!delegate_);
  CHECK(delegate);
  delegate_ = delegate;

  if (type_ == SPDY_PUSH_STREAM) {
    DCHECK(continue_buffering_data_);
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&SpdyStream::PushedStreamReplayData, GetWeakPtr()));
  }
}

void SpdyStream::PushedStreamReplayData() {
  DCHECK_EQ(type_, SPDY_PUSH_STREAM);
  DCHECK_NE(stream_id_, 0u);
  DCHECK(continue_buffering_data_);

  continue_buffering_data_ = false;

  // The delegate methods called below may delete |this|, so use
  // |weak_this| to detect that.
  base::WeakPtr<SpdyStream> weak_this = GetWeakPtr();

  CHECK(delegate_);
  SpdyResponseHeadersStatus status =
      delegate_->OnResponseHeadersUpdated(response_headers_);
  if (status == RESPONSE_HEADERS_ARE_INCOMPLETE) {
    // Since RESPONSE_HEADERS_ARE_INCOMPLETE was returned, we must not
    // have been closed. Since we don't have complete headers, assume
    // we're waiting for another HEADERS frame, and we had better not
    // have any pending data frames.
    CHECK(weak_this);
    if (!pending_buffers_.empty()) {
      LogStreamError(ERR_SPDY_PROTOCOL_ERROR,
                     "Data received with incomplete headers.");
      session_->CloseActiveStream(stream_id_, ERR_SPDY_PROTOCOL_ERROR);
    }
    return;
  }

  // OnResponseHeadersUpdated() may have closed |this|.
  if (!weak_this)
    return;

  response_headers_status_ = RESPONSE_HEADERS_ARE_COMPLETE;

  while (!pending_buffers_.empty()) {
    // Take ownership of the first element of |pending_buffers_|.
    scoped_ptr<SpdyBuffer> buffer(pending_buffers_.front());
    pending_buffers_.weak_erase(pending_buffers_.begin());

    bool eof = (buffer == NULL);

    CHECK(delegate_);
    delegate_->OnDataReceived(buffer.Pass());

    // OnDataReceived() may have closed |this|.
    if (!weak_this)
      return;

    if (eof) {
      DCHECK(pending_buffers_.empty());
      session_->CloseActiveStream(stream_id_, OK);
      DCHECK(!weak_this);
      // |pending_buffers_| is invalid at this point.
      break;
    }
  }
}

scoped_ptr<SpdyFrame> SpdyStream::ProduceSynStreamFrame() {
  CHECK_EQ(io_state_, STATE_SEND_REQUEST_HEADERS_COMPLETE);
  CHECK(request_headers_);
  CHECK_GT(stream_id_, 0u);

  SpdyControlFlags flags =
      (send_status_ == NO_MORE_DATA_TO_SEND) ?
      CONTROL_FLAG_FIN : CONTROL_FLAG_NONE;
  scoped_ptr<SpdyFrame> frame(session_->CreateSynStream(
      stream_id_, priority_, slot_, flags, *request_headers_));
  send_time_ = base::TimeTicks::Now();
  return frame.Pass();
}

void SpdyStream::DetachDelegate() {
  CHECK(!in_do_loop_);
  DCHECK(!IsClosed());
  delegate_ = NULL;
  Cancel();
}

void SpdyStream::AdjustSendWindowSize(int32 delta_window_size) {
  DCHECK_GE(session_->flow_control_state(), SpdySession::FLOW_CONTROL_STREAM);

  if (IsClosed())
    return;

  // Check for wraparound.
  if (send_window_size_ > 0) {
    DCHECK_LE(delta_window_size, kint32max - send_window_size_);
  }
  if (send_window_size_ < 0) {
    DCHECK_GE(delta_window_size, kint32min - send_window_size_);
  }
  send_window_size_ += delta_window_size;
  PossiblyResumeIfSendStalled();
}

void SpdyStream::OnWriteBufferConsumed(
    size_t frame_payload_size,
    size_t consume_size,
    SpdyBuffer::ConsumeSource consume_source) {
  DCHECK_GE(session_->flow_control_state(), SpdySession::FLOW_CONTROL_STREAM);
  if (consume_source == SpdyBuffer::DISCARD) {
    // If we're discarding a frame or part of it, increase the send
    // window by the number of discarded bytes. (Although if we're
    // discarding part of a frame, it's probably because of a write
    // error and we'll be tearing down the stream soon.)
    size_t remaining_payload_bytes = std::min(consume_size, frame_payload_size);
    DCHECK_GT(remaining_payload_bytes, 0u);
    IncreaseSendWindowSize(static_cast<int32>(remaining_payload_bytes));
  }
  // For consumed bytes, the send window is increased when we receive
  // a WINDOW_UPDATE frame.
}

void SpdyStream::IncreaseSendWindowSize(int32 delta_window_size) {
  DCHECK_GE(session_->flow_control_state(), SpdySession::FLOW_CONTROL_STREAM);
  DCHECK_GE(delta_window_size, 1);

  // Ignore late WINDOW_UPDATEs.
  if (IsClosed())
    return;

  if (send_window_size_ > 0) {
    // Check for overflow.
    int32 max_delta_window_size = kint32max - send_window_size_;
    if (delta_window_size > max_delta_window_size) {
      std::string desc = base::StringPrintf(
          "Received WINDOW_UPDATE [delta: %d] for stream %d overflows "
          "send_window_size_ [current: %d]", delta_window_size, stream_id_,
          send_window_size_);
      session_->ResetStream(stream_id_, RST_STREAM_FLOW_CONTROL_ERROR, desc);
      return;
    }
  }

  send_window_size_ += delta_window_size;

  net_log_.AddEvent(
      NetLog::TYPE_SPDY_STREAM_UPDATE_SEND_WINDOW,
      base::Bind(&NetLogSpdyStreamWindowUpdateCallback,
                 stream_id_, delta_window_size, send_window_size_));

  PossiblyResumeIfSendStalled();
}

void SpdyStream::DecreaseSendWindowSize(int32 delta_window_size) {
  DCHECK_GE(session_->flow_control_state(), SpdySession::FLOW_CONTROL_STREAM);

  if (IsClosed())
    return;

  // We only call this method when sending a frame. Therefore,
  // |delta_window_size| should be within the valid frame size range.
  DCHECK_GE(delta_window_size, 1);
  DCHECK_LE(delta_window_size, kMaxSpdyFrameChunkSize);

  // |send_window_size_| should have been at least |delta_window_size| for
  // this call to happen.
  DCHECK_GE(send_window_size_, delta_window_size);

  send_window_size_ -= delta_window_size;

  net_log_.AddEvent(
      NetLog::TYPE_SPDY_STREAM_UPDATE_SEND_WINDOW,
      base::Bind(&NetLogSpdyStreamWindowUpdateCallback,
                 stream_id_, -delta_window_size, send_window_size_));
}

void SpdyStream::OnReadBufferConsumed(
    size_t consume_size,
    SpdyBuffer::ConsumeSource consume_source) {
  DCHECK_GE(session_->flow_control_state(), SpdySession::FLOW_CONTROL_STREAM);
  DCHECK_GE(consume_size, 1u);
  DCHECK_LE(consume_size, static_cast<size_t>(kint32max));
  IncreaseRecvWindowSize(static_cast<int32>(consume_size));
}

void SpdyStream::IncreaseRecvWindowSize(int32 delta_window_size) {
  DCHECK_GE(session_->flow_control_state(), SpdySession::FLOW_CONTROL_STREAM);

  // By the time a read is processed by the delegate, this stream may
  // already be inactive.
  if (!session_->IsStreamActive(stream_id_))
    return;

  DCHECK_GE(unacked_recv_window_bytes_, 0);
  DCHECK_GE(recv_window_size_, unacked_recv_window_bytes_);
  DCHECK_GE(delta_window_size, 1);
  // Check for overflow.
  DCHECK_LE(delta_window_size, kint32max - recv_window_size_);

  recv_window_size_ += delta_window_size;
  net_log_.AddEvent(
      NetLog::TYPE_SPDY_STREAM_UPDATE_RECV_WINDOW,
      base::Bind(&NetLogSpdyStreamWindowUpdateCallback,
                 stream_id_, delta_window_size, recv_window_size_));

  unacked_recv_window_bytes_ += delta_window_size;
  if (unacked_recv_window_bytes_ >
      session_->stream_initial_recv_window_size() / 2) {
    session_->SendStreamWindowUpdate(
        stream_id_, static_cast<uint32>(unacked_recv_window_bytes_));
    unacked_recv_window_bytes_ = 0;
  }
}

void SpdyStream::DecreaseRecvWindowSize(int32 delta_window_size) {
  DCHECK(session_->IsStreamActive(stream_id_));
  DCHECK_GE(session_->flow_control_state(), SpdySession::FLOW_CONTROL_STREAM);
  DCHECK_GE(delta_window_size, 1);

  // Since we never decrease the initial receive window size,
  // |delta_window_size| should never cause |recv_window_size_| to go
  // negative. If we do, the receive window isn't being respected.
  if (delta_window_size > recv_window_size_) {
    session_->ResetStream(
        stream_id_, RST_STREAM_PROTOCOL_ERROR,
        "delta_window_size is " + base::IntToString(delta_window_size) +
            " in DecreaseRecvWindowSize, which is larger than the receive " +
            "window size of " + base::IntToString(recv_window_size_));
    return;
  }

  recv_window_size_ -= delta_window_size;
  net_log_.AddEvent(
      NetLog::TYPE_SPDY_STREAM_UPDATE_RECV_WINDOW,
      base::Bind(&NetLogSpdyStreamWindowUpdateCallback,
                 stream_id_, -delta_window_size, recv_window_size_));
}

int SpdyStream::GetPeerAddress(IPEndPoint* address) const {
  return session_->GetPeerAddress(address);
}

int SpdyStream::GetLocalAddress(IPEndPoint* address) const {
  return session_->GetLocalAddress(address);
}

bool SpdyStream::WasEverUsed() const {
  return session_->WasEverUsed();
}

base::Time SpdyStream::GetRequestTime() const {
  return request_time_;
}

void SpdyStream::SetRequestTime(base::Time t) {
  request_time_ = t;
}

int SpdyStream::OnInitialResponseHeadersReceived(
    const SpdyHeaderBlock& initial_response_headers,
    base::Time response_time,
    base::TimeTicks recv_first_byte_time) {
  // SpdySession guarantees that this is called at most once.
  CHECK(response_headers_.empty());

  // Check to make sure that we don't receive the response headers
  // before we're ready for it.
  switch (type_) {
    case SPDY_BIDIRECTIONAL_STREAM:
      // For a bidirectional stream, we're ready for the response
      // headers once we've finished sending the request headers.
      if (io_state_ < STATE_IDLE) {
        session_->ResetStream(stream_id_, RST_STREAM_PROTOCOL_ERROR,
                              "Response received before request sent");
        return ERR_SPDY_PROTOCOL_ERROR;
      }
      break;

    case SPDY_REQUEST_RESPONSE_STREAM:
      // For a request/response stream, we're ready for the response
      // headers once we've finished sending the request headers and
      // the request body (if we have one).
      if ((io_state_ < STATE_IDLE) || (send_status_ == MORE_DATA_TO_SEND) ||
          pending_send_data_.get()) {
        session_->ResetStream(stream_id_, RST_STREAM_PROTOCOL_ERROR,
                              "Response received before request sent");
        return ERR_SPDY_PROTOCOL_ERROR;
      }
      break;

    case SPDY_PUSH_STREAM:
      // For a push stream, we're ready immediately.
      DCHECK_EQ(send_status_, NO_MORE_DATA_TO_SEND);
      DCHECK_EQ(io_state_, STATE_IDLE);
      break;
  }

  metrics_.StartStream();

  DCHECK_EQ(io_state_, STATE_IDLE);

  response_time_ = response_time;
  recv_first_byte_time_ = recv_first_byte_time;
  return MergeWithResponseHeaders(initial_response_headers);
}

int SpdyStream::OnAdditionalResponseHeadersReceived(
    const SpdyHeaderBlock& additional_response_headers) {
  if (type_ == SPDY_REQUEST_RESPONSE_STREAM) {
    session_->ResetStream(
        stream_id_, RST_STREAM_PROTOCOL_ERROR,
        "Additional headers received for request/response stream");
    return ERR_SPDY_PROTOCOL_ERROR;
  } else if (type_ == SPDY_PUSH_STREAM &&
             response_headers_status_ == RESPONSE_HEADERS_ARE_COMPLETE) {
    session_->ResetStream(
        stream_id_, RST_STREAM_PROTOCOL_ERROR,
        "Additional headers received for push stream");
    return ERR_SPDY_PROTOCOL_ERROR;
  }
  return MergeWithResponseHeaders(additional_response_headers);
}

void SpdyStream::OnDataReceived(scoped_ptr<SpdyBuffer> buffer) {
  DCHECK(session_->IsStreamActive(stream_id_));

  // If we're still buffering data for a push stream, we will do the
  // check for data received with incomplete headers in
  // PushedStreamReplayData().
  if (!delegate_ || continue_buffering_data_) {
    DCHECK_EQ(type_, SPDY_PUSH_STREAM);
    // It should be valid for this to happen in the server push case.
    // We'll return received data when delegate gets attached to the stream.
    if (buffer) {
      pending_buffers_.push_back(buffer.release());
    } else {
      pending_buffers_.push_back(NULL);
      metrics_.StopStream();
      // Note: we leave the stream open in the session until the stream
      //       is claimed.
    }
    return;
  }

  // If we have response headers but the delegate has indicated that
  // it's still incomplete, then that's a protocol error.
  if (response_headers_status_ == RESPONSE_HEADERS_ARE_INCOMPLETE) {
    LogStreamError(ERR_SPDY_PROTOCOL_ERROR,
                   "Data received with incomplete headers.");
    session_->CloseActiveStream(stream_id_, ERR_SPDY_PROTOCOL_ERROR);
    return;
  }

  CHECK(!IsClosed());

  if (!buffer) {
    metrics_.StopStream();
    // Deletes |this|.
    session_->CloseActiveStream(stream_id_, OK);
    return;
  }

  size_t length = buffer->GetRemainingSize();
  DCHECK_LE(length, session_->GetDataFrameMaximumPayload());
  if (session_->flow_control_state() >= SpdySession::FLOW_CONTROL_STREAM) {
    DecreaseRecvWindowSize(static_cast<int32>(length));
    buffer->AddConsumeCallback(
        base::Bind(&SpdyStream::OnReadBufferConsumed, GetWeakPtr()));
  }

  // Track our bandwidth.
  metrics_.RecordBytes(length);
  recv_bytes_ += length;
  recv_last_byte_time_ = base::TimeTicks::Now();

  // May close |this|.
  delegate_->OnDataReceived(buffer.Pass());
}

void SpdyStream::OnFrameWriteComplete(SpdyFrameType frame_type,
                                      size_t frame_size) {
  if (frame_size < session_->GetFrameMinimumSize() ||
      frame_size > session_->GetFrameMaximumSize()) {
    NOTREACHED();
    return;
  }
  if (IsClosed())
    return;
  just_completed_frame_type_ = frame_type;
  just_completed_frame_size_ = frame_size;
  DoLoop(OK);
}

SpdyMajorVersion SpdyStream::GetProtocolVersion() const {
  return session_->GetProtocolVersion();
}

void SpdyStream::LogStreamError(int status, const std::string& description) {
  net_log_.AddEvent(NetLog::TYPE_SPDY_STREAM_ERROR,
                    base::Bind(&NetLogSpdyStreamErrorCallback,
                               stream_id_, status, &description));
}

void SpdyStream::OnClose(int status) {
  CHECK(!in_do_loop_);
  io_state_ = STATE_CLOSED;
  response_status_ = status;
  Delegate* delegate = delegate_;
  delegate_ = NULL;
  if (delegate)
    delegate->OnClose(status);
  // Unset |stream_id_| last so that the delegate can look it up.
  stream_id_ = 0;
}

void SpdyStream::Cancel() {
  CHECK(!in_do_loop_);
  // We may be called again from a delegate's OnClose().
  if (io_state_ == STATE_CLOSED)
    return;

  if (stream_id_ != 0) {
    session_->ResetStream(stream_id_, RST_STREAM_CANCEL, std::string());
  } else {
    session_->CloseCreatedStream(GetWeakPtr(), RST_STREAM_CANCEL);
  }
  // |this| is invalid at this point.
}

void SpdyStream::Close() {
  CHECK(!in_do_loop_);
  // We may be called again from a delegate's OnClose().
  if (io_state_ == STATE_CLOSED)
    return;

  if (stream_id_ != 0) {
    session_->CloseActiveStream(stream_id_, OK);
  } else {
    session_->CloseCreatedStream(GetWeakPtr(), OK);
  }
  // |this| is invalid at this point.
}

base::WeakPtr<SpdyStream> SpdyStream::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

int SpdyStream::SendRequestHeaders(scoped_ptr<SpdyHeaderBlock> request_headers,
                                   SpdySendStatus send_status) {
  CHECK_NE(type_, SPDY_PUSH_STREAM);
  CHECK_EQ(send_status_, MORE_DATA_TO_SEND);
  CHECK(!request_headers_);
  CHECK(!pending_send_data_.get());
  CHECK_EQ(io_state_, STATE_NONE);
  request_headers_ = request_headers.Pass();
  send_status_ = send_status;
  io_state_ = STATE_SEND_REQUEST_HEADERS;
  return DoLoop(OK);
}

void SpdyStream::SendData(IOBuffer* data,
                          int length,
                          SpdySendStatus send_status) {
  CHECK_NE(type_, SPDY_PUSH_STREAM);
  CHECK_EQ(send_status_, MORE_DATA_TO_SEND);
  CHECK_GE(io_state_, STATE_SEND_REQUEST_HEADERS_COMPLETE);
  CHECK(!pending_send_data_.get());
  pending_send_data_ = new DrainableIOBuffer(data, length);
  send_status_ = send_status;
  QueueNextDataFrame();
}

bool SpdyStream::GetSSLInfo(SSLInfo* ssl_info,
                            bool* was_npn_negotiated,
                            NextProto* protocol_negotiated) {
  return session_->GetSSLInfo(
      ssl_info, was_npn_negotiated, protocol_negotiated);
}

bool SpdyStream::GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) {
  return session_->GetSSLCertRequestInfo(cert_request_info);
}

void SpdyStream::PossiblyResumeIfSendStalled() {
  DCHECK(!IsClosed());

  if (send_stalled_by_flow_control_ && !session_->IsSendStalled() &&
      send_window_size_ > 0) {
    net_log_.AddEvent(
        NetLog::TYPE_SPDY_STREAM_FLOW_CONTROL_UNSTALLED,
        NetLog::IntegerCallback("stream_id", stream_id_));
    send_stalled_by_flow_control_ = false;
    QueueNextDataFrame();
  }
}

bool SpdyStream::IsClosed() const {
  return io_state_ == STATE_CLOSED;
}

bool SpdyStream::IsIdle() const {
  return io_state_ == STATE_IDLE;
}

NextProto SpdyStream::GetProtocol() const {
  return session_->protocol();
}

bool SpdyStream::GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const {
  if (stream_id_ == 0)
    return false;

  return session_->GetLoadTimingInfo(stream_id_, load_timing_info);
}

GURL SpdyStream::GetUrlFromHeaders() const {
  if (type_ != SPDY_PUSH_STREAM && !request_headers_)
    return GURL();

  const SpdyHeaderBlock& headers =
      (type_ == SPDY_PUSH_STREAM) ? response_headers_ : *request_headers_;
  return GetUrlFromHeaderBlock(headers, GetProtocolVersion(),
                               type_ == SPDY_PUSH_STREAM);
}

bool SpdyStream::HasUrlFromHeaders() const {
  return !GetUrlFromHeaders().is_empty();
}

int SpdyStream::DoLoop(int result) {
  CHECK(!in_do_loop_);
  in_do_loop_ = true;

  do {
    State state = io_state_;
    io_state_ = STATE_NONE;
    switch (state) {
      case STATE_SEND_REQUEST_HEADERS:
        CHECK_EQ(result, OK);
        result = DoSendRequestHeaders();
        break;
      case STATE_SEND_REQUEST_HEADERS_COMPLETE:
        CHECK_EQ(result, OK);
        result = DoSendRequestHeadersComplete();
        break;

      // For request/response streams, no data is sent from the client
      // while in the OPEN state, so OnFrameWriteComplete is never
      // called here.  The HTTP body is handled in the OnDataReceived
      // callback, which does not call into DoLoop.
      //
      // For bidirectional streams, we'll send and receive data once
      // the connection is established.  Received data is handled in
      // OnDataReceived.  Sent data is handled in
      // OnFrameWriteComplete, which calls DoOpen().
      case STATE_IDLE:
        CHECK_EQ(result, OK);
        result = DoOpen();
        break;

      case STATE_CLOSED:
        DCHECK_NE(result, ERR_IO_PENDING);
        break;
      default:
        NOTREACHED() << io_state_;
        break;
    }
  } while (result != ERR_IO_PENDING && io_state_ != STATE_NONE &&
           io_state_ != STATE_IDLE);

  CHECK(in_do_loop_);
  in_do_loop_ = false;

  return result;
}

int SpdyStream::DoSendRequestHeaders() {
  DCHECK_NE(type_, SPDY_PUSH_STREAM);
  io_state_ = STATE_SEND_REQUEST_HEADERS_COMPLETE;

  session_->EnqueueStreamWrite(
      GetWeakPtr(), SYN_STREAM,
      scoped_ptr<SpdyBufferProducer>(
          new SynStreamBufferProducer(GetWeakPtr())));
  return ERR_IO_PENDING;
}

namespace {

// Assuming we're in STATE_IDLE, maps the given type (which must not
// be SPDY_PUSH_STREAM) and send status to a result to return from
// DoSendRequestHeadersComplete() or DoOpen().
int GetOpenStateResult(SpdyStreamType type, SpdySendStatus send_status) {
  switch (type) {
    case SPDY_BIDIRECTIONAL_STREAM:
      // For bidirectional streams, there's nothing else to do.
      DCHECK_EQ(send_status, MORE_DATA_TO_SEND);
      return OK;

    case SPDY_REQUEST_RESPONSE_STREAM:
      // For request/response streams, wait for the delegate to send
      // data if there's request data to send; we'll get called back
      // when the send finishes.
      if (send_status == MORE_DATA_TO_SEND)
        return ERR_IO_PENDING;

      return OK;

    case SPDY_PUSH_STREAM:
      // This should never be called for push streams.
      break;
  }

  CHECK(false);
  return ERR_UNEXPECTED;
}

}  // namespace

int SpdyStream::DoSendRequestHeadersComplete() {
  DCHECK_NE(type_, SPDY_PUSH_STREAM);
  DCHECK_EQ(just_completed_frame_type_, SYN_STREAM);
  DCHECK_NE(stream_id_, 0u);

  io_state_ = STATE_IDLE;

  CHECK(delegate_);
  // Must not close |this|; if it does, it will trigger the |in_do_loop_|
  // check in the destructor.
  delegate_->OnRequestHeadersSent();

  return GetOpenStateResult(type_, send_status_);
}

int SpdyStream::DoOpen() {
  DCHECK_NE(type_, SPDY_PUSH_STREAM);

  if (just_completed_frame_type_ != DATA) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }

  if (just_completed_frame_size_ < session_->GetDataFrameMinimumSize()) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }

  size_t frame_payload_size =
      just_completed_frame_size_ - session_->GetDataFrameMinimumSize();
  if (frame_payload_size > session_->GetDataFrameMaximumPayload()) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }

  // Set |io_state_| first as |delegate_| may check it.
  io_state_ = STATE_IDLE;

  send_bytes_ += frame_payload_size;

  pending_send_data_->DidConsume(frame_payload_size);
  if (pending_send_data_->BytesRemaining() > 0) {
    QueueNextDataFrame();
    return ERR_IO_PENDING;
  }

  pending_send_data_ = NULL;

  CHECK(delegate_);
  // Must not close |this|; if it does, it will trigger the
  // |in_do_loop_| check in the destructor.
  delegate_->OnDataSent();

  return GetOpenStateResult(type_, send_status_);
}

void SpdyStream::UpdateHistograms() {
  // We need at least the receive timers to be filled in, as otherwise
  // metrics can be bogus.
  if (recv_first_byte_time_.is_null() || recv_last_byte_time_.is_null())
    return;

  base::TimeTicks effective_send_time;
  if (type_ == SPDY_PUSH_STREAM) {
    // Push streams shouldn't have |send_time_| filled in.
    DCHECK(send_time_.is_null());
    effective_send_time = recv_first_byte_time_;
  } else {
    // For non-push streams, we also need |send_time_| to be filled
    // in.
    if (send_time_.is_null())
      return;
    effective_send_time = send_time_;
  }

  UMA_HISTOGRAM_TIMES("Net.SpdyStreamTimeToFirstByte",
                      recv_first_byte_time_ - effective_send_time);
  UMA_HISTOGRAM_TIMES("Net.SpdyStreamDownloadTime",
                      recv_last_byte_time_ - recv_first_byte_time_);
  UMA_HISTOGRAM_TIMES("Net.SpdyStreamTime",
                      recv_last_byte_time_ - effective_send_time);

  UMA_HISTOGRAM_COUNTS("Net.SpdySendBytes", send_bytes_);
  UMA_HISTOGRAM_COUNTS("Net.SpdyRecvBytes", recv_bytes_);
}

void SpdyStream::QueueNextDataFrame() {
  // Until the request has been completely sent, we cannot be sure
  // that our stream_id is correct.
  DCHECK_GT(io_state_, STATE_SEND_REQUEST_HEADERS_COMPLETE);
  CHECK_GT(stream_id_, 0u);
  CHECK(pending_send_data_.get());
  CHECK_GT(pending_send_data_->BytesRemaining(), 0);

  SpdyDataFlags flags =
      (send_status_ == NO_MORE_DATA_TO_SEND) ?
      DATA_FLAG_FIN : DATA_FLAG_NONE;
  scoped_ptr<SpdyBuffer> data_buffer(
      session_->CreateDataBuffer(stream_id_,
                                 pending_send_data_.get(),
                                 pending_send_data_->BytesRemaining(),
                                 flags));
  // We'll get called again by PossiblyResumeIfSendStalled().
  if (!data_buffer)
    return;

  if (session_->flow_control_state() >= SpdySession::FLOW_CONTROL_STREAM) {
    DCHECK_GE(data_buffer->GetRemainingSize(),
              session_->GetDataFrameMinimumSize());
    size_t payload_size =
        data_buffer->GetRemainingSize() - session_->GetDataFrameMinimumSize();
    DCHECK_LE(payload_size, session_->GetDataFrameMaximumPayload());
    DecreaseSendWindowSize(static_cast<int32>(payload_size));
    // This currently isn't strictly needed, since write frames are
    // discarded only if the stream is about to be closed. But have it
    // here anyway just in case this changes.
    data_buffer->AddConsumeCallback(
        base::Bind(&SpdyStream::OnWriteBufferConsumed,
                   GetWeakPtr(), payload_size));
  }

  session_->EnqueueStreamWrite(
      GetWeakPtr(), DATA,
      scoped_ptr<SpdyBufferProducer>(
          new SimpleBufferProducer(data_buffer.Pass())));
}

int SpdyStream::MergeWithResponseHeaders(
    const SpdyHeaderBlock& new_response_headers) {
  if (new_response_headers.find("transfer-encoding") !=
      new_response_headers.end()) {
    session_->ResetStream(stream_id_, RST_STREAM_PROTOCOL_ERROR,
                         "Received transfer-encoding header");
    return ERR_SPDY_PROTOCOL_ERROR;
  }

  for (SpdyHeaderBlock::const_iterator it = new_response_headers.begin();
      it != new_response_headers.end(); ++it) {
    // Disallow uppercase headers.
    if (ContainsUppercaseAscii(it->first)) {
      session_->ResetStream(stream_id_, RST_STREAM_PROTOCOL_ERROR,
                            "Upper case characters in header: " + it->first);
      return ERR_SPDY_PROTOCOL_ERROR;
    }

    SpdyHeaderBlock::iterator it2 = response_headers_.lower_bound(it->first);
    // Disallow duplicate headers.  This is just to be conservative.
    if (it2 != response_headers_.end() && it2->first == it->first) {
      session_->ResetStream(stream_id_, RST_STREAM_PROTOCOL_ERROR,
                            "Duplicate header: " + it->first);
      return ERR_SPDY_PROTOCOL_ERROR;
    }

    response_headers_.insert(it2, *it);
  }

  // If delegate_ is not yet attached, we'll call
  // OnResponseHeadersUpdated() after the delegate gets attached to
  // the stream.
  if (delegate_) {
    // The call to OnResponseHeadersUpdated() below may delete |this|,
    // so use |weak_this| to detect that.
    base::WeakPtr<SpdyStream> weak_this = GetWeakPtr();

    SpdyResponseHeadersStatus status =
        delegate_->OnResponseHeadersUpdated(response_headers_);
    if (status == RESPONSE_HEADERS_ARE_INCOMPLETE) {
      // Since RESPONSE_HEADERS_ARE_INCOMPLETE was returned, we must not
      // have been closed.
      CHECK(weak_this);
      // Incomplete headers are OK only for push streams.
      if (type_ != SPDY_PUSH_STREAM) {
        session_->ResetStream(stream_id_, RST_STREAM_PROTOCOL_ERROR,
                              "Incomplete headers");
        return ERR_INCOMPLETE_SPDY_HEADERS;
      }
    } else if (weak_this) {
      response_headers_status_ = RESPONSE_HEADERS_ARE_COMPLETE;
    }
  }

  return OK;
}

}  // namespace net
