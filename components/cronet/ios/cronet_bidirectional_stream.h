// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_IOS_CRONET_BIDIRECTIONAL_STREAM_H_
#define COMPONENTS_CRONET_IOS_CRONET_BIDIRECTIONAL_STREAM_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "net/http/bidirectional_stream.h"

namespace net {
class HttpRequestHeaders;
class WrappedIOBuffer;
}  // namespace net

namespace cronet {

class CronetEnvironment;

// An adapter to net::BidirectionalStream.
// Created and configured from any thread. Start, ReadData, WriteData and
// Destroy can be called on any thread (including network thread), and post
// calls to corresponding {Start|ReadData|WriteData|Destroy}OnNetworkThread to
// the network thread. The object is always deleted on network thread. All
// callbacks into the Delegate are done on the network thread.
// The app is expected to initiate the next step like ReadData or Destroy.
// Public methods can be called on any thread.
class CronetBidirectionalStream : public net::BidirectionalStream::Delegate {
 public:
  class Delegate {
   public:
    virtual void OnHeadersSent() = 0;

    virtual void OnHeadersReceived(const net::SpdyHeaderBlock& response_headers,
                                   const char* negotiated_protocol) = 0;

    virtual void OnDataRead(char* data, int size) = 0;

    virtual void OnDataSent(const char* data) = 0;

    virtual void OnTrailersReceived(const net::SpdyHeaderBlock& trailers) = 0;

    virtual void OnSucceeded() = 0;

    virtual void OnFailed(int error) = 0;

    virtual void OnCanceled() = 0;
  };

  CronetBidirectionalStream(CronetEnvironment* environment, Delegate* delegate);
  ~CronetBidirectionalStream() override;

  // Validates method and headers, initializes and starts the request. If
  // |end_of_stream| is true, then stream is half-closed after sending header
  // frame and no data is expected to be written.
  // Returns 0 if request is valid and started successfully,
  // Returns -1 if |method| is not valid HTTP method name.
  // Returns position of invalid header value in |headers| if header name is
  // not valid.
  int Start(const char* url,
            int priority,
            const char* method,
            const net::HttpRequestHeaders& headers,
            bool end_of_stream);

  // Reads more data into |buffer| up to |capacity| bytes.
  bool ReadData(char* buffer, int capacity);

  // Writes |count| bytes of data from |buffer|. The |end_of_stream| is
  // passed to remote to indicate end of stream.
  bool WriteData(const char* buffer, int count, bool end_of_stream);

  // Cancels the request. The OnCanceled callback is invoked when request is
  // caneceled, and not other callbacks are invoked afterwards..
  void Cancel();

  // Releases all resources for the request and deletes the object itself.
  void Destroy();

 private:
  // States of BidirectionalStream are tracked in |read_state_| and
  // |write_state_|.
  // The write state is separated as it changes independently of the read state.
  // There is one initial state: NOT_STARTED. There is one normal final state:
  // SUCCESS, reached after READING_DONE and WRITING_DONE. There are two
  // exceptional final states: CANCELED and ERROR, which can be reached from
  // any other non-final state.
  enum State {
    // Initial state, stream not started.
    NOT_STARTED,
    // Stream started, request headers are being sent.
    STARTED,
    // Waiting for ReadData() to be called.
    WAITING_FOR_READ,
    // Reading from the remote, OnDataRead callback will be invoked when done.
    READING,
    // There is no more data to read and stream is half-closed by the remote
    // side.
    READING_DONE,
    // Stream is canceled.
    CANCELED,
    // Error has occured, stream is closed.
    ERROR,
    // Reading and writing are done, and the stream is closed successfully.
    SUCCESS,
    // Waiting for WriteData() to be called.
    WAITING_FOR_WRITE,
    // Writing to the remote, callback will be invoked when done.
    WRITING,
    // There is no more data to write and stream is half-closed by the local
    // side.
    WRITING_DONE,
  };

  // net::BidirectionalStream::Delegate implementations:
  void OnHeadersSent() override;
  void OnHeadersReceived(const net::SpdyHeaderBlock& response_headers) override;
  void OnDataRead(int bytes_read) override;
  void OnDataSent() override;
  void OnTrailersReceived(const net::SpdyHeaderBlock& trailers) override;
  void OnFailed(int error) override;
  // Helper method to derive OnSucceeded.
  void MaybeOnSucceded();

  void StartOnNetworkThread(
      std::unique_ptr<net::BidirectionalStreamRequestInfo> request_info);
  void ReadDataOnNetworkThread(scoped_refptr<net::WrappedIOBuffer> read_buffer,
                               int buffer_size);
  void WriteDataOnNetworkThread(scoped_refptr<net::WrappedIOBuffer> read_buffer,
                                int buffer_size,
                                bool end_of_stream);
  void CancelOnNetworkThread();
  void DestroyOnNetworkThread();

  // Read state is tracking reading flow. Only accessed on network thread.
  //                         / <--- READING <--- \
  //                         |                   |
  //                         \                   /
  // NOT_STARTED -> STARTED --> WAITING_FOR_READ -> READING_DONE -> SUCCESS
  State read_state_;

  // Write state is tracking writing flow.  Only accessed on network thread.
  //                         / <--- WRITING <---  \
  //                         |                    |
  //                         \                    /
  // NOT_STARTED -> STARTED --> WAITING_FOR_WRITE -> WRITING_DONE -> SUCCESS
  State write_state_;

  bool write_end_of_stream_;

  CronetEnvironment* const environment_;

  scoped_refptr<net::WrappedIOBuffer> read_buffer_;
  scoped_refptr<net::WrappedIOBuffer> write_buffer_;
  std::unique_ptr<net::BidirectionalStream> bidi_stream_;
  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(CronetBidirectionalStream);
};

}  // namespace cronet

#endif  // COMPONENTS_CRONET_IOS_CRONET_BIDIRECTIONAL_STREAM_H_
