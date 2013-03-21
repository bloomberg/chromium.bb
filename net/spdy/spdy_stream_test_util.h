// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_STREAM_TEST_UTIL_H_
#define NET_SPDY_SPDY_STREAM_TEST_UTIL_H_

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "net/base/io_buffer.h"
#include "net/base/test_completion_callback.h"
#include "net/spdy/spdy_stream.h"

namespace net {

namespace test {

// Delegate that calls Close() on |stream_| on OnClose. Used by tests
// to make sure that such an action is harmless.
class ClosingDelegate : public SpdyStream::Delegate {
 public:
  explicit ClosingDelegate(const scoped_refptr<SpdyStream>& stream);
  virtual ~ClosingDelegate();

  // SpdyStream::Delegate implementation.
  virtual bool OnSendHeadersComplete(int status) OVERRIDE;
  virtual int OnSendBody() OVERRIDE;
  virtual int OnSendBodyComplete(int status, bool* eof) OVERRIDE;
  virtual int OnResponseReceived(const SpdyHeaderBlock& response,
                                 base::Time response_time,
                                 int status) OVERRIDE;
  virtual void OnHeadersSent() OVERRIDE;
  virtual int OnDataReceived(const char* data, int length) OVERRIDE;
  virtual void OnDataSent(int length) OVERRIDE;
  virtual void OnClose(int status) OVERRIDE;

 private:
  scoped_refptr<SpdyStream> stream_;
};

// Base class with shared functionality for test delegate
// implementations below.
class StreamDelegateBase : public SpdyStream::Delegate {
 public:
  explicit StreamDelegateBase(const scoped_refptr<SpdyStream>& stream);
  virtual ~StreamDelegateBase();

  virtual bool OnSendHeadersComplete(int status) OVERRIDE;
  virtual int OnSendBody() = 0;
  virtual int OnSendBodyComplete(int status, bool* eof) = 0;
  virtual int OnResponseReceived(const SpdyHeaderBlock& response,
                                 base::Time response_time,
                                 int status) OVERRIDE;
  virtual void OnHeadersSent() OVERRIDE;
  virtual int OnDataReceived(const char* buffer, int bytes) OVERRIDE;
  virtual void OnDataSent(int length) OVERRIDE;
  virtual void OnClose(int status) OVERRIDE;

  // Waits for the stream to be closed and returns the status passed
  // to OnClose().
  int WaitForClose();

  std::string GetResponseHeaderValue(const std::string& name) const;
  bool send_headers_completed() const { return send_headers_completed_; }
  const std::string& received_data() const { return received_data_; }
  int headers_sent() const { return headers_sent_; }
  int data_sent() const { return data_sent_; }

 protected:
  const scoped_refptr<SpdyStream>& stream() { return stream_; }

 private:
  scoped_refptr<SpdyStream> stream_;
  TestCompletionCallback callback_;
  bool send_headers_completed_;
  SpdyHeaderBlock response_;
  std::string received_data_;
  int headers_sent_;
  int data_sent_;
};

// Test delegate that sends data immediately in OnResponseReceived().
class StreamDelegateSendImmediate : public StreamDelegateBase {
 public:
  // Both |headers| and |buf| can be NULL.
  StreamDelegateSendImmediate(const scoped_refptr<SpdyStream>& stream,
                              scoped_ptr<SpdyHeaderBlock> headers,
                              base::StringPiece data);
  virtual ~StreamDelegateSendImmediate();

  virtual int OnSendBody() OVERRIDE;
  virtual int OnSendBodyComplete(int status, bool* eof) OVERRIDE;
  virtual int OnResponseReceived(const SpdyHeaderBlock& response,
                                 base::Time response_time,
                                 int status) OVERRIDE;

 private:
  scoped_ptr<SpdyHeaderBlock> headers_;
  base::StringPiece data_;
};

// Test delegate that sends body data.
class StreamDelegateWithBody : public StreamDelegateBase {
 public:
  StreamDelegateWithBody(const scoped_refptr<SpdyStream>& stream,
                         base::StringPiece data);
  virtual ~StreamDelegateWithBody();

  virtual int OnSendBody() OVERRIDE;
  virtual int OnSendBodyComplete(int status, bool* eof) OVERRIDE;

  int body_data_sent() const { return body_data_sent_; }

 private:
  scoped_refptr<DrainableIOBuffer> buf_;
  int body_data_sent_;
};

} // namespace test

} // namespace net

#endif // NET_SPDY_SPDY_STREAM_TEST_UTIL_H_
