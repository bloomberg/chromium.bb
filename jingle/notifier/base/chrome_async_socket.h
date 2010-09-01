// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// An implementation of buzz::AsyncSocket that uses Chrome sockets.

#ifndef JINGLE_NOTIFIER_BASE_CHROME_ASYNC_SOCKET_H_
#define JINGLE_NOTIFIER_BASE_CHROME_ASYNC_SOCKET_H_

#if !defined(FEATURE_ENABLE_SSL)
#error ChromeAsyncSocket expects FEATURE_ENABLE_SSL to be defined
#endif

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/ssl_config_service.h"
#include "talk/xmpp/asyncsocket.h"

namespace net {
class ClientSocket;
class ClientSocketFactory;
class IOBufferWithSize;
}  // namespace net

namespace notifier {

class ChromeAsyncSocket : public buzz::AsyncSocket {
 public:
  // Takes ownership of |client_socket_factory| but not |net_log|.
  // |net_log| may be NULL.
  ChromeAsyncSocket(net::ClientSocketFactory* client_socket_factory,
                    const net::SSLConfig& ssl_config,
                    size_t read_buf_size,
                    size_t write_buf_size,
                    net::NetLog* net_log);

  // Does not raise any signals.
  virtual ~ChromeAsyncSocket();

  // buzz::AsyncSocket implementation.

  // The current state (see buzz::AsyncSocket::State; all but
  // STATE_CLOSING is used).
  virtual State state();

  // The last generated error.  Errors are generated when the main
  // functions below return false or when SignalClosed is raised due
  // to an asynchronous error.
  virtual Error error();

  // GetError() (which is of type net::Error) != net::OK only when
  // error() == ERROR_WINSOCK.
  virtual int GetError();

  // Tries to connect to the given address.
  //
  // If state() is not STATE_CLOSED, sets error to ERROR_WRONGSTATE
  // and returns false.
  //
  // If |address| is not resolved, sets error to ERROR_DNS and returns
  // false.
  //
  // Otherwise, starts the connection process and returns true.
  // SignalConnected will be raised when the connection is successful;
  // otherwise, SignalClosed will be raised with a net error set.
  virtual bool Connect(const talk_base::SocketAddress& address);

  // Tries to read at most |len| bytes into |data|.
  //
  // If state() is not STATE_TLS_CONNECTING, STATE_OPEN, or
  // STATE_TLS_OPEN, sets error to ERROR_WRONGSTATE and returns false.
  //
  // Otherwise, fills in |len_read| with the number of bytes read and
  // returns true.  If this is called when state() is
  // STATE_TLS_CONNECTING, reads 0 bytes.  (We have to handle this
  // case because StartTls() is called during a slot connected to
  // SignalRead after parsing the final non-TLS reply from the server
  // [see XmppClient::Private::OnSocketRead()].)
  virtual bool Read(char* data, size_t len, size_t* len_read);

  // Queues up |len| bytes of |data| for writing.
  //
  // If state() is not STATE_TLS_CONNECTING, STATE_OPEN, or
  // STATE_TLS_OPEN, sets error to ERROR_WRONGSTATE and returns false.
  //
  // If the given data is too big for the internal write buffer, sets
  // error to ERROR_WINSOCK/net::ERR_INSUFFICIENT_RESOURCES and
  // returns false.
  //
  // Otherwise, queues up the data and returns true.  If this is
  // called when state() == STATE_TLS_CONNECTING, the data is will be
  // sent only after the TLS connection succeeds.  (See StartTls()
  // below for why this happens.)
  //
  // Note that there's no guarantee that the data will actually be
  // sent; however, it is guaranteed that the any data sent will be
  // sent in FIFO order.
  virtual bool Write(const char* data, size_t len);

  // If the socket is not already closed, closes the socket and raises
  // SignalClosed.  Always returns true.
  virtual bool Close();

  // Tries to change to a TLS connection with the given domain name.
  //
  // If state() is not STATE_OPEN or there are pending reads or
  // writes, sets error to ERROR_WRONGSTATE and returns false.  (In
  // practice, this means that StartTls() can only be called from a
  // slot connected to SignalRead.)
  //
  // Otherwise, starts the TLS connection process and returns true.
  // SignalSSLConnected will be raised when the connection is
  // successful; otherwise, SignalClosed will be raised with a net
  // error set.
  virtual bool StartTls(const std::string& domain_name);

  // Signal behavior:
  //
  // SignalConnected: raised whenever the connect initiated by a call
  // to Connect() is complete.
  //
  // SignalSSLConnected: raised whenever the connect initiated by a
  // call to StartTls() is complete.  Not actually used by
  // XmppClient. (It just assumes that if SignalRead is raised after a
  // call to StartTls(), the connection has been successfully
  // upgraded.)
  //
  // SignalClosed: raised whenever the socket is closed, either due to
  // an asynchronous error, the other side closing the connection, or
  // when Close() is called.
  //
  // SignalRead: raised whenever the next call to Read() will succeed
  // with a non-zero |len_read| (assuming nothing else happens in the
  // meantime).
  //
  // SignalError: not used.

 private:
  enum AsyncIOState {
    // An I/O op is not in progress.
    IDLE,
    // A function has been posted to do the I/O.
    POSTED,
    // An async I/O operation is pending.
    PENDING,
  };

  bool IsOpen() const;

  // Error functions.
  void DoNonNetError(Error error);
  void DoNetError(net::Error net_error);
  void DoNetErrorFromStatus(int status);

  // Connection functions.
  void ProcessConnectDone(int status);

  // Read loop functions.
  void PostDoRead();
  void DoRead();
  void ProcessReadDone(int status);

  // Write loop functions.
  void PostDoWrite();
  void DoWrite();
  void ProcessWriteDone(int status);

  // SSL/TLS connection functions.
  void ProcessSSLConnectDone(int status);

  // Close functions.
  void DoClose();

  // Callbacks passed to |transport_socket_|.
  net::CompletionCallbackImpl<ChromeAsyncSocket> connect_callback_;
  net::CompletionCallbackImpl<ChromeAsyncSocket> read_callback_;
  net::CompletionCallbackImpl<ChromeAsyncSocket> write_callback_;
  net::CompletionCallbackImpl<ChromeAsyncSocket> ssl_connect_callback_;

  scoped_ptr<net::ClientSocketFactory> client_socket_factory_;
  const net::SSLConfig ssl_config_;
  net::BoundNetLog bound_net_log_;

  // buzz::AsyncSocket state.
  buzz::AsyncSocket::State state_;
  buzz::AsyncSocket::Error error_;
  net::Error net_error_;

  // Used by read/write loops.
  ScopedRunnableMethodFactory<ChromeAsyncSocket>
      scoped_runnable_method_factory_;

  // NULL iff state() == STATE_CLOSED.
  //
  // TODO(akalin): Use ClientSocketPool.
  scoped_ptr<net::ClientSocket> transport_socket_;

  // State for the read loop.  |read_start_| <= |read_end_| <=
  // |read_buf_->size()|.  There's a read in flight (i.e.,
  // |read_state_| != IDLE) iff |read_end_| == 0.
  AsyncIOState read_state_;
  scoped_refptr<net::IOBufferWithSize> read_buf_;
  size_t read_start_, read_end_;

  // State for the write loop.  |write_end_| <= |write_buf_->size()|.
  // There's a write in flight (i.e., |write_state_| != IDLE) iff
  // |write_end_| > 0.
  AsyncIOState write_state_;
  scoped_refptr<net::IOBufferWithSize> write_buf_;
  size_t write_end_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAsyncSocket);
};

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_BASE_CHROME_ASYNC_SOCKET_H_
