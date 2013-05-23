// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_stream_test_util.h"

#include <cstddef>

#include "base/stl_util.h"
#include "net/base/completion_callback.h"
#include "net/spdy/spdy_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace test {

ClosingDelegate::ClosingDelegate(
    const base::WeakPtr<SpdyStream>& stream) : stream_(stream) {}

ClosingDelegate::~ClosingDelegate() {}

SpdySendStatus ClosingDelegate::OnSendHeadersComplete() {
  return NO_MORE_DATA_TO_SEND;
}

void ClosingDelegate::OnSendBody() {
  ADD_FAILURE() << "OnSendBody should not be called";
}

SpdySendStatus ClosingDelegate::OnSendBodyComplete() {
  return NO_MORE_DATA_TO_SEND;
}

int ClosingDelegate::OnResponseReceived(const SpdyHeaderBlock& response,
                                        base::Time response_time,
                                        int status) {
  return OK;
}

void ClosingDelegate::OnHeadersSent() {}

int ClosingDelegate::OnDataReceived(scoped_ptr<SpdyBuffer> buffer) {
  return OK;
}

void ClosingDelegate::OnDataSent() {}

void ClosingDelegate::OnClose(int status) {
  DCHECK(stream_);
  stream_->Close();
  DCHECK(!stream_);
}

StreamDelegateBase::StreamDelegateBase(
    const base::WeakPtr<SpdyStream>& stream)
    : stream_(stream),
      stream_id_(0),
      send_headers_completed_(false) {
}

StreamDelegateBase::~StreamDelegateBase() {
}

SpdySendStatus StreamDelegateBase::OnSendHeadersComplete() {
  stream_id_ = stream_->stream_id();
  EXPECT_NE(stream_id_, 0u);
  send_headers_completed_ = true;
  return NO_MORE_DATA_TO_SEND;
}

int StreamDelegateBase::OnResponseReceived(const SpdyHeaderBlock& response,
                                           base::Time response_time,
                                           int status) {
  EXPECT_TRUE(send_headers_completed_);
  response_ = response;
  return status;
}

void StreamDelegateBase::OnHeadersSent() {}

int StreamDelegateBase::OnDataReceived(scoped_ptr<SpdyBuffer> buffer) {
  if (buffer)
    received_data_queue_.Enqueue(buffer.Pass());
  return OK;
}

void StreamDelegateBase::OnDataSent() {}

void StreamDelegateBase::OnClose(int status) {
  if (!stream_)
    return;
  stream_id_ = stream_->stream_id();
  stream_.reset();
  callback_.callback().Run(status);
}

int StreamDelegateBase::WaitForClose() {
  int result = callback_.WaitForResult();
  EXPECT_TRUE(!stream_.get());
  return result;
}

std::string StreamDelegateBase::TakeReceivedData() {
  size_t len = received_data_queue_.GetTotalSize();
  std::string received_data(len, '\0');
  if (len > 0) {
    EXPECT_EQ(
        len,
        received_data_queue_.Dequeue(string_as_array(&received_data), len));
  }
  return received_data;
}

std::string StreamDelegateBase::GetResponseHeaderValue(
    const std::string& name) const {
  SpdyHeaderBlock::const_iterator it = response_.find(name);
  return (it == response_.end()) ? std::string() : it->second;
}

StreamDelegateDoNothing::StreamDelegateDoNothing(
    const base::WeakPtr<SpdyStream>& stream)
    : StreamDelegateBase(stream) {}

StreamDelegateDoNothing::~StreamDelegateDoNothing() {
}

void StreamDelegateDoNothing::OnSendBody() {
  ADD_FAILURE() << "OnSendBody should not be called";
}

SpdySendStatus StreamDelegateDoNothing::OnSendBodyComplete() {
  return NO_MORE_DATA_TO_SEND;
}

StreamDelegateSendImmediate::StreamDelegateSendImmediate(
    const base::WeakPtr<SpdyStream>& stream,
    base::StringPiece data)
    : StreamDelegateBase(stream),
      data_(data) {}

StreamDelegateSendImmediate::~StreamDelegateSendImmediate() {
}

void StreamDelegateSendImmediate::OnSendBody() {
  ADD_FAILURE() << "OnSendBody should not be called";
}

SpdySendStatus StreamDelegateSendImmediate::OnSendBodyComplete() {
  ADD_FAILURE() << "OnSendBodyComplete should not be called";
  return NO_MORE_DATA_TO_SEND;
}

int StreamDelegateSendImmediate::OnResponseReceived(
    const SpdyHeaderBlock& response,
    base::Time response_time,
    int status) {
  status =
      StreamDelegateBase::OnResponseReceived(response, response_time, status);
  if (data_.data()) {
    scoped_refptr<StringIOBuffer> buf(new StringIOBuffer(data_.as_string()));
    stream()->SendStreamData(buf, buf->size(), DATA_FLAG_NONE);
  }
  return status;
}

StreamDelegateWithBody::StreamDelegateWithBody(
    const base::WeakPtr<SpdyStream>& stream,
    base::StringPiece data)
    : StreamDelegateBase(stream),
      buf_(new StringIOBuffer(data.as_string())) {}

StreamDelegateWithBody::~StreamDelegateWithBody() {
}

SpdySendStatus StreamDelegateWithBody::OnSendHeadersComplete() {
  StreamDelegateBase::OnSendHeadersComplete();
  return MORE_DATA_TO_SEND;
}

void StreamDelegateWithBody::OnSendBody() {
  stream()->SendStreamData(buf_.get(), buf_->size(), DATA_FLAG_NONE);
}

SpdySendStatus StreamDelegateWithBody::OnSendBodyComplete() {
  return NO_MORE_DATA_TO_SEND;
}

} // namespace test

} // namespace net
