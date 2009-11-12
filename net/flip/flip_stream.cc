// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/flip/flip_stream.h"

#include "net/flip/flip_session.h"
#include "net/http/http_response_info.h"

namespace net {

bool FlipStream::AttachDelegate(FlipDelegate* delegate) {
  DCHECK(delegate_ == NULL);  // Don't attach if already attached.
  DCHECK(path_.length() > 0);  // Path needs to be set for push streams.
  delegate_ = delegate;

  // If there is pending data, send it up here.

  // Check for the OnReply, and pass it up.
  if (response_.get())
    delegate_->OnResponseReceived(response_.get());

  // Pass data up
  while (response_body_.size()) {
    scoped_refptr<IOBufferWithSize> buffer = response_body_.front();
    response_body_.pop_front();
    delegate_->OnDataReceived(buffer->data(), buffer->size());
  }

  // Finally send up the end-of-stream.
  if (download_finished_) {
    delegate_->OnClose(net::OK);
    return true;  // tell the caller to shut us down
  }
  return false;
}

void FlipStream::OnReply(const flip::FlipHeaderBlock* headers) {
  DCHECK(headers);
  DCHECK(headers->find("version") != headers->end());
  DCHECK(headers->find("status") != headers->end());

  // TODO(mbelshe): if no version or status is found, we need to error
  // out the stream.

  metrics_.StartStream();

  // Server initiated streams must send a URL to us in the headers.
  if (headers->find("path") != headers->end())
    path_ = headers->find("path")->second;

  // TODO(mbelshe): For now we convert from our nice hash map back
  // to a string of headers; this is because the HttpResponseInfo
  // is a bit rigid for its http (non-flip) design.
  std::string raw_headers(headers->find("version")->second);
  raw_headers.append(" ", 1);
  raw_headers.append(headers->find("status")->second);
  raw_headers.append("\0", 1);
  flip::FlipHeaderBlock::const_iterator it;
  for (it = headers->begin(); it != headers->end(); ++it) {
    raw_headers.append(it->first);
    raw_headers.append(":", 1);
    raw_headers.append(it->second);
    raw_headers.append("\0", 1);
  }

  LOG(INFO) << "FlipStream: SynReply received for " << stream_id_;

  DCHECK(response_ == NULL);
  response_.reset(new HttpResponseInfo());
  response_->headers = new HttpResponseHeaders(raw_headers);
  // When pushing content from the server, we may not yet have a delegate_
  // to notify.  When the delegate is attached, it will notify then.
  if (delegate_)
    delegate_->OnResponseReceived(response_.get());
}

bool FlipStream::OnData(const char* data, int length) {
  DCHECK(length >= 0);
  LOG(INFO) << "FlipStream: Data (" << length << " bytes) received for "
            << stream_id_;

  // If we don't have a response, then the SYN_REPLY did not come through.
  // We cannot pass data up to the caller unless the reply headers have been
  // received.
  if (!response_.get()) {
    if (delegate_)
      delegate_->OnClose(ERR_SYN_REPLY_NOT_RECEIVED);
    return true;
  }

  // A zero-length read means that the stream is being closed.
  if (!length) {
    metrics_.StopStream();
    download_finished_ = true;
    if (delegate_) {
      delegate_->OnClose(net::OK);
      return true;  // Tell the caller to clean us up.
    }
  }

  // Track our bandwidth.
  metrics_.RecordBytes(length);

  // We've received data.  We try to pass it up to the caller.
  // In the case of server-push streams, we may not have a delegate yet assigned
  // to this stream.  In that case we just queue the data for later.
  if (delegate_) {
    delegate_->OnDataReceived(data, length);
  } else {
    // Save the data for use later.
    // TODO(mbelshe): We need to have some throttling on this.  We shouldn't
    //                buffer an infinite amount of data.
    IOBufferWithSize* io_buffer = new IOBufferWithSize(length);
    memcpy(io_buffer->data(), data, length);
    response_body_.push_back(io_buffer);
  }
  return false;
}

void FlipStream::OnError(int err) {
  if (delegate_)
    delegate_->OnClose(err);
}

int FlipStream::OnWriteComplete(int result) {
  // We only write to the socket when there is a delegate.
  DCHECK(delegate_);

  delegate_->OnWriteComplete(result);

  // TODO(mbelshe): we might want to remove the status code
  //                since we're not doing anything useful with it.
  return OK;
}

}  // namespace net

