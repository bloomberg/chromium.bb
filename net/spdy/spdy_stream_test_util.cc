// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_stream_test_util.h"

#include "net/base/completion_callback.h"
#include "net/spdy/spdy_stream.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace test {

ClosingDelegate::ClosingDelegate(
    const scoped_refptr<SpdyStream>& stream) : stream_(stream) {}

ClosingDelegate::~ClosingDelegate() {}

bool ClosingDelegate::OnSendHeadersComplete(int status) {
  return true;
}

int ClosingDelegate::OnSendBody() {
  return OK;
}

int ClosingDelegate::OnSendBodyComplete(int status, bool* eof) {
  return OK;
}
int ClosingDelegate::OnResponseReceived(const SpdyHeaderBlock& response,
                                        base::Time response_time,
                                        int status) {
  return OK;
}

void ClosingDelegate::OnHeadersSent() {}

int ClosingDelegate::OnDataReceived(const char* data, int length) {
  return OK;
}

void ClosingDelegate::OnDataSent(int length) {}

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

bool StreamDelegateBase::OnSendHeadersComplete(int status) {
  send_headers_completed_ = true;
  return true;
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

int StreamDelegateBase::OnDataReceived(const char* buffer, int bytes) {
  received_data_ += std::string(buffer, bytes);
  return OK;
}

void StreamDelegateBase::OnDataSent(int length) {
  data_sent_ += length;
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

std::string StreamDelegateBase::GetResponseHeaderValue(
    const std::string& name) const {
  SpdyHeaderBlock::const_iterator it = response_.find(name);
  return (it == response_.end()) ? "" : it->second;
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
int StreamDelegateSendImmediate::OnSendBodyComplete(
    int /*status*/,
    bool* /*eof*/) {
  ADD_FAILURE() << "OnSendBodyComplete should not be called";
  return ERR_UNEXPECTED;
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

int StreamDelegateWithBody::OnSendBodyComplete(int status, bool* eof) {
  EXPECT_GE(status, 0);

  *eof = false;

  if (status > 0) {
    buf_->DidConsume(status);
    body_data_sent_ += status;
    if (buf_->BytesRemaining() > 0) {
      // Go back to OnSendBody() to send the remaining data.
      return OK;
    }
  }

  // Check if the entire body data has been sent.
  *eof = (buf_->BytesRemaining() == 0);
  return status;
}

} // namespace test

} // namespace net
