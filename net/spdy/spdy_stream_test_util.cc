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
    const scoped_refptr<SpdyStream>& stream) : stream_(stream) {}

ClosingDelegate::~ClosingDelegate() {}

SpdySendStatus ClosingDelegate::OnSendHeadersComplete() {
  return NO_MORE_DATA_TO_SEND;
}

int ClosingDelegate::OnSendBody() {
  return OK;
}

SpdySendStatus ClosingDelegate::OnSendBodyComplete(size_t /*bytes_sent*/) {
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

void ClosingDelegate::OnDataSent(size_t bytes_sent) {}

void ClosingDelegate::OnClose(int status) {
  if (stream_)
    stream_->Close();
  stream_ = NULL;
}

StreamDelegateBase::StreamDelegateBase(
    const scoped_refptr<SpdyStream>& stream)
    : stream_(stream),
      send_headers_completed_(false),
      headers_sent_(0),
      data_sent_(0) {
}

StreamDelegateBase::~StreamDelegateBase() {
}

SpdySendStatus StreamDelegateBase::OnSendHeadersComplete() {
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

void StreamDelegateBase::OnHeadersSent() {
  headers_sent_++;
}

int StreamDelegateBase::OnDataReceived(scoped_ptr<SpdyBuffer> buffer) {
  if (buffer)
    received_data_queue_.Enqueue(buffer.Pass());
  return OK;
}

void StreamDelegateBase::OnDataSent(size_t bytes_sent) {
  data_sent_ += bytes_sent;
}

void StreamDelegateBase::OnClose(int status) {
  if (!stream_)
    return;
  stream_ = NULL;
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

StreamDelegateSendImmediate::StreamDelegateSendImmediate(
    const scoped_refptr<SpdyStream>& stream,
    scoped_ptr<SpdyHeaderBlock> headers,
    base::StringPiece data)
    : StreamDelegateBase(stream),
      headers_(headers.Pass()),
      data_(data) {}

StreamDelegateSendImmediate::~StreamDelegateSendImmediate() {
}

int StreamDelegateSendImmediate::OnSendBody() {
  ADD_FAILURE() << "OnSendBody should not be called";
  return ERR_UNEXPECTED;
}
SpdySendStatus StreamDelegateSendImmediate::OnSendBodyComplete(
    size_t /*bytes_sent*/) {
  ADD_FAILURE() << "OnSendBodyComplete should not be called";
  return NO_MORE_DATA_TO_SEND;
}

int StreamDelegateSendImmediate::OnResponseReceived(
    const SpdyHeaderBlock& response,
    base::Time response_time,
    int status) {
  status =
      StreamDelegateBase::OnResponseReceived(response, response_time, status);
  if (headers_.get()) {
    stream()->QueueHeaders(headers_.Pass());
  }
  if (data_.data()) {
    scoped_refptr<StringIOBuffer> buf(new StringIOBuffer(data_.as_string()));
    stream()->QueueStreamData(buf, buf->size(), DATA_FLAG_NONE);
  }
  return status;
}

StreamDelegateWithBody::StreamDelegateWithBody(
    const scoped_refptr<SpdyStream>& stream,
    base::StringPiece data)
    : StreamDelegateBase(stream),
      buf_(new DrainableIOBuffer(new StringIOBuffer(data.as_string()),
                                 data.size())),
      body_data_sent_(0) {}

StreamDelegateWithBody::~StreamDelegateWithBody() {
}

int StreamDelegateWithBody::OnSendBody() {
  stream()->QueueStreamData(buf_.get(), buf_->BytesRemaining(),
                            DATA_FLAG_NONE);
  return ERR_IO_PENDING;
}

SpdySendStatus StreamDelegateWithBody::OnSendBodyComplete(size_t bytes_sent) {
  EXPECT_GT(bytes_sent, 0u);

  buf_->DidConsume(bytes_sent);
  body_data_sent_ += bytes_sent;
  if (buf_->BytesRemaining() > 0) {
    // Go back to OnSendBody() to send the remaining data.
    return MORE_DATA_TO_SEND;
  }

  return NO_MORE_DATA_TO_SEND;
}

} // namespace test

} // namespace net
