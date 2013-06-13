// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_stream.h"

#include <limits>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/string_number_conversions.h"
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

bool ContainsUpperAscii(const std::string& str) {
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
                       SpdySession* session,
                       const std::string& path,
                       RequestPriority priority,
                       int32 initial_send_window_size,
                       int32 initial_recv_window_size,
                       const BoundNetLog& net_log)
    : type_(type),
      weak_ptr_factory_(this),
      in_do_loop_(false),
      continue_buffering_data_(true),
      stream_id_(0),
      path_(path),
      priority_(priority),
      slot_(0),
      send_stalled_by_flow_control_(false),
      send_window_size_(initial_send_window_size),
      recv_window_size_(initial_recv_window_size),
      unacked_recv_window_bytes_(0),
      response_received_(false),
      session_(session),
      delegate_(NULL),
      send_status_(
          (type_ == SPDY_PUSH_STREAM) ?
          NO_MORE_DATA_TO_SEND : MORE_DATA_TO_SEND),
      request_time_(base::Time::Now()),
      response_(new SpdyHeaderBlock),
      io_state_((type_ == SPDY_PUSH_STREAM) ? STATE_OPEN : STATE_NONE),
      response_status_(OK),
      net_log_(net_log),
      send_bytes_(0),
      recv_bytes_(0),
      domain_bound_cert_type_(CLIENT_CERT_INVALID_TYPE),
      just_completed_frame_type_(DATA),
      just_completed_frame_size_(0) {
  CHECK(type_ == SPDY_BIDIRECTIONAL_STREAM ||
        type_ == SPDY_REQUEST_RESPONSE_STREAM ||
        type_ == SPDY_PUSH_STREAM);
}

SpdyStream::~SpdyStream() {
  CHECK(!in_do_loop_);
  UpdateHistograms();
}

void SpdyStream::SetDelegate(Delegate* delegate) {
  CHECK(delegate);
  delegate_ = delegate;

  if (type_ == SPDY_PUSH_STREAM) {
    CHECK(response_received());
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&SpdyStream::PushedStreamReplayData, GetWeakPtr()));
  } else {
    continue_buffering_data_ = false;
  }
}

void SpdyStream::PushedStreamReplayData() {
  DCHECK_NE(stream_id_, 0u);

  if (!delegate_)
    return;

  continue_buffering_data_ = false;

  // TODO(akalin): This call may delete this object. Figure out what
  // to do in that case.
  int rv = delegate_->OnResponseHeadersReceived(*response_, response_time_, OK);
  if (rv == ERR_INCOMPLETE_SPDY_HEADERS) {
    // We don't have complete headers.  Assume we're waiting for another
    // HEADERS frame.  Since we don't have headers, we had better not have
    // any pending data frames.
    if (pending_buffers_.size() != 0U) {
      LogStreamError(ERR_SPDY_PROTOCOL_ERROR,
                     "HEADERS incomplete headers, but pending data frames.");
      session_->CloseActiveStream(stream_id_, ERR_SPDY_PROTOCOL_ERROR);
    }
    return;
  }

  std::vector<SpdyBuffer*> buffers;
  pending_buffers_.release(&buffers);
  for (size_t i = 0; i < buffers.size(); ++i) {
    // It is always possible that a callback to the delegate results in
    // the delegate no longer being available.
    if (!delegate_)
      break;
    if (buffers[i]) {
      delegate_->OnDataReceived(scoped_ptr<SpdyBuffer>(buffers[i]));
    } else {
      delegate_->OnDataReceived(scoped_ptr<SpdyBuffer>());
      session_->CloseActiveStream(stream_id_, OK);
      // Note: |this| may be deleted after calling CloseActiveStream.
      DCHECK_EQ(buffers.size() - 1, i);
    }
  }
}

