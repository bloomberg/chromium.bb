// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FLIP_NETWORK_TRANSACTION_H_
#define NET_FLIP_NETWORK_TRANSACTION_H_

#include <string>
#include <deque>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "net/base/io_buffer.h"
#include "net/base/load_states.h"
#include "net/flip/flip_session.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"

namespace net {

class FlipSession;
class HttpNetworkSession;
class UploadDataStream;

// FlipStreamParser is a class to encapsulate the IO of a FLIP
// stream on top of a FlipSession.  All read/writes go through
// the FlipStreamParser.
class FlipStreamParser : public FlipDelegate {
 public:
  FlipStreamParser();
  ~FlipStreamParser();

  // Creates a FLIP stream from |flip| and send the HTTP request over it.
  // |request|'s lifetime must persist longer than |this|.  This always
  // completes asynchronously, so |callback| must be non-NULL.  Returns a net
  // error code.
  int SendRequest(FlipSession* flip, const HttpRequestInfo* request,
                  CompletionCallback* callback);

  // Reads the response headers.  Returns a net error code.
  int ReadResponseHeaders(CompletionCallback* callback);

  // Reads the response body.  Returns a net error code or the number of bytes
  // read.
  int ReadResponseBody(
      IOBuffer* buf, int buf_len, CompletionCallback* callback);

  // Returns the number of bytes uploaded.
  uint64 GetUploadProgress() const;

  const HttpResponseInfo* GetResponseInfo() const;

  // FlipDelegate methods:
  virtual const HttpRequestInfo* request() const;
  virtual const UploadDataStream* data() const;
  virtual void OnRequestSent(int status);
  virtual void OnUploadDataSent(int result);
  virtual void OnResponseReceived(HttpResponseInfo* response);
  virtual void OnDataReceived(const char* buffer, int bytes);
  virtual void OnClose(int status);

 private:
  friend class FlipStreamParserPeer;

  void DoCallback(int rv);

  // The Flip request id for this request.
  scoped_refptr<FlipSession> flip_;
  flip::FlipStreamId flip_stream_id_;

  const HttpRequestInfo* request_;
  scoped_ptr<HttpResponseInfo> response_;
  scoped_ptr<UploadDataStream> request_body_stream_;

  bool response_complete_;
  // We buffer the response body as it arrives asynchronously from the stream.
  // TODO(mbelshe):  is this infinite buffering?
  std::deque<scoped_refptr<IOBufferWithSize> > response_body_;

  // Since we buffer the response, we also buffer the response status.
  // Not valid until response_complete_ is true.
  int response_status_;

  CompletionCallback* user_callback_;

  // User provided buffer for the ReadResponseBody() response.
  scoped_refptr<IOBuffer> user_buffer_;
  int user_buffer_len_;

  DISALLOW_COPY_AND_ASSIGN(FlipStreamParser);
};

// A FlipNetworkTransaction can be used to fetch HTTP conent.
// The FlipDelegate is the consumer of events from the FlipSession.
class FlipNetworkTransaction : public HttpTransaction {
 public:
  explicit FlipNetworkTransaction(HttpNetworkSession* session);
  virtual ~FlipNetworkTransaction();

  // HttpTransaction methods:
  virtual int Start(const HttpRequestInfo* request_info,
                    CompletionCallback* callback,
                    LoadLog* load_log);
  virtual int RestartIgnoringLastError(CompletionCallback* callback);
  virtual int RestartWithCertificate(X509Certificate* client_cert,
                                     CompletionCallback* callback);
  virtual int RestartWithAuth(const std::wstring& username,
                              const std::wstring& password,
                              CompletionCallback* callback);
  virtual bool IsReadyToRestartForAuth() { return false; }
  virtual int Read(IOBuffer* buf, int buf_len, CompletionCallback* callback);
  virtual const HttpResponseInfo* GetResponseInfo() const;
  virtual LoadState GetLoadState() const;
  virtual uint64 GetUploadProgress() const;

 protected:
  friend class FlipNetworkTransactionTest;

  // Provide access to the session for testing.
  FlipSession* GetFlipSession() { return flip_.get(); }

 private:
  enum State {
    STATE_INIT_CONNECTION,
    STATE_INIT_CONNECTION_COMPLETE,
    STATE_SEND_REQUEST,
    STATE_SEND_REQUEST_COMPLETE,
    STATE_READ_HEADERS,
    STATE_READ_HEADERS_COMPLETE,
    STATE_READ_BODY,
    STATE_READ_BODY_COMPLETE,
    STATE_NONE
  };

  void DoCallback(int result);
  void OnIOComplete(int result);

  // Runs the state transition loop.
  int DoLoop(int result);

  // Each of these methods corresponds to a State value.  Those with an input
  // argument receive the result from the previous state.  If a method returns
  // ERR_IO_PENDING, then the result from OnIOComplete will be passed to the
  // next state method as the result arg.
  int DoInitConnection();
  int DoInitConnectionComplete(int result);
  int DoSendRequest();
  int DoSendRequestComplete(int result);
  int DoReadHeaders();
  int DoReadHeadersComplete(int result);
  int DoReadBody();
  int DoReadBodyComplete(int result);

  // The Flip session servicing this request.  If NULL, the request has not
  // started.
  scoped_refptr<FlipSession> flip_;

  CompletionCallbackImpl<FlipNetworkTransaction> io_callback_;
  CompletionCallback* user_callback_;

  // Used to pass onto the FlipStreamParser.
  scoped_refptr<IOBuffer> user_buffer_;
  int user_buffer_len_;

  scoped_refptr<HttpNetworkSession> session_;

  const HttpRequestInfo* request_;

  // The time the Start method was called.
  base::TimeTicks start_time_;

  // The next state in the state machine.
  State next_state_;

  scoped_ptr<FlipStreamParser> flip_stream_parser_;

  DISALLOW_COPY_AND_ASSIGN(FlipNetworkTransaction);
};

}  // namespace net

#endif  // NET_HTTP_NETWORK_TRANSACTION_H_
