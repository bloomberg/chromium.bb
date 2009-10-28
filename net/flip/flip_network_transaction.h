// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FLIP_NETWORK_TRANSACTION_H_
#define NET_FLIP_NETWORK_TRANSACTION_H_

#include <string>
#include <deque>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "net/base/address_list.h"
#include "net/base/host_resolver.h"
#include "net/base/io_buffer.h"
#include "net/base/load_flags.h"
#include "net/base/load_states.h"
#include "net/base/ssl_config_service.h"
#include "net/flip/flip_session.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_handler.h"
#include "net/http/http_response_info.h"
#include "net/http/http_transaction.h"
#include "net/proxy/proxy_service.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace net {

class ClientSocketFactory;
class HttpNetworkSession;
class UploadDataStream;

// A FlipNetworkTransaction can be used to fetch HTTP conent.
// The FlipDelegate is the consumer of events from the FlipSession.
class FlipNetworkTransaction : public HttpTransaction, public FlipDelegate {
 public:
  explicit FlipNetworkTransaction(HttpNetworkSession* session);
  virtual ~FlipNetworkTransaction();

  // FlipDelegate methods:
  virtual const HttpRequestInfo* request();
  virtual const UploadDataStream* data();
  virtual void OnRequestSent(int status);
  virtual void OnUploadDataSent(int result);
  virtual void OnResponseReceived(HttpResponseInfo* response);
  virtual void OnDataReceived(const char* buffer, int bytes);
  virtual void OnClose(int status);

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

  // Used to callback an HttpTransaction call.
  void DoHttpTransactionCallback(int result);

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

  // The Flip request id for this request.
  int flip_request_id_;

  // The Flip session servicing this request.  If NULL, the request has not
  // started.
  scoped_refptr<FlipSession> flip_;

  CompletionCallback* user_callback_;
  scoped_refptr<IOBuffer> user_buffer_;
  int user_buffer_bytes_remaining_;

  scoped_refptr<HttpNetworkSession> session_;

  const HttpRequestInfo* request_;
  HttpResponseInfo response_;

  scoped_ptr<UploadDataStream> request_body_stream_;

  // We buffer the response body as it arrives asynchronously from the stream.
  // TODO(mbelshe):  is this infinite buffering?
  std::deque<scoped_refptr<IOBufferWithSize> > response_body_;
  bool response_complete_;
  // Since we buffer the response, we also buffer the response status.
  // Not valid until response_complete_ is true.
  int response_status_;

  // The time the Start method was called.
  base::Time start_time_;

  // The next state in the state machine.
  State next_state_;
};

}  // namespace net

#endif  // NET_HTTP_NETWORK_TRANSACTION_H_

