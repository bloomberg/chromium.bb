// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_stream.h"

#include <limits>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "net/spdy/spdy_buffer_producer.h"
#include "net/spdy/spdy_http_utils.h"
#include "net/spdy/spdy_session.h"

namespace net {

namespace {

Value* NetLogSpdyStreamErrorCallback(SpdyStreamId stream_id,
                                     int status,
                                     const std::string* description,
                                     NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("stream_id", static_cast<int>(stream_id));
  dict->SetInteger("status", status);
  dict->SetString("description", *description);
  return dict;
}

Value* NetLogSpdyStreamWindowUpdateCallback(SpdyStreamId stream_id,
                                            int32 delta,
                                            int32 window_size,
                                            NetLog::LogLevel /* log_level */) {
  DictionaryValue* dict = new DictionaryValue();
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
    DCHECK(stream_);
  }

  virtual ~SynStreamBufferProducer() {}

  virtual scoped_ptr<SpdyBuffer> ProduceBuffer() OVERRIDE {
    if (!stream_) {
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

// A wrapper around a stream that calls into ProduceHeaderFrame() with
// a given header block.
class SpdyStream::HeaderBufferProducer : public SpdyBufferProducer {
 public:
  HeaderBufferProducer(const base::WeakPtr<SpdyStream>& stream,
                      scoped_ptr<SpdyHeaderBlock> headers)
      : stream_(stream),
        headers_(headers.Pass()) {
    DCHECK(stream_);
    DCHECK(headers_);
  }

  virtual ~HeaderBufferProducer() {}

  virtual scoped_ptr<SpdyBuffer> ProduceBuffer() OVERRIDE {
    if (!stream_) {
      NOTREACHED();
      return scoped_ptr<SpdyBuffer>();
    }
    DCHECK_GT(stream_->stream_id(), 0u);
    return scoped_ptr<SpdyBuffer>(
        new SpdyBuffer(stream_->ProduceHeaderFrame(headers_.Pass())));
  }

 private:
  const base::WeakPtr<SpdyStream> stream_;
  scoped_ptr<SpdyHeaderBlock> headers_;
};

SpdyStream::SpdyStream(SpdySession* session,
                       const std::string& path,
                       RequestPriority priority,
                       int32 initial_send_window_size,
                       int32 initial_recv_window_size,
                       bool pushed,
                       const BoundNetLog& net_log)
    : weak_ptr_factory_(this),
      continue_buffering_data_(true),
      stream_id_(0),
      path_(path),
      priority_(priority),
      slot_(0),
      send_stalled_by_flow_control_(false),
      send_window_size_(initial_send_window_size),
      recv_window_size_(initial_recv_window_size),
      unacked_recv_window_bytes_(0),
      pushed_(pushed),
      response_received_(false),
      session_(session),
      delegate_(NULL),
      request_time_(base::Time::Now()),
      response_(new SpdyHeaderBlock),
      io_state_(STATE_NONE),
      response_status_(OK),
      cancelled_(false),
      has_upload_data_(false),
      net_log_(net_log),
      send_bytes_(0),
      recv_bytes_(0),
      domain_bound_cert_type_(CLIENT_CERT_INVALID_TYPE),
      just_completed_frame_type_(DATA),
      just_completed_frame_size_(0) {
}

SpdyStream::~SpdyStream() {
  UpdateHistograms();
}

void SpdyStream::SetDelegate(Delegate* delegate) {
  CHECK(delegate);
  delegate_ = delegate;

  if (pushed_) {
    CHECK(response_received());
    MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&SpdyStream::PushedStreamReplayData, this));
  } else {
    continue_buffering_data_ = false;
  }
}

void SpdyStream::PushedStreamReplayData() {
  if (cancelled_ || !delegate_)
    return;

  continue_buffering_data_ = false;

  int rv = delegate_->OnResponseReceived(*response_, response_time_, OK);
  if (rv == ERR_INCOMPLETE_SPDY_HEADERS) {
    // We don't have complete headers.  Assume we're waiting for another
    // HEADERS frame.  Since we don't have headers, we had better not have
    // any pending data frames.
    if (pending_buffers_.size() != 0U) {
      LogStreamError(ERR_SPDY_PROTOCOL_ERROR,
                     "HEADERS incomplete headers, but pending data frames.");
      session_->CloseStream(stream_id_, ERR_SPDY_PROTOCOL_ERROR);
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
      session_->CloseStream(stream_id_, OK);
      // Note: |this| may be deleted after calling CloseStream.
      DCHECK_EQ(buffers.size() - 1, i);
    }
  }
}

scoped_ptr<SpdyFrame> SpdyStream::ProduceSynStreamFrame() {
  CHECK_EQ(io_state_, STATE_SEND_HEADERS_COMPLETE);
  CHECK(request_.get());
  CHECK_GT(stream_id_, 0u);

  SpdyControlFlags flags =
      has_upload_data_ ? CONTROL_FLAG_NONE : CONTROL_FLAG_FIN;
  scoped_ptr<SpdyFrame> frame(session_->CreateSynStream(
      stream_id_, priority_, slot_, flags, *request_));
  send_time_ = base::TimeTicks::Now();
  return frame.Pass();
}

scoped_ptr<SpdyFrame> SpdyStream::ProduceHeaderFrame(
    scoped_ptr<SpdyHeaderBlock> header_block) {
  CHECK(!cancelled());
  // We must need to write stream data.
  // Until the headers have been completely sent, we can not be sure
  // that our stream_id is correct.
  DCHECK_GT(io_state_, STATE_SEND_HEADERS_COMPLETE);
  DCHECK_GT(stream_id_, 0u);

  // Create actual HEADERS frame just in time because it depends on
  // compression context and should not be reordered after the creation.
  scoped_ptr<SpdyFrame> header_frame(session_->CreateHeadersFrame(
      stream_id_, *header_block, SpdyControlFlags()));
  return header_frame.Pass();
}

void SpdyStream::DetachDelegate() {
  delegate_ = NULL;
  if (!closed())
    Cancel();
}

const SpdyHeaderBlock& SpdyStream::spdy_headers() const {
  DCHECK(request_ != NULL);
  return *request_.get();
}

void SpdyStream::set_spdy_headers(scoped_ptr<SpdyHeaderBlock> headers) {
  request_.reset(headers.release());
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

int SpdyStream::OnResponseReceived(const SpdyHeaderBlock& response) {
  int rv = OK;

  metrics_.StartStream();

  DCHECK(response_->empty());
  *response_ = response;  // TODO(ukai): avoid copy.

  recv_first_byte_time_ = base::TimeTicks::Now();
  response_time_ = base::Time::Now();

  // If we receive a response before we are in STATE_WAITING_FOR_RESPONSE, then
  // the server has sent the SYN_REPLY too early.
  if (!pushed_ && io_state_ != STATE_WAITING_FOR_RESPONSE)
    return ERR_SPDY_PROTOCOL_ERROR;
  if (pushed_)
    CHECK(io_state_ == STATE_NONE);
  io_state_ = STATE_OPEN;

  // Append all the headers into the response header block.
  for (SpdyHeaderBlock::const_iterator it = response.begin();
       it != response.end(); ++it) {
    // Disallow uppercase headers.
    if (ContainsUpperAscii(it->first)) {
      session_->ResetStream(stream_id_, RST_STREAM_PROTOCOL_ERROR,
                            "Upper case characters in header: " + it->first);
      response_status_ = ERR_SPDY_PROTOCOL_ERROR;
      return ERR_SPDY_PROTOCOL_ERROR;
    }
  }

  if ((*response_).find("transfer-encoding") != (*response_).end()) {
    session_->ResetStream(stream_id_, RST_STREAM_PROTOCOL_ERROR,
                         "Received transfer-encoding header");
    return ERR_SPDY_PROTOCOL_ERROR;
  }

  if (delegate_)
    rv = delegate_->OnResponseReceived(*response_, response_time_, rv);
  // If delegate_ is not yet attached, we'll call OnResponseReceived after the
  // delegate gets attached to the stream.

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
      session_->ResetStream(stream_id_, RST_STREAM_PROTOCOL_ERROR,
                            "Upper case characters in header: " + it->first);
      response_status_ = ERR_SPDY_PROTOCOL_ERROR;
      return ERR_SPDY_PROTOCOL_ERROR;
    }

    (*response_)[it->first] = it->second;
  }