scoped_ptr<SpdyFrame> SpdyStream::ProduceSynStreamFrame() {
  CHECK_EQ(io_state_, STATE_SEND_REQUEST_HEADERS_COMPLETE);
  CHECK(request_);
  CHECK_GT(stream_id_, 0u);

  SpdyControlFlags flags =
      (send_status_ == NO_MORE_DATA_TO_SEND) ?
      CONTROL_FLAG_FIN : CONTROL_FLAG_NONE;
  scoped_ptr<SpdyFrame> frame(session_->CreateSynStream(
      stream_id_, priority_, slot_, flags, *request_));
  send_time_ = base::TimeTicks::Now();
  return frame.Pass();
}

void SpdyStream::DetachDelegate() {
  CHECK(!in_do_loop_);
  DCHECK(!closed());
  delegate_ = NULL;
  Cancel();
}

void SpdyStream::AdjustSendWindowSize(int32 delta_window_size) {
  DCHECK_GE(session_->flow_control_state(), SpdySession::FLOW_CONTROL_STREAM);

  if (closed())
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
  if (closed())
    return;

  if (send_window_size_ > 0) {
    // Check for overflow.
    int32 max_delta_window_size = kint32max - send_window_size_;
    if (delta_window_size > max_delta_window_size) {
      std::string desc = base::StringPrintf(
          "Received WINDOW_UPDATE [delta: %d] for stream %d overflows "
          "send_window_size_ [current: %d]", delta_window_size, stream_id_,
          send_window_size_);
      session_->ResetStream(stream_id_, priority_,
                            RST_STREAM_FLOW_CONTROL_ERROR, desc);
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

  if (closed())
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
        stream_id_, priority_, RST_STREAM_PROTOCOL_ERROR,
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

int SpdyStream::OnResponseHeadersReceived(const SpdyHeaderBlock& response) {
  int rv = OK;

  metrics_.StartStream();

  // TODO(akalin): This should be handled as a protocol error.
  DCHECK(response_->empty());
  *response_ = response;  // TODO(ukai): avoid copy.

  recv_first_byte_time_ = base::TimeTicks::Now();
  response_time_ = base::Time::Now();

  // Check to make sure that we don't receive the response headers
  // before we're ready for it.
  switch (type_) {
    case SPDY_BIDIRECTIONAL_STREAM:
      // For a bidirectional stream, we're ready for the response
      // headers once we've finished sending the request headers.
      if (io_state_ < STATE_OPEN)
        return ERR_SPDY_PROTOCOL_ERROR;
      break;

    case SPDY_REQUEST_RESPONSE_STREAM:
      // For a request/response stream, we're ready for the response
      // headers once we've finished sending the request headers and
      // the request body (if we have one).
      if ((io_state_ < STATE_OPEN) || (send_status_ == MORE_DATA_TO_SEND) ||
          pending_send_data_.get())
        return ERR_SPDY_PROTOCOL_ERROR;
      break;

    case SPDY_PUSH_STREAM:
      // For a push stream, we're ready immediately.
      DCHECK_EQ(send_status_, NO_MORE_DATA_TO_SEND);
      DCHECK_EQ(io_state_, STATE_OPEN);
      break;
  }

  DCHECK_EQ(io_state_, STATE_OPEN);

  // TODO(akalin): Merge the code below with the code in OnHeaders().

  // Append all the headers into the response header block.
  for (SpdyHeaderBlock::const_iterator it = response.begin();
       it != response.end(); ++it) {
    // Disallow uppercase headers.
    if (ContainsUpperAscii(it->first)) {
      session_->ResetStream(stream_id_, priority_, RST_STREAM_PROTOCOL_ERROR,
                            "Upper case characters in header: " + it->first);
      return ERR_SPDY_PROTOCOL_ERROR;
    }
  }

  if ((*response_).find("transfer-encoding") != (*response_).end()) {
    session_->ResetStream(stream_id_, priority_, RST_STREAM_PROTOCOL_ERROR,
                         "Received transfer-encoding header");
    return ERR_SPDY_PROTOCOL_ERROR;
  }

  if (delegate_) {
    // May delete this object.
    rv = delegate_->OnResponseHeadersReceived(*response_, response_time_, rv);
  }
  // If delegate_ is not yet attached, we'll call
  // OnResponseHeadersReceived after the delegate gets attached to the
  // stream.

  return rv;
}

int SpdyStream::OnHeaders(const SpdyHeaderBlock& headers) {
  DCHECK(!response_->empty());

  // Append all the headers into the response header block.
  for (SpdyHeaderBlock::const_iterator it = headers.begin();
      it != headers.end(); ++it) {
    // Disallow duplicate headers.  This is just to be conservative.
    if ((*response_).find(it->first) != (*response_).end()) {
      LogStreamError(ERR_SPDY_PROTOCOL_ERROR, "HEADERS duplicate header");
      response_status_ = ERR_SPDY_PROTOCOL_ERROR;
      return ERR_SPDY_PROTOCOL_ERROR;
    }

    // Disallow uppercase headers.
    if (ContainsUpperAscii(it->first)) {
      session_->ResetStream(stream_id_, priority_, RST_STREAM_PROTOCOL_ERROR,
                            "Upper case characters in header: " + it->first);
      return ERR_SPDY_PROTOCOL_ERROR;
    }

    (*response_)[it->first] = it->second;
  }

  if ((*response_).find("transfer-encoding") != (*response_).end()) {
    session_->ResetStream(stream_id_, priority_, RST_STREAM_PROTOCOL_ERROR,
                         "Received transfer-encoding header");
    return ERR_SPDY_PROTOCOL_ERROR;
  }

  int rv = OK;
  if (delegate_) {
    // May delete this object.
    rv = delegate_->OnResponseHeadersReceived(*response_, response_time_, rv);
    // ERR_INCOMPLETE_SPDY_HEADERS means that we are waiting for more
    // headers before the response header block is complete.
    if (rv == ERR_INCOMPLETE_SPDY_HEADERS)
      rv = OK;
  }
  return rv;
}

void SpdyStream::OnDataReceived(scoped_ptr<SpdyBuffer> buffer) {
  DCHECK(session_->IsStreamActive(stream_id_));
  // If we don't have a response, then the SYN_REPLY did not come through.
  // We cannot pass data up to the caller unless the reply headers have been
  // received.
  if (!response_received()) {
    LogStreamError(ERR_SYN_REPLY_NOT_RECEIVED, "Didn't receive a response.");
    session_->CloseActiveStream(stream_id_, ERR_SYN_REPLY_NOT_RECEIVED);
    return;
  }

  if (!delegate_ || continue_buffering_data_) {
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

  CHECK(!closed());

  if (!buffer) {
    metrics_.StopStream();
    session_->CloseActiveStream(stream_id_, OK);
    // Note: |this| may be deleted after calling CloseActiveStream.
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

  if (delegate_->OnDataReceived(buffer.Pass()) != OK) {
    // |delegate_| rejected the data.
    LogStreamError(ERR_SPDY_PROTOCOL_ERROR, "Delegate rejected the data");
    session_->CloseActiveStream(stream_id_, ERR_SPDY_PROTOCOL_ERROR);
    return;
  }
}

void SpdyStream::OnFrameWriteComplete(SpdyFrameType frame_type,
                                      size_t frame_size) {
  if (frame_size < session_->GetFrameMinimumSize() ||
      frame_size > session_->GetFrameMaximumSize()) {
    NOTREACHED();
    return;
  }
  if (closed())
    return;
  just_completed_frame_type_ = frame_type;
  just_completed_frame_size_ = frame_size;
  DoLoop(OK);
}

int SpdyStream::GetProtocolVersion() const {
  return session_->GetProtocolVersion();
}

void SpdyStream::LogStreamError(int status, const std::string& description) {
  net_log_.AddEvent(NetLog::TYPE_SPDY_STREAM_ERROR,
                    base::Bind(&NetLogSpdyStreamErrorCallback,
                               stream_id_, status, &description));
}

void SpdyStream::OnClose(int status) {
  CHECK(!in_do_loop_);
  io_state_ = STATE_DONE;
  response_status_ = status;
  Delegate* delegate = delegate_;
  delegate_ = NULL;
  if (delegate)
    delegate->OnClose(status);
}

void SpdyStream::Cancel() {
  CHECK(!in_do_loop_);
  if (stream_id_ != 0) {
    session_->ResetStream(stream_id_, priority_,
                          RST_STREAM_CANCEL, std::string());
  } else {
    session_->CloseCreatedStream(GetWeakPtr(), RST_STREAM_CANCEL);
  }
}

void SpdyStream::Close() {
  CHECK(!in_do_loop_);
  if (stream_id_ != 0) {
    session_->CloseActiveStream(stream_id_, OK);
  } else {
    session_->CloseCreatedStream(GetWeakPtr(), OK);
  }
}

int SpdyStream::SendRequestHeaders(scoped_ptr<SpdyHeaderBlock> headers,
                                   SpdySendStatus send_status) {
  CHECK_NE(type_, SPDY_PUSH_STREAM);
  CHECK_EQ(send_status_, MORE_DATA_TO_SEND);
  CHECK(!request_);
  CHECK(!pending_send_data_.get());
  CHECK_EQ(io_state_, STATE_NONE);
  request_ = headers.Pass();
  send_status_ = send_status;
  io_state_ = STATE_GET_DOMAIN_BOUND_CERT;
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
  DCHECK(!closed());

  if (send_stalled_by_flow_control_ && !session_->IsSendStalled() &&
      send_window_size_ > 0) {
    net_log_.AddEvent(
        NetLog::TYPE_SPDY_STREAM_FLOW_CONTROL_UNSTALLED,
        NetLog::IntegerCallback("stream_id", stream_id_));
    send_stalled_by_flow_control_ = false;
    QueueNextDataFrame();
  }
}

base::WeakPtr<SpdyStream> SpdyStream::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool SpdyStream::HasUrl() const {
  if (type_ == SPDY_PUSH_STREAM)
    return response_received();
  return request_ != NULL;
}

GURL SpdyStream::GetUrl() const {
  DCHECK(HasUrl());

  const SpdyHeaderBlock& headers =
      (type_ == SPDY_PUSH_STREAM) ? *response_ : *request_;
  return GetUrlFromHeaderBlock(headers, GetProtocolVersion(),
                               type_ == SPDY_PUSH_STREAM);
}

void SpdyStream::OnGetDomainBoundCertComplete(int result) {
  DCHECK_EQ(io_state_, STATE_GET_DOMAIN_BOUND_CERT_COMPLETE);
  DoLoop(result);
}

int SpdyStream::DoLoop(int result) {
  CHECK(!in_do_loop_);
  in_do_loop_ = true;

  do {
    State state = io_state_;
    io_state_ = STATE_NONE;
    switch (state) {
      case STATE_GET_DOMAIN_BOUND_CERT:
        CHECK_EQ(result, OK);
        result = DoGetDomainBoundCert();
        break;
      case STATE_GET_DOMAIN_BOUND_CERT_COMPLETE:
        result = DoGetDomainBoundCertComplete(result);
        break;
      case STATE_SEND_DOMAIN_BOUND_CERT:
        CHECK_EQ(result, OK);
        result = DoSendDomainBoundCert();
        break;
      case STATE_SEND_DOMAIN_BOUND_CERT_COMPLETE:
        result = DoSendDomainBoundCertComplete(result);
        break;
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
      case STATE_OPEN:
        CHECK_EQ(result, OK);
        result = DoOpen();
        break;

      case STATE_DONE:
        DCHECK(result != ERR_IO_PENDING);
        break;
      default:
        NOTREACHED() << io_state_;
        break;
    }
  } while (result != ERR_IO_PENDING && io_state_ != STATE_NONE &&
           io_state_ != STATE_OPEN);

  CHECK(in_do_loop_);
  in_do_loop_ = false;

  return result;
}

int SpdyStream::DoGetDomainBoundCert() {
  CHECK(request_);
  DCHECK_NE(type_, SPDY_PUSH_STREAM);
  GURL url = GetUrl();
  if (!session_->NeedsCredentials() || !url.SchemeIs("https")) {
    // Proceed directly to sending the request headers
    io_state_ = STATE_SEND_REQUEST_HEADERS;
    return OK;
  }

  slot_ = session_->credential_state()->FindCredentialSlot(GetUrl());
  if (slot_ != SpdyCredentialState::kNoEntry) {
    // Proceed directly to sending the request headers
    io_state_ = STATE_SEND_REQUEST_HEADERS;
    return OK;
  }

  io_state_ = STATE_GET_DOMAIN_BOUND_CERT_COMPLETE;
  ServerBoundCertService* sbc_service = session_->GetServerBoundCertService();
  DCHECK(sbc_service != NULL);
  std::vector<uint8> requested_cert_types;
  requested_cert_types.push_back(CLIENT_CERT_ECDSA_SIGN);
  int rv = sbc_service->GetDomainBoundCert(
      url.GetOrigin().host(), requested_cert_types,
      &domain_bound_cert_type_, &domain_bound_private_key_, &domain_bound_cert_,
      base::Bind(&SpdyStream::OnGetDomainBoundCertComplete, GetWeakPtr()),
      &domain_bound_cert_request_handle_);
  return rv;
}

int SpdyStream::DoGetDomainBoundCertComplete(int result) {
  DCHECK_NE(type_, SPDY_PUSH_STREAM);
  if (result != OK)
    return result;

  io_state_ = STATE_SEND_DOMAIN_BOUND_CERT;
  slot_ =  session_->credential_state()->SetHasCredential(GetUrl());
  return OK;
}

int SpdyStream::DoSendDomainBoundCert() {
  CHECK(request_);
  DCHECK_NE(type_, SPDY_PUSH_STREAM);
  io_state_ = STATE_SEND_DOMAIN_BOUND_CERT_COMPLETE;

  std::string origin = GetUrl().GetOrigin().spec();
  DCHECK(origin[origin.length() - 1] == '/');
  origin.erase(origin.length() - 1);  // Trim trailing slash.
  scoped_ptr<SpdyFrame> frame;
  int rv = session_->CreateCredentialFrame(
      origin, domain_bound_cert_type_, domain_bound_private_key_,
      domain_bound_cert_, priority_, &frame);
  if (rv != OK) {
    DCHECK_NE(rv, ERR_IO_PENDING);
    return rv;
  }

  DCHECK(frame);
  // TODO(akalin): Fix the following race condition:
  //
  // Since this is decoupled from sending the SYN_STREAM frame, it is
  // possible that other domain-bound cert frames will clobber ours
  // before our SYN_STREAM frame gets sent. This can be solved by
  // immediately enqueueing the SYN_STREAM frame here and adjusting
  // the state machine appropriately.
  session_->EnqueueStreamWrite(
      GetWeakPtr(), CREDENTIAL,
      scoped_ptr<SpdyBufferProducer>(
          new SimpleBufferProducer(
              scoped_ptr<SpdyBuffer>(new SpdyBuffer(frame.Pass())))));
  return ERR_IO_PENDING;
}

int SpdyStream::DoSendDomainBoundCertComplete(int result) {
  DCHECK_NE(type_, SPDY_PUSH_STREAM);
  if (result != OK)
    return result;

  DCHECK_EQ(just_completed_frame_type_, CREDENTIAL);
  io_state_ = STATE_SEND_REQUEST_HEADERS;
  return OK;
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

// Assuming we're in STATE_OPEN, maps the given type (which must not
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

  io_state_ = STATE_OPEN;

  // Do this before calling into the |delegate_| as that call may
  // delete us.
  int result = GetOpenStateResult(type_, send_status_);

  CHECK(delegate_);
  delegate_->OnRequestHeadersSent();

  return result;
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
  io_state_ = STATE_OPEN;

  send_bytes_ += frame_payload_size;

  pending_send_data_->DidConsume(frame_payload_size);
  if (pending_send_data_->BytesRemaining() > 0) {
    QueueNextDataFrame();
    return ERR_IO_PENDING;
  }

  pending_send_data_ = NULL;

  // Do this before calling into the |delegate_| as that call may
  // delete us.
  int result = GetOpenStateResult(type_, send_status_);

  CHECK(delegate_);
  delegate_->OnDataSent();

  return result;
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

}  // namespace net
