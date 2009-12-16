// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/flip/flip_stream.h"

#include "base/logging.h"
#include "net/flip/flip_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"

namespace net {

FlipStream::FlipStream(FlipSession* session,
                       flip::FlipStreamId stream_id,
                       bool pushed)
    : stream_id_(stream_id),
      priority_(0),
      pushed_(pushed),
      download_finished_(false),
      metrics_(Singleton<BandwidthMetrics>::get()),
      session_(session),
      response_(NULL),
      request_body_stream_(NULL),
      response_complete_(false),
      io_state_(STATE_NONE),
      response_status_(OK),
      user_callback_(NULL),
      user_buffer_(NULL),
      user_buffer_len_(0),
      cancelled_(false) {}

FlipStream::~FlipStream() {
  DLOG(INFO) << "Deleting FlipStream for stream " << stream_id_;

  // TODO(willchan): We're still calling CancelStream() too many times, because
  // inactive pending/pushed streams will still have stream_id_ set.
  if (stream_id_) {
    session_->CancelStream(stream_id_);
  } else if (!response_complete_) {
    NOTREACHED();
  }
}

uint64 FlipStream::GetUploadProgress() const {
  if (!request_body_stream_.get())
    return 0;

  return request_body_stream_->position();
}

const HttpResponseInfo* FlipStream::GetResponseInfo() const {
  return response_;
}

int FlipStream::ReadResponseHeaders(CompletionCallback* callback) {
  // Note: The FlipStream may have already received the response headers, so
  //       this call may complete synchronously.
  CHECK(callback);
  CHECK(io_state_ == STATE_NONE);
  CHECK(!cancelled_);

  // The SYN_REPLY has already been received.
  if (response_->headers)
    return OK;

  io_state_ = STATE_READ_HEADERS;
  CHECK(!user_callback_);
  user_callback_ = callback;
  return ERR_IO_PENDING;
}

int FlipStream::ReadResponseBody(
    IOBuffer* buf, int buf_len, CompletionCallback* callback) {
  DCHECK_EQ(io_state_, STATE_NONE);
  CHECK(buf);
  CHECK(buf_len);
  CHECK(callback);
  CHECK(!cancelled_);

  // If we have data buffered, complete the IO immediately.
  if (response_body_.size()) {
    int bytes_read = 0;
    while (response_body_.size() && buf_len > 0) {
      scoped_refptr<IOBufferWithSize> data = response_body_.front();
      const int bytes_to_copy = std::min(buf_len, data->size());
      memcpy(&(buf->data()[bytes_read]), data->data(), bytes_to_copy);
      buf_len -= bytes_to_copy;
      if (bytes_to_copy == data->size()) {
        response_body_.pop_front();
      } else {
        const int bytes_remaining = data->size() - bytes_to_copy;
        IOBufferWithSize* new_buffer = new IOBufferWithSize(bytes_remaining);
        memcpy(new_buffer->data(), &(data->data()[bytes_to_copy]),
               bytes_remaining);
        response_body_.pop_front();
        response_body_.push_front(new_buffer);
      }
      bytes_read += bytes_to_copy;
    }
    return bytes_read;
  } else if (response_complete_) {
    return response_status_;
  }

  CHECK(!user_callback_);
  CHECK(!user_buffer_);
  CHECK(user_buffer_len_ == 0);

  user_callback_ = callback;
  user_buffer_ = buf;
  user_buffer_len_ = buf_len;
  return ERR_IO_PENDING;
}

int FlipStream::SendRequest(UploadDataStream* upload_data,
                            HttpResponseInfo* response,
                            CompletionCallback* callback) {
  CHECK(callback);
  CHECK(!cancelled_);
  CHECK(response);

  response_ = response;

  if (upload_data) {
    if (upload_data->size())
      request_body_stream_.reset(upload_data);
    else
      delete upload_data;
  }

  DCHECK_EQ(io_state_, STATE_NONE);
  if (!pushed_)
    io_state_ = STATE_SEND_HEADERS;
  else
    io_state_ = STATE_READ_HEADERS;
  int result = DoLoop(OK);
  if (result == ERR_IO_PENDING) {
    CHECK(!user_callback_);
    user_callback_ = callback;
  }
  return result;
}

void FlipStream::Cancel() {
  cancelled_ = true;
  user_callback_ = NULL;

  // TODO(willchan): Should we also call FlipSession::CancelStream()?  What
  // makes more sense?  If we cancel the stream, perhaps we should send the
  // server a control packet to tell it to stop sending to the dead stream.  But
  // then does FlipSession deactivate the stream?  It would log warnings for
  // data packets for an invalid stream then, unless we maintained some data
  // structure to keep track of cancelled stream ids.  Ugh.  Currently
  // FlipStream does not call CancelStream().  We should free up all the memory
  // associated with the stream though (ditch all the IOBuffers) and at all
  // FlipSession's entrypoints into FlipStream check |cancelled_| and not copy
  // the data.
}

void FlipStream::OnResponseReceived(const HttpResponseInfo& response) {
  metrics_.StartStream();

  CHECK(!response_->headers);

  *response_ = response;  // TODO(mbelshe): avoid copy.
  DCHECK(response_->headers);

  if (io_state_ == STATE_NONE) {
    CHECK(pushed_);
  } else if (io_state_ == STATE_READ_HEADERS_COMPLETE) {
    CHECK(!pushed_);
  } else {
    NOTREACHED();
  }

  int rv = DoLoop(OK);

  if (user_callback_)
    DoCallback(rv);
}

bool FlipStream::OnDataReceived(const char* data, int length) {
  DCHECK_GE(length, 0);
  LOG(INFO) << "FlipStream: Data (" << length << " bytes) received for "
            << stream_id_;

  // If we don't have a response, then the SYN_REPLY did not come through.
  // We cannot pass data up to the caller unless the reply headers have been
  // received.
  if (!response_->headers) {
    OnClose(ERR_SYN_REPLY_NOT_RECEIVED);
    return false;
  }

  // A zero-length read means that the stream is being closed.
  if (!length) {
    metrics_.StopStream();
    download_finished_ = true;
    OnClose(net::OK);
    return true;
  }

  // Track our bandwidth.
  metrics_.RecordBytes(length);

  if (length > 0) {
    // TODO(mbelshe): If read is pending, we should copy the data straight into
    // the read buffer here.  For now, we'll queue it always.
    // TODO(mbelshe): We need to have some throttling on this.  We shouldn't
    //                buffer an infinite amount of data.

    IOBufferWithSize* io_buffer = new IOBufferWithSize(length);
    memcpy(io_buffer->data(), data, length);

    response_body_.push_back(io_buffer);
  }

  // Note that data may be received for a FlipStream prior to the user calling
  // ReadResponseBody(), therefore user_callback_ may be NULL.  This may often
  // happen for server initiated streams.
  if (user_callback_) {
    int rv = ReadResponseBody(user_buffer_, user_buffer_len_, user_callback_);
    CHECK(rv != ERR_IO_PENDING);
    user_buffer_ = NULL;
    user_buffer_len_ = 0;
    DoCallback(rv);
  }

  return true;
}

void FlipStream::OnWriteComplete(int status) {
  DoLoop(status);
}

void FlipStream::OnClose(int status) {
  response_complete_ = true;
  response_status_ = status;
  stream_id_ = 0;

  if (user_callback_)
    DoCallback(status);
}

int FlipStream::DoLoop(int result) {
  do {
    State state = io_state_;
    io_state_ = STATE_NONE;
    switch (state) {
      // State machine 1: Send headers and wait for response headers.
      case STATE_SEND_HEADERS:
        CHECK(result == OK);
        result = DoSendHeaders();
        break;
      case STATE_SEND_HEADERS_COMPLETE:
        result = DoSendHeadersComplete(result);
        break;
      case STATE_SEND_BODY:
        CHECK(result == OK);
        result = DoSendBody();
        break;
      case STATE_SEND_BODY_COMPLETE:
        result = DoSendBodyComplete(result);
        break;
      case STATE_READ_HEADERS:
        CHECK(result == OK);
        result = DoReadHeaders();
        break;
      case STATE_READ_HEADERS_COMPLETE:
        result = DoReadHeadersComplete(result);
        break;

      // State machine 2: Read body.
      // NOTE(willchan): Currently unused.  Currently we handle this stuff in
      // the OnDataReceived()/OnClose()/ReadResponseHeaders()/etc.  Only reason
      // to do this is for consistency with the Http code.
      case STATE_READ_BODY:
        result = DoReadBody();
        break;
      case STATE_READ_BODY_COMPLETE:
        result = DoReadBodyComplete(result);
        break;
      case STATE_DONE:
        DCHECK(result != ERR_IO_PENDING);
        break;
      default:
        NOTREACHED();
        break;
    }
  } while (result != ERR_IO_PENDING && io_state_ != STATE_NONE);

  return result;
}

void FlipStream::DoCallback(int rv) {
  CHECK(rv != ERR_IO_PENDING);
  CHECK(user_callback_);

  // Since Run may result in being called back, clear user_callback_ in advance.
  CompletionCallback* c = user_callback_;
  user_callback_ = NULL;
  c->Run(rv);
}

int FlipStream::DoSendHeaders() {
  // The FlipSession will always call us back when the send is complete.
  // TODO(willchan): This code makes the assumption that for the non-push stream
  // case, the client code calls SendRequest() after creating the stream and
  // before yielding back to the MessageLoop.  This is true in the current code,
  // but is not obvious from the headers.  We should make the code handle
  // SendRequest() being called after the SYN_REPLY has been received.
  io_state_ = STATE_SEND_HEADERS_COMPLETE;
  return ERR_IO_PENDING;
}

int FlipStream::DoSendHeadersComplete(int result) {
  if (result < 0)
    return result;

  CHECK(result > 0);

  // There is no body, skip that state.
  if (!request_body_stream_.get()) {
    io_state_ = STATE_READ_HEADERS;
    return OK;
  }

  io_state_ = STATE_SEND_BODY;
  return OK;
}

// DoSendBody is called to send the optional body for the request.  This call
// will also be called as each write of a chunk of the body completes.
int FlipStream::DoSendBody() {
  // If we're already in the STATE_SENDING_BODY state, then we've already
  // sent a portion of the body.  In that case, we need to first consume
  // the bytes written in the body stream.  Note that the bytes written is
  // the number of bytes in the frame that were written, only consume the
  // data portion, of course.
  io_state_ = STATE_SEND_BODY_COMPLETE;
  int buf_len = static_cast<int>(request_body_stream_->buf_len());
  return session_->WriteStreamData(stream_id_,
                                   request_body_stream_->buf(),
                                   buf_len);
}

int FlipStream::DoSendBodyComplete(int result) {
  if (result < 0)
    return result;

  CHECK(result != 0);

  request_body_stream_->DidConsume(result);

  if (request_body_stream_->position() < request_body_stream_->size())
    io_state_ = STATE_SEND_BODY;
  else
    io_state_ = STATE_READ_HEADERS;

  return OK;
}

int FlipStream::DoReadHeaders() {
  io_state_ = STATE_READ_HEADERS_COMPLETE;
  return response_->headers ? OK : ERR_IO_PENDING;
}

int FlipStream::DoReadHeadersComplete(int result) {
  return result;
}

int FlipStream::DoReadBody() {
  // TODO(mbelshe): merge FlipStreamParser with FlipStream and then this
  // makes sense.
  return ERR_IO_PENDING;
}

int FlipStream::DoReadBodyComplete(int result) {
  // TODO(mbelshe): merge FlipStreamParser with FlipStream and then this
  // makes sense.
  return ERR_IO_PENDING;
}

}  // namespace net