  if ((*response_).find("transfer-encoding") != (*response_).end()) {
    session_->ResetStream(stream_id_, RST_STREAM_PROTOCOL_ERROR,
                         "Received transfer-encoding header");
    return ERR_SPDY_PROTOCOL_ERROR;
  }

  int rv = OK;
  if (delegate_) {
    rv = delegate_->OnResponseReceived(*response_, response_time_, rv);
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
    session_->CloseStream(stream_id_, ERR_SYN_REPLY_NOT_RECEIVED);
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
    session_->CloseStream(stream_id_, OK);
    // Note: |this| may be deleted after calling CloseStream.
    return;
  }

  size_t length = buffer->GetRemainingSize();
  DCHECK_LE(length, session_->GetDataFrameMaximumPayload());
  if (session_->flow_control_state() >= SpdySession::FLOW_CONTROL_STREAM) {
    DecreaseRecvWindowSize(static_cast<int32>(length));
    buffer->AddConsumeCallback(
        base::Bind(&SpdyStream::OnReadBufferConsumed,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  // Track our bandwidth.
  metrics_.RecordBytes(length);
  recv_bytes_ += length;
  recv_last_byte_time_ = base::TimeTicks::Now();

  if (delegate_->OnDataReceived(buffer.Pass()) != OK) {
    // |delegate_| rejected the data.
    LogStreamError(ERR_SPDY_PROTOCOL_ERROR, "Delegate rejected the data");
    session_->CloseStream(stream_id_, ERR_SPDY_PROTOCOL_ERROR);
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
  if (cancelled() || closed())
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
  io_state_ = STATE_DONE;
  response_status_ = status;
  Delegate* delegate = delegate_;
  delegate_ = NULL;
  if (delegate)
    delegate->OnClose(status);
}

void SpdyStream::Cancel() {
  if (cancelled())
    return;

  cancelled_ = true;
  if (session_->IsStreamActive(stream_id_))
    session_->ResetStream(stream_id_, RST_STREAM_CANCEL, std::string());
  else if (stream_id_ == 0)
    session_->CloseCreatedStream(this, RST_STREAM_CANCEL);
}

void SpdyStream::Close() {
  if (stream_id_ != 0)
    session_->CloseStream(stream_id_, OK);
  else
    session_->CloseCreatedStream(this, OK);
}

int SpdyStream::SendRequest(bool has_upload_data) {
  // Pushed streams do not send any data, and should always be
  // idle. However, we still want to return IO_PENDING to mimic
  // non-push behavior.
  has_upload_data_ = has_upload_data;
  if (pushed_) {
    DCHECK(is_idle());
    DCHECK(!has_upload_data_);
    DCHECK(response_received());
    send_time_ = base::TimeTicks::Now();
    return ERR_IO_PENDING;
  }
  CHECK_EQ(STATE_NONE, io_state_);
  io_state_ = STATE_GET_DOMAIN_BOUND_CERT;
  return DoLoop(OK);
}

void SpdyStream::QueueHeaders(scoped_ptr<SpdyHeaderBlock> headers) {
  // Until the first headers by SYN_STREAM have been completely sent, we can
  // not be sure that our stream_id is correct.
  DCHECK_GT(io_state_, STATE_SEND_HEADERS_COMPLETE);
  CHECK_GT(stream_id_, 0u);

  session_->EnqueueStreamWrite(
      this, HEADERS,
      scoped_ptr<SpdyBufferProducer>(
          new HeaderBufferProducer(
              weak_ptr_factory_.GetWeakPtr(), headers.Pass())));
}

void SpdyStream::QueueStreamData(IOBuffer* data,
                                 int length,
                                 SpdyDataFlags flags) {
  // Until the headers have been completely sent, we can not be sure
  // that our stream_id is correct.
  DCHECK_GT(io_state_, STATE_SEND_HEADERS_COMPLETE);
  CHECK_GT(stream_id_, 0u);
  CHECK(!cancelled());

  scoped_ptr<SpdyBuffer> data_buffer(session_->CreateDataBuffer(
      stream_id_, data, length, flags));
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
                   weak_ptr_factory_.GetWeakPtr(),
                   payload_size));
  }

  session_->EnqueueStreamWrite(
      this, DATA,
      scoped_ptr<SpdyBufferProducer>(
          new SimpleBufferProducer(data_buffer.Pass())));
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
    io_state_ = STATE_SEND_BODY;
    DoLoop(OK);
  }
}

bool SpdyStream::HasUrl() const {
  if (pushed_)
    return response_received();
  return request_.get() != NULL;
}

GURL SpdyStream::GetUrl() const {
  DCHECK(HasUrl());

  const SpdyHeaderBlock& headers = (pushed_) ? *response_ : *request_;
  return GetUrlFromHeaderBlock(headers, GetProtocolVersion(), pushed_);
}

void SpdyStream::OnGetDomainBoundCertComplete(int result) {
  DCHECK_EQ(STATE_GET_DOMAIN_BOUND_CERT_COMPLETE, io_state_);
  DoLoop(result);
}

int SpdyStream::DoLoop(int result) {
  do {
    State state = io_state_;
    io_state_ = STATE_NONE;
    switch (state) {
      // State machine 1: Send headers and body.
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
        CHECK_EQ(result, OK);
        result = DoSendDomainBoundCertComplete();
        break;
      case STATE_SEND_HEADERS:
        CHECK_EQ(result, OK);
        result = DoSendHeaders();
        break;
      case STATE_SEND_HEADERS_COMPLETE:
        CHECK_EQ(result, OK);
        result = DoSendHeadersComplete();
        break;
      case STATE_SEND_BODY:
        CHECK_EQ(result, OK);
        result = DoSendBody();
        break;
      case STATE_SEND_BODY_COMPLETE:
        CHECK_EQ(result, OK);
        result = DoSendBodyComplete();
        break;
      // This is an intermediary waiting state. This state is reached when all
      // data has been sent, but no data has been received.
      case STATE_WAITING_FOR_RESPONSE:
        io_state_ = STATE_WAITING_FOR_RESPONSE;
        result = ERR_IO_PENDING;
        break;
      // State machine 2: connection is established.
      // In STATE_OPEN, OnResponseReceived has already been called.
      // OnDataReceived, OnClose and OnFrameWriteComplete can be called.
      // Only OnFrameWriteComplete calls DoLoop().
      //
      // For HTTP streams, no data is sent from the client while in the OPEN
      // state, so OnFrameWriteComplete is never called here.  The HTTP body is
      // handled in the OnDataReceived callback, which does not call into
      // DoLoop.
      //
      // For WebSocket streams, which are bi-directional, we'll send and
      // receive data once the connection is established.  Received data is
      // handled in OnDataReceived.  Sent data is handled in
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

  return result;
}

int SpdyStream::DoGetDomainBoundCert() {
  CHECK(request_.get());
  GURL url = GetUrl();
  if (!session_->NeedsCredentials() || pushed_ || !url.SchemeIs("https")) {
    // Proceed directly to sending headers
    io_state_ = STATE_SEND_HEADERS;
    return OK;
  }

  slot_ = session_->credential_state()->FindCredentialSlot(GetUrl());
  if (slot_ != SpdyCredentialState::kNoEntry) {
    // Proceed directly to sending headers
    io_state_ = STATE_SEND_HEADERS;
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
      base::Bind(&SpdyStream::OnGetDomainBoundCertComplete,
                 weak_ptr_factory_.GetWeakPtr()),
      &domain_bound_cert_request_handle_);
  return rv;
}

int SpdyStream::DoGetDomainBoundCertComplete(int result) {
  if (result != OK)
    return result;

  io_state_ = STATE_SEND_DOMAIN_BOUND_CERT;
  slot_ =  session_->credential_state()->SetHasCredential(GetUrl());
  return OK;
}

int SpdyStream::DoSendDomainBoundCert() {
  io_state_ = STATE_SEND_DOMAIN_BOUND_CERT_COMPLETE;
  CHECK(request_.get());

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
      this, CREDENTIAL,
      scoped_ptr<SpdyBufferProducer>(
          new SimpleBufferProducer(
              scoped_ptr<SpdyBuffer>(new SpdyBuffer(frame.Pass())))));
  return ERR_IO_PENDING;
}

int SpdyStream::DoSendDomainBoundCertComplete() {
  DCHECK_EQ(just_completed_frame_type_, CREDENTIAL);
  io_state_ = STATE_SEND_HEADERS;
  return OK;
}

int SpdyStream::DoSendHeaders() {
  CHECK(!cancelled_);
  io_state_ = STATE_SEND_HEADERS_COMPLETE;

  session_->EnqueueStreamWrite(
      this, SYN_STREAM,
      scoped_ptr<SpdyBufferProducer>(
          new SynStreamBufferProducer(weak_ptr_factory_.GetWeakPtr())));
  return ERR_IO_PENDING;
}

int SpdyStream::DoSendHeadersComplete() {
  DCHECK_EQ(just_completed_frame_type_, SYN_STREAM);
  DCHECK_NE(stream_id_, 0u);
  if (!delegate_)
    return ERR_UNEXPECTED;

  io_state_ =
      (delegate_->OnSendHeadersComplete() == MORE_DATA_TO_SEND) ?
      STATE_SEND_BODY : STATE_WAITING_FOR_RESPONSE;

  return OK;
}

// DoSendBody is called to send the optional body for the request.  This call
// will also be called as each write of a chunk of the body completes.
int SpdyStream::DoSendBody() {
  // If we're already in the STATE_SEND_BODY state, then we've already
  // sent a portion of the body.  In that case, we need to first consume
  // the bytes written in the body stream.  Note that the bytes written is
  // the number of bytes in the frame that were written, only consume the
  // data portion, of course.
  io_state_ = STATE_SEND_BODY_COMPLETE;
  if (!delegate_)
    return ERR_UNEXPECTED;
  return delegate_->OnSendBody();
}

int SpdyStream::DoSendBodyComplete() {
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

  if (!delegate_) {
    NOTREACHED();
    return ERR_UNEXPECTED;
  }

  send_bytes_ += frame_payload_size;

  io_state_ =
      (delegate_->OnSendBodyComplete(frame_payload_size) == MORE_DATA_TO_SEND) ?
      STATE_SEND_BODY : STATE_WAITING_FOR_RESPONSE;

  return OK;
}

int SpdyStream::DoOpen() {
  io_state_ = STATE_OPEN;

  switch (just_completed_frame_type_) {
    case DATA: {
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

      send_bytes_ += frame_payload_size;
      if (delegate_)
        delegate_->OnDataSent(frame_payload_size);
      break;
    }

    case HEADERS:
      if (delegate_)
        delegate_->OnHeadersSent();
      break;

    default:
      NOTREACHED();
      return ERR_UNEXPECTED;
  }

  return OK;
}

void SpdyStream::UpdateHistograms() {
  // We need all timers to be filled in, otherwise metrics can be bogus.
  if (send_time_.is_null() || recv_first_byte_time_.is_null() ||
      recv_last_byte_time_.is_null())
    return;

  UMA_HISTOGRAM_TIMES("Net.SpdyStreamTimeToFirstByte",
      recv_first_byte_time_ - send_time_);
  UMA_HISTOGRAM_TIMES("Net.SpdyStreamDownloadTime",
      recv_last_byte_time_ - recv_first_byte_time_);
  UMA_HISTOGRAM_TIMES("Net.SpdyStreamTime",
      recv_last_byte_time_ - send_time_);

  UMA_HISTOGRAM_COUNTS("Net.SpdySendBytes", send_bytes_);
  UMA_HISTOGRAM_COUNTS("Net.SpdyRecvBytes", recv_bytes_);
}

}  // namespace net
