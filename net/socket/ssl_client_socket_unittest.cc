// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/ssl_client_socket.h"

#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/test_completion_callback.h"
#include "net/base/test_data_directory.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/cert/test_root_certs.h"
#include "net/dns/host_resolver.h"
#include "net/http/transport_security_state.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/tcp_client_socket.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config_service.h"
#include "net/test/cert_test_util.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace net {

namespace {

const SSLConfig kDefaultSSLConfig;

// WrappedStreamSocket is a base class that wraps an existing StreamSocket,
// forwarding the Socket and StreamSocket interfaces to the underlying
// transport.
// This is to provide a common base class for subclasses to override specific
// StreamSocket methods for testing, while still communicating with a 'real'
// StreamSocket.
class WrappedStreamSocket : public StreamSocket {
 public:
  explicit WrappedStreamSocket(scoped_ptr<StreamSocket> transport)
      : transport_(transport.Pass()) {}
  virtual ~WrappedStreamSocket() {}

  // StreamSocket implementation:
  virtual int Connect(const CompletionCallback& callback) OVERRIDE {
    return transport_->Connect(callback);
  }
  virtual void Disconnect() OVERRIDE { transport_->Disconnect(); }
  virtual bool IsConnected() const OVERRIDE {
    return transport_->IsConnected();
  }
  virtual bool IsConnectedAndIdle() const OVERRIDE {
    return transport_->IsConnectedAndIdle();
  }
  virtual int GetPeerAddress(IPEndPoint* address) const OVERRIDE {
    return transport_->GetPeerAddress(address);
  }
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE {
    return transport_->GetLocalAddress(address);
  }
  virtual const BoundNetLog& NetLog() const OVERRIDE {
    return transport_->NetLog();
  }
  virtual void SetSubresourceSpeculation() OVERRIDE {
    transport_->SetSubresourceSpeculation();
  }
  virtual void SetOmniboxSpeculation() OVERRIDE {
    transport_->SetOmniboxSpeculation();
  }
  virtual bool WasEverUsed() const OVERRIDE {
    return transport_->WasEverUsed();
  }
  virtual bool UsingTCPFastOpen() const OVERRIDE {
    return transport_->UsingTCPFastOpen();
  }
  virtual bool WasNpnNegotiated() const OVERRIDE {
    return transport_->WasNpnNegotiated();
  }
  virtual NextProto GetNegotiatedProtocol() const OVERRIDE {
    return transport_->GetNegotiatedProtocol();
  }
  virtual bool GetSSLInfo(SSLInfo* ssl_info) OVERRIDE {
    return transport_->GetSSLInfo(ssl_info);
  }

  // Socket implementation:
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   const CompletionCallback& callback) OVERRIDE {
    return transport_->Read(buf, buf_len, callback);
  }
  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    const CompletionCallback& callback) OVERRIDE {
    return transport_->Write(buf, buf_len, callback);
  }
  virtual int SetReceiveBufferSize(int32 size) OVERRIDE {
    return transport_->SetReceiveBufferSize(size);
  }
  virtual int SetSendBufferSize(int32 size) OVERRIDE {
    return transport_->SetSendBufferSize(size);
  }

 protected:
  scoped_ptr<StreamSocket> transport_;
};

// ReadBufferingStreamSocket is a wrapper for an existing StreamSocket that
// will ensure a certain amount of data is internally buffered before
// satisfying a Read() request. It exists to mimic OS-level internal
// buffering, but in a way to guarantee that X number of bytes will be
// returned to callers of Read(), regardless of how quickly the OS receives
// them from the TestServer.
class ReadBufferingStreamSocket : public WrappedStreamSocket {
 public:
  explicit ReadBufferingStreamSocket(scoped_ptr<StreamSocket> transport);
  virtual ~ReadBufferingStreamSocket() {}

  // Socket implementation:
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   const CompletionCallback& callback) OVERRIDE;

  // Sets the internal buffer to |size|. This must not be greater than
  // the largest value supplied to Read() - that is, it does not handle
  // having "leftovers" at the end of Read().
  // Each call to Read() will be prevented from completion until at least
  // |size| data has been read.
  // Set to 0 to turn off buffering, causing Read() to transparently
  // read via the underlying transport.
  void SetBufferSize(int size);

 private:
  enum State {
    STATE_NONE,
    STATE_READ,
    STATE_READ_COMPLETE,
  };

  int DoLoop(int result);
  int DoRead();
  int DoReadComplete(int result);
  void OnReadCompleted(int result);

  State state_;
  scoped_refptr<GrowableIOBuffer> read_buffer_;
  int buffer_size_;

  scoped_refptr<IOBuffer> user_read_buf_;
  CompletionCallback user_read_callback_;
};

ReadBufferingStreamSocket::ReadBufferingStreamSocket(
    scoped_ptr<StreamSocket> transport)
    : WrappedStreamSocket(transport.Pass()),
      read_buffer_(new GrowableIOBuffer()),
      buffer_size_(0) {}

void ReadBufferingStreamSocket::SetBufferSize(int size) {
  DCHECK(!user_read_buf_.get());
  buffer_size_ = size;
  read_buffer_->SetCapacity(size);
}

int ReadBufferingStreamSocket::Read(IOBuffer* buf,
                                    int buf_len,
                                    const CompletionCallback& callback) {
  if (buffer_size_ == 0)
    return transport_->Read(buf, buf_len, callback);

  if (buf_len < buffer_size_)
    return ERR_UNEXPECTED;

  state_ = STATE_READ;
  user_read_buf_ = buf;
  int result = DoLoop(OK);
  if (result == ERR_IO_PENDING)
    user_read_callback_ = callback;
  else
    user_read_buf_ = NULL;
  return result;
}

int ReadBufferingStreamSocket::DoLoop(int result) {
  int rv = result;
  do {
    State current_state = state_;
    state_ = STATE_NONE;
    switch (current_state) {
      case STATE_READ:
        rv = DoRead();
        break;
      case STATE_READ_COMPLETE:
        rv = DoReadComplete(rv);
        break;
      case STATE_NONE:
      default:
        NOTREACHED() << "Unexpected state: " << current_state;
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && state_ != STATE_NONE);
  return rv;
}

int ReadBufferingStreamSocket::DoRead() {
  state_ = STATE_READ_COMPLETE;
  int rv =
      transport_->Read(read_buffer_.get(),
                       read_buffer_->RemainingCapacity(),
                       base::Bind(&ReadBufferingStreamSocket::OnReadCompleted,
                                  base::Unretained(this)));
  return rv;
}

int ReadBufferingStreamSocket::DoReadComplete(int result) {
  state_ = STATE_NONE;
  if (result <= 0)
    return result;

  read_buffer_->set_offset(read_buffer_->offset() + result);
  if (read_buffer_->RemainingCapacity() > 0) {
    state_ = STATE_READ;
    return OK;
  }

  memcpy(user_read_buf_->data(),
         read_buffer_->StartOfBuffer(),
         read_buffer_->capacity());
  read_buffer_->set_offset(0);
  return read_buffer_->capacity();
}

void ReadBufferingStreamSocket::OnReadCompleted(int result) {
  result = DoLoop(result);
  if (result == ERR_IO_PENDING)
    return;

  user_read_buf_ = NULL;
  base::ResetAndReturn(&user_read_callback_).Run(result);
}

// Simulates synchronously receiving an error during Read() or Write()
class SynchronousErrorStreamSocket : public WrappedStreamSocket {
 public:
  explicit SynchronousErrorStreamSocket(scoped_ptr<StreamSocket> transport);
  virtual ~SynchronousErrorStreamSocket() {}

  // Socket implementation:
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   const CompletionCallback& callback) OVERRIDE;
  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    const CompletionCallback& callback) OVERRIDE;

  // Sets the next Read() call and all future calls to return |error|.
  // If there is already a pending asynchronous read, the configured error
  // will not be returned until that asynchronous read has completed and Read()
  // is called again.
  void SetNextReadError(Error error) {
    DCHECK_GE(0, error);
    have_read_error_ = true;
    pending_read_error_ = error;
  }

  // Sets the next Write() call and all future calls to return |error|.
  // If there is already a pending asynchronous write, the configured error
  // will not be returned until that asynchronous write has completed and
  // Write() is called again.
  void SetNextWriteError(Error error) {
    DCHECK_GE(0, error);
    have_write_error_ = true;
    pending_write_error_ = error;
  }

 private:
  bool have_read_error_;
  int pending_read_error_;

  bool have_write_error_;
  int pending_write_error_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousErrorStreamSocket);
};

SynchronousErrorStreamSocket::SynchronousErrorStreamSocket(
    scoped_ptr<StreamSocket> transport)
    : WrappedStreamSocket(transport.Pass()),
      have_read_error_(false),
      pending_read_error_(OK),
      have_write_error_(false),
      pending_write_error_(OK) {}

int SynchronousErrorStreamSocket::Read(IOBuffer* buf,
                                       int buf_len,
                                       const CompletionCallback& callback) {
  if (have_read_error_)
    return pending_read_error_;
  return transport_->Read(buf, buf_len, callback);
}

int SynchronousErrorStreamSocket::Write(IOBuffer* buf,
                                        int buf_len,
                                        const CompletionCallback& callback) {
  if (have_write_error_)
    return pending_write_error_;
  return transport_->Write(buf, buf_len, callback);
}

// FakeBlockingStreamSocket wraps an existing StreamSocket and simulates the
// underlying transport needing to complete things asynchronously in a
// deterministic manner (e.g.: independent of the TestServer and the OS's
// semantics).
class FakeBlockingStreamSocket : public WrappedStreamSocket {
 public:
  explicit FakeBlockingStreamSocket(scoped_ptr<StreamSocket> transport);
  virtual ~FakeBlockingStreamSocket() {}

  // Socket implementation:
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   const CompletionCallback& callback) OVERRIDE {
    return read_state_.RunWrappedFunction(buf, buf_len, callback);
  }
  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    const CompletionCallback& callback) OVERRIDE {
    return write_state_.RunWrappedFunction(buf, buf_len, callback);
  }

  // Causes the next call to Read() to return ERR_IO_PENDING, not completing
  // (invoking the callback) until UnblockRead() has been called and the
  // underlying transport has completed.
  void SetNextReadShouldBlock() { read_state_.SetShouldBlock(); }
  void UnblockRead() { read_state_.Unblock(); }

  // Causes the next call to Write() to return ERR_IO_PENDING, not completing
  // (invoking the callback) until UnblockWrite() has been called and the
  // underlying transport has completed.
  void SetNextWriteShouldBlock() { write_state_.SetShouldBlock(); }
  void UnblockWrite() { write_state_.Unblock(); }

 private:
  // Tracks the state for simulating a blocking Read/Write operation.
  class BlockingState {
   public:
    // Wrapper for the underlying Socket function to call (ie: Read/Write).
    typedef base::Callback<int(IOBuffer*, int, const CompletionCallback&)>
        WrappedSocketFunction;

    explicit BlockingState(const WrappedSocketFunction& function);
    ~BlockingState() {}

    // Sets the next call to RunWrappedFunction() to block, returning
    // ERR_IO_PENDING and not invoking the user callback until Unblock() is
    // called.
    void SetShouldBlock();

    // Unblocks the currently blocked pending function, invoking the user
    // callback if the results are immediately available.
    // Note: It's not valid to call this unless SetShouldBlock() has been
    // called beforehand.
    void Unblock();

    // Performs the wrapped socket function on the underlying transport. If
    // configured to block via SetShouldBlock(), then |user_callback| will not
    // be invoked until Unblock() has been called.
    int RunWrappedFunction(IOBuffer* buf,
                           int len,
                           const CompletionCallback& user_callback);

   private:
    // Handles completion from the underlying wrapped socket function.
    void OnCompleted(int result);

    WrappedSocketFunction wrapped_function_;
    bool should_block_;
    bool have_result_;
    int pending_result_;
    CompletionCallback user_callback_;
  };

  BlockingState read_state_;
  BlockingState write_state_;

  DISALLOW_COPY_AND_ASSIGN(FakeBlockingStreamSocket);
};

FakeBlockingStreamSocket::FakeBlockingStreamSocket(
    scoped_ptr<StreamSocket> transport)
    : WrappedStreamSocket(transport.Pass()),
      read_state_(base::Bind(&Socket::Read,
                             base::Unretained(transport_.get()))),
      write_state_(base::Bind(&Socket::Write,
                              base::Unretained(transport_.get()))) {}

FakeBlockingStreamSocket::BlockingState::BlockingState(
    const WrappedSocketFunction& function)
    : wrapped_function_(function),
      should_block_(false),
      have_result_(false),
      pending_result_(OK) {}

void FakeBlockingStreamSocket::BlockingState::SetShouldBlock() {
  DCHECK(!should_block_);
  should_block_ = true;
}

void FakeBlockingStreamSocket::BlockingState::Unblock() {
  DCHECK(should_block_);
  should_block_ = false;

  // If the operation is still pending in the underlying transport, immediately
  // return - OnCompleted() will handle invoking the callback once the transport
  // has completed.
  if (!have_result_)
    return;

  have_result_ = false;

  base::ResetAndReturn(&user_callback_).Run(pending_result_);
}

int FakeBlockingStreamSocket::BlockingState::RunWrappedFunction(
    IOBuffer* buf,
    int len,
    const CompletionCallback& callback) {

  // The callback to be called by the underlying transport. Either forward
  // directly to the user's callback if not set to block, or intercept it with
  // OnCompleted so that the user's callback is not invoked until Unblock() is
  // called.
  CompletionCallback transport_callback =
      !should_block_ ? callback : base::Bind(&BlockingState::OnCompleted,
                                             base::Unretained(this));
  int rv = wrapped_function_.Run(buf, len, transport_callback);
  if (should_block_) {
    user_callback_ = callback;
    // May have completed synchronously.
    have_result_ = (rv != ERR_IO_PENDING);
    pending_result_ = rv;
    return ERR_IO_PENDING;
  }

  return rv;
}

void FakeBlockingStreamSocket::BlockingState::OnCompleted(int result) {
  if (should_block_) {
    // Store the result so that the callback can be invoked once Unblock() is
    // called.
    have_result_ = true;
    pending_result_ = result;
    return;
  }

  // Otherwise, the Unblock() function was called before the underlying
  // transport completed, so run the user's callback immediately.
  base::ResetAndReturn(&user_callback_).Run(result);
}

// CompletionCallback that will delete the associated StreamSocket when
// the callback is invoked.
class DeleteSocketCallback : public TestCompletionCallbackBase {
 public:
  explicit DeleteSocketCallback(StreamSocket* socket)
      : socket_(socket),
        callback_(base::Bind(&DeleteSocketCallback::OnComplete,
                             base::Unretained(this))) {}
  virtual ~DeleteSocketCallback() {}

  const CompletionCallback& callback() const { return callback_; }

 private:
  void OnComplete(int result) {
    if (socket_) {
      delete socket_;
      socket_ = NULL;
    } else {
      ADD_FAILURE() << "Deleting socket twice";
    }
    SetResult(result);
  }

  StreamSocket* socket_;
  CompletionCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(DeleteSocketCallback);
};

class SSLClientSocketTest : public PlatformTest {
 public:
  SSLClientSocketTest()
      : socket_factory_(ClientSocketFactory::GetDefaultFactory()),
        cert_verifier_(new MockCertVerifier),
        transport_security_state_(new TransportSecurityState) {
    cert_verifier_->set_default_result(OK);
    context_.cert_verifier = cert_verifier_.get();
    context_.transport_security_state = transport_security_state_.get();
  }

 protected:
  scoped_ptr<SSLClientSocket> CreateSSLClientSocket(
      scoped_ptr<StreamSocket> transport_socket,
      const HostPortPair& host_and_port,
      const SSLConfig& ssl_config) {
    scoped_ptr<ClientSocketHandle> connection(new ClientSocketHandle);
    connection->SetSocket(transport_socket.Pass());
    return socket_factory_->CreateSSLClientSocket(
        connection.Pass(), host_and_port, ssl_config, context_);
  }

  ClientSocketFactory* socket_factory_;
  scoped_ptr<MockCertVerifier> cert_verifier_;
  scoped_ptr<TransportSecurityState> transport_security_state_;
  SSLClientSocketContext context_;
};

// Verifies the correctness of GetSSLCertRequestInfo.
class SSLClientSocketCertRequestInfoTest : public SSLClientSocketTest {
 protected:
  // Creates a test server with the given SSLOptions, connects to it and returns
  // the SSLCertRequestInfo reported by the socket.
  scoped_refptr<SSLCertRequestInfo> GetCertRequest(
      SpawnedTestServer::SSLOptions ssl_options) {
    SpawnedTestServer test_server(
        SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath());
    if (!test_server.Start())
      return NULL;

    AddressList addr;
    if (!test_server.GetAddressList(&addr))
      return NULL;

    TestCompletionCallback callback;
    CapturingNetLog log;
    scoped_ptr<StreamSocket> transport(
        new TCPClientSocket(addr, &log, NetLog::Source()));
    int rv = transport->Connect(callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    EXPECT_EQ(OK, rv);

    scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
        transport.Pass(), test_server.host_port_pair(), kDefaultSSLConfig));
    EXPECT_FALSE(sock->IsConnected());

    rv = sock->Connect(callback.callback());
    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();
    scoped_refptr<SSLCertRequestInfo> request_info = new SSLCertRequestInfo();
    sock->GetSSLCertRequestInfo(request_info.get());
    sock->Disconnect();
    EXPECT_FALSE(sock->IsConnected());

    return request_info;
  }
};

//-----------------------------------------------------------------------------

// LogContainsSSLConnectEndEvent returns true if the given index in the given
// log is an SSL connect end event. The NSS sockets will cork in an attempt to
// merge the first application data record with the Finished message when false
// starting. However, in order to avoid the server timing out the handshake,
// they'll give up waiting for application data and send the Finished after a
// timeout. This means that an SSL connect end event may appear as a socket
// write.
static bool LogContainsSSLConnectEndEvent(
    const CapturingNetLog::CapturedEntryList& log,
    int i) {
  return LogContainsEndEvent(log, i, NetLog::TYPE_SSL_CONNECT) ||
         LogContainsEvent(
             log, i, NetLog::TYPE_SOCKET_BYTES_SENT, NetLog::PHASE_NONE);
}

}  // namespace

TEST_F(SSLClientSocketTest, Connect) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  CapturingNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());

  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

TEST_F(SSLClientSocketTest, ConnectExpired) {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_EXPIRED);
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath());
  ASSERT_TRUE(test_server.Start());

  cert_verifier_->set_default_result(ERR_CERT_DATE_INVALID);

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  CapturingNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());

  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(ERR_CERT_DATE_INVALID, rv);

  // Rather than testing whether or not the underlying socket is connected,
  // test that the handshake has finished. This is because it may be
  // desirable to disconnect the socket before showing a user prompt, since
  // the user may take indefinitely long to respond.
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));
}

TEST_F(SSLClientSocketTest, ConnectMismatched) {
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_MISMATCHED_NAME);
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath());
  ASSERT_TRUE(test_server.Start());

  cert_verifier_->set_default_result(ERR_CERT_COMMON_NAME_INVALID);

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  CapturingNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());

  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(ERR_CERT_COMMON_NAME_INVALID, rv);

  // Rather than testing whether or not the underlying socket is connected,
  // test that the handshake has finished. This is because it may be
  // desirable to disconnect the socket before showing a user prompt, since
  // the user may take indefinitely long to respond.
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));
}

// Attempt to connect to a page which requests a client certificate. It should
// return an error code on connect.
TEST_F(SSLClientSocketTest, ConnectClientAuthCertRequested) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  CapturingNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), kDefaultSSLConfig));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());

  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();

  log.GetEntries(&entries);
  // Because we prematurely kill the handshake at CertificateRequest,
  // the server may still send data (notably the ServerHelloDone)
  // after the error is returned. As a result, the SSL_CONNECT may not
  // be the last entry. See http://crbug.com/54445. We use
  // ExpectLogContainsSomewhere instead of
  // LogContainsSSLConnectEndEvent to avoid assuming, e.g., only one
  // extra read instead of two. This occurs before the handshake ends,
  // so the corking logic of LogContainsSSLConnectEndEvent isn't
  // necessary.
  //
  // TODO(davidben): When SSL_RestartHandshakeAfterCertReq in NSS is
  // fixed and we can respond to the first CertificateRequest
  // without closing the socket, add a unit test for sending the
  // certificate. This test may still be useful as we'll want to close
  // the socket on a timeout if the user takes a long time to pick a
  // cert. Related bug: https://bugzilla.mozilla.org/show_bug.cgi?id=542832
  ExpectLogContainsSomewhere(
      entries, 0, NetLog::TYPE_SSL_CONNECT, NetLog::PHASE_END);
  EXPECT_EQ(ERR_SSL_CLIENT_AUTH_CERT_NEEDED, rv);
  EXPECT_FALSE(sock->IsConnected());
}

// Connect to a server requesting optional client authentication. Send it a
// null certificate. It should allow the connection.
//
// TODO(davidben): Also test providing an actual certificate.
TEST_F(SSLClientSocketTest, ConnectClientAuthSendNullCert) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  CapturingNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  SSLConfig ssl_config = kDefaultSSLConfig;
  ssl_config.send_client_cert = true;
  ssl_config.client_cert = NULL;

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), ssl_config));

  EXPECT_FALSE(sock->IsConnected());

  // Our test server accepts certificate-less connections.
  // TODO(davidben): Add a test which requires them and verify the error.
  rv = sock->Connect(callback.callback());

  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));

  // We responded to the server's certificate request with a Certificate
  // message with no client certificate in it.  ssl_info.client_cert_sent
  // should be false in this case.
  SSLInfo ssl_info;
  sock->GetSSLInfo(&ssl_info);
  EXPECT_FALSE(ssl_info.client_cert_sent);

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

// TODO(wtc): Add unit tests for IsConnectedAndIdle:
//   - Server closes an SSL connection (with a close_notify alert message).
//   - Server closes the underlying TCP connection directly.
//   - Server sends data unexpectedly.

TEST_F(SSLClientSocketTest, Read) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), kDefaultSSLConfig));

  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<IOBuffer> request_buffer(
      new IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(
      request_buffer.get(), arraysize(request_text) - 1, callback.callback());
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));
  for (;;) {
    rv = sock->Read(buf.get(), 4096, callback.callback());
    EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
}

// Tests that the SSLClientSocket properly handles when the underlying transport
// synchronously returns an error code - such as if an intermediary terminates
// the socket connection uncleanly.
// This is a regression test for http://crbug.com/238536
TEST_F(SSLClientSocketTest, Read_WithSynchronousError) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> real_transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  scoped_ptr<SynchronousErrorStreamSocket> transport(
      new SynchronousErrorStreamSocket(real_transport.Pass()));
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  // Disable TLS False Start to avoid handshake non-determinism.
  SSLConfig ssl_config;
  ssl_config.false_start_enabled = false;

  SynchronousErrorStreamSocket* raw_transport = transport.get();
  scoped_ptr<SSLClientSocket> sock(
      CreateSSLClientSocket(transport.PassAs<StreamSocket>(),
                            test_server.host_port_pair(),
                            ssl_config));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  static const int kRequestTextSize =
      static_cast<int>(arraysize(request_text) - 1);
  scoped_refptr<IOBuffer> request_buffer(new IOBuffer(kRequestTextSize));
  memcpy(request_buffer->data(), request_text, kRequestTextSize);

  rv = callback.GetResult(
      sock->Write(request_buffer.get(), kRequestTextSize, callback.callback()));
  EXPECT_EQ(kRequestTextSize, rv);

  // Simulate an unclean/forcible shutdown.
  raw_transport->SetNextReadError(ERR_CONNECTION_RESET);

  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));

  // Note: This test will hang if this bug has regressed. Simply checking that
  // rv != ERR_IO_PENDING is insufficient, as ERR_IO_PENDING is a legitimate
  // result when using a dedicated task runner for NSS.
  rv = callback.GetResult(sock->Read(buf.get(), 4096, callback.callback()));

#if !defined(USE_OPENSSL)
  // SSLClientSocketNSS records the error exactly
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);
#else
  // SSLClientSocketOpenSSL treats any errors as a simple EOF.
  EXPECT_EQ(0, rv);
#endif
}

// Tests that the SSLClientSocket properly handles when the underlying transport
// asynchronously returns an error code while writing data - such as if an
// intermediary terminates the socket connection uncleanly.
// This is a regression test for http://crbug.com/249848
TEST_F(SSLClientSocketTest, Write_WithSynchronousError) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> real_transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  // Note: |error_socket|'s ownership is handed to |transport|, but a pointer
  // is retained in order to configure additional errors.
  scoped_ptr<SynchronousErrorStreamSocket> error_socket(
      new SynchronousErrorStreamSocket(real_transport.Pass()));
  SynchronousErrorStreamSocket* raw_error_socket = error_socket.get();
  scoped_ptr<FakeBlockingStreamSocket> transport(
      new FakeBlockingStreamSocket(error_socket.PassAs<StreamSocket>()));
  FakeBlockingStreamSocket* raw_transport = transport.get();
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  // Disable TLS False Start to avoid handshake non-determinism.
  SSLConfig ssl_config;
  ssl_config.false_start_enabled = false;

  scoped_ptr<SSLClientSocket> sock(
      CreateSSLClientSocket(transport.PassAs<StreamSocket>(),
                            test_server.host_port_pair(),
                            ssl_config));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  static const int kRequestTextSize =
      static_cast<int>(arraysize(request_text) - 1);
  scoped_refptr<IOBuffer> request_buffer(new IOBuffer(kRequestTextSize));
  memcpy(request_buffer->data(), request_text, kRequestTextSize);

  // Simulate an unclean/forcible shutdown on the underlying socket.
  // However, simulate this error asynchronously.
  raw_error_socket->SetNextWriteError(ERR_CONNECTION_RESET);
  raw_transport->SetNextWriteShouldBlock();

  // This write should complete synchronously, because the TLS ciphertext
  // can be created and placed into the outgoing buffers independent of the
  // underlying transport.
  rv = callback.GetResult(
      sock->Write(request_buffer.get(), kRequestTextSize, callback.callback()));
  EXPECT_EQ(kRequestTextSize, rv);

  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));

  rv = sock->Read(buf.get(), 4096, callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Now unblock the outgoing request, having it fail with the connection
  // being reset.
  raw_transport->UnblockWrite();

  // Note: This will cause an inifite loop if this bug has regressed. Simply
  // checking that rv != ERR_IO_PENDING is insufficient, as ERR_IO_PENDING
  // is a legitimate result when using a dedicated task runner for NSS.
  rv = callback.GetResult(rv);

#if !defined(USE_OPENSSL)
  // SSLClientSocketNSS records the error exactly
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);
#else
  // SSLClientSocketOpenSSL treats any errors as a simple EOF.
  EXPECT_EQ(0, rv);
#endif
}

// Test the full duplex mode, with Read and Write pending at the same time.
// This test also serves as a regression test for http://crbug.com/29815.
TEST_F(SSLClientSocketTest, Read_FullDuplex) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;  // Used for everything except Write.

  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), kDefaultSSLConfig));

  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  // Issue a "hanging" Read first.
  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));
  rv = sock->Read(buf.get(), 4096, callback.callback());
  // We haven't written the request, so there should be no response yet.
  ASSERT_EQ(ERR_IO_PENDING, rv);

  // Write the request.
  // The request is padded with a User-Agent header to a size that causes the
  // memio circular buffer (4k bytes) in SSLClientSocketNSS to wrap around.
  // This tests the fix for http://crbug.com/29815.
  std::string request_text = "GET / HTTP/1.1\r\nUser-Agent: long browser name ";
  for (int i = 0; i < 3770; ++i)
    request_text.push_back('*');
  request_text.append("\r\n\r\n");
  scoped_refptr<IOBuffer> request_buffer(new StringIOBuffer(request_text));

  TestCompletionCallback callback2;  // Used for Write only.
  rv = sock->Write(
      request_buffer.get(), request_text.size(), callback2.callback());
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING)
    rv = callback2.WaitForResult();
  EXPECT_EQ(static_cast<int>(request_text.size()), rv);

  // Now get the Read result.
  rv = callback.WaitForResult();
  EXPECT_GT(rv, 0);
}

// Attempts to Read() and Write() from an SSLClientSocketNSS in full duplex
// mode when the underlying transport is blocked on sending data. When the
// underlying transport completes due to an error, it should invoke both the
// Read() and Write() callbacks. If the socket is deleted by the Read()
// callback, the Write() callback should not be invoked.
// Regression test for http://crbug.com/232633
TEST_F(SSLClientSocketTest, Read_DeleteWhilePendingFullDuplex) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> real_transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  // Note: |error_socket|'s ownership is handed to |transport|, but a pointer
  // is retained in order to configure additional errors.
  scoped_ptr<SynchronousErrorStreamSocket> error_socket(
      new SynchronousErrorStreamSocket(real_transport.Pass()));
  SynchronousErrorStreamSocket* raw_error_socket = error_socket.get();
  scoped_ptr<FakeBlockingStreamSocket> transport(
      new FakeBlockingStreamSocket(error_socket.PassAs<StreamSocket>()));
  FakeBlockingStreamSocket* raw_transport = transport.get();

  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  // Disable TLS False Start to avoid handshake non-determinism.
  SSLConfig ssl_config;
  ssl_config.false_start_enabled = false;

  scoped_ptr<SSLClientSocket> sock =
      CreateSSLClientSocket(transport.PassAs<StreamSocket>(),
                            test_server.host_port_pair(),
                            ssl_config);

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  std::string request_text = "GET / HTTP/1.1\r\nUser-Agent: long browser name ";
  request_text.append(20 * 1024, '*');
  request_text.append("\r\n\r\n");
  scoped_refptr<DrainableIOBuffer> request_buffer(new DrainableIOBuffer(
      new StringIOBuffer(request_text), request_text.size()));

  // Simulate errors being returned from the underlying Read() and Write() ...
  raw_error_socket->SetNextReadError(ERR_CONNECTION_RESET);
  raw_error_socket->SetNextWriteError(ERR_CONNECTION_RESET);
  // ... but have those errors returned asynchronously. Because the Write() will
  // return first, this will trigger the error.
  raw_transport->SetNextReadShouldBlock();
  raw_transport->SetNextWriteShouldBlock();

  // Enqueue a Read() before calling Write(), which should "hang" due to
  // the ERR_IO_PENDING caused by SetReadShouldBlock() and thus return.
  SSLClientSocket* raw_sock = sock.get();
  DeleteSocketCallback read_callback(sock.release());
  scoped_refptr<IOBuffer> read_buf(new IOBuffer(4096));
  rv = raw_sock->Read(read_buf.get(), 4096, read_callback.callback());

  // Ensure things didn't complete synchronously, otherwise |sock| is invalid.
  ASSERT_EQ(ERR_IO_PENDING, rv);
  ASSERT_FALSE(read_callback.have_result());

#if !defined(USE_OPENSSL)
  // NSS follows a pattern where a call to PR_Write will only consume as
  // much data as it can encode into application data records before the
  // internal memio buffer is full, which should only fill if writing a large
  // amount of data and the underlying transport is blocked. Once this happens,
  // NSS will return (total size of all application data records it wrote) - 1,
  // with the caller expected to resume with the remaining unsent data.
  //
  // This causes SSLClientSocketNSS::Write to return that it wrote some data
  // before it will return ERR_IO_PENDING, so make an extra call to Write() to
  // get the socket in the state needed for the test below.
  //
  // This is not needed for OpenSSL, because for OpenSSL,
  // SSL_MODE_ENABLE_PARTIAL_WRITE is not specified - thus
  // SSLClientSocketOpenSSL::Write() will not return until all of
  // |request_buffer| has been written to the underlying BIO (although not
  // necessarily the underlying transport).
  rv = callback.GetResult(raw_sock->Write(request_buffer.get(),
                                          request_buffer->BytesRemaining(),
                                          callback.callback()));
  ASSERT_LT(0, rv);
  request_buffer->DidConsume(rv);

  // Guard to ensure that |request_buffer| was larger than all of the internal
  // buffers (transport, memio, NSS) along the way - otherwise the next call
  // to Write() will crash with an invalid buffer.
  ASSERT_LT(0, request_buffer->BytesRemaining());
#endif

  // Attempt to write the remaining data. NSS will not be able to consume the
  // application data because the internal buffers are full, while OpenSSL will
  // return that its blocked because the underlying transport is blocked.
  rv = raw_sock->Write(request_buffer.get(),
                       request_buffer->BytesRemaining(),
                       callback.callback());
  ASSERT_EQ(ERR_IO_PENDING, rv);
  ASSERT_FALSE(callback.have_result());

  // Now unblock Write(), which will invoke OnSendComplete and (eventually)
  // call the Read() callback, deleting the socket and thus aborting calling
  // the Write() callback.
  raw_transport->UnblockWrite();

  rv = read_callback.WaitForResult();

#if !defined(USE_OPENSSL)
  // NSS records the error exactly.
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);
#else
  // OpenSSL treats any errors as a simple EOF.
  EXPECT_EQ(0, rv);
#endif

  // The Write callback should not have been called.
  EXPECT_FALSE(callback.have_result());
}

// Tests that the SSLClientSocket does not crash if data is received on the
// transport socket after a failing write. This can occur if we have a Write
// error in a SPDY socket.
// Regression test for http://crbug.com/335557
TEST_F(SSLClientSocketTest, Read_WithWriteError) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> real_transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  // Note: |error_socket|'s ownership is handed to |transport|, but a pointer
  // is retained in order to configure additional errors.
  scoped_ptr<SynchronousErrorStreamSocket> error_socket(
      new SynchronousErrorStreamSocket(real_transport.Pass()));
  SynchronousErrorStreamSocket* raw_error_socket = error_socket.get();
  scoped_ptr<FakeBlockingStreamSocket> transport(
      new FakeBlockingStreamSocket(error_socket.PassAs<StreamSocket>()));
  FakeBlockingStreamSocket* raw_transport = transport.get();

  int rv = callback.GetResult(transport->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);

  // Disable TLS False Start to avoid handshake non-determinism.
  SSLConfig ssl_config;
  ssl_config.false_start_enabled = false;

  scoped_ptr<SSLClientSocket> sock(
      CreateSSLClientSocket(transport.PassAs<StreamSocket>(),
                            test_server.host_port_pair(),
                            ssl_config));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  // Send a request so there is something to read from the socket.
  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  static const int kRequestTextSize =
      static_cast<int>(arraysize(request_text) - 1);
  scoped_refptr<IOBuffer> request_buffer(new IOBuffer(kRequestTextSize));
  memcpy(request_buffer->data(), request_text, kRequestTextSize);

  rv = callback.GetResult(
      sock->Write(request_buffer.get(), kRequestTextSize, callback.callback()));
  EXPECT_EQ(kRequestTextSize, rv);

  // Start a hanging read.
  TestCompletionCallback read_callback;
  raw_transport->SetNextReadShouldBlock();
  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));
  rv = sock->Read(buf.get(), 4096, read_callback.callback());
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Perform another write, but have it fail. Write a request larger than the
  // internal socket buffers so that the request hits the underlying transport
  // socket and detects the error.
  std::string long_request_text =
      "GET / HTTP/1.1\r\nUser-Agent: long browser name ";
  long_request_text.append(20 * 1024, '*');
  long_request_text.append("\r\n\r\n");
  scoped_refptr<DrainableIOBuffer> long_request_buffer(new DrainableIOBuffer(
      new StringIOBuffer(long_request_text), long_request_text.size()));

  raw_error_socket->SetNextWriteError(ERR_CONNECTION_RESET);

  // Write as much data as possible until hitting an error. This is necessary
  // for NSS. PR_Write will only consume as much data as it can encode into
  // application data records before the internal memio buffer is full, which
  // should only fill if writing a large amount of data and the underlying
  // transport is blocked. Once this happens, NSS will return (total size of all
  // application data records it wrote) - 1, with the caller expected to resume
  // with the remaining unsent data.
  do {
    rv = callback.GetResult(sock->Write(long_request_buffer.get(),
                                        long_request_buffer->BytesRemaining(),
                                        callback.callback()));
    if (rv > 0) {
      long_request_buffer->DidConsume(rv);
      // Abort if the entire buffer is ever consumed.
      ASSERT_LT(0, long_request_buffer->BytesRemaining());
    }
  } while (rv > 0);

#if !defined(USE_OPENSSL)
  // NSS records the error exactly.
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);
#else
  // OpenSSL treats the reset as a generic protocol error.
  EXPECT_EQ(ERR_SSL_PROTOCOL_ERROR, rv);
#endif

  // Release the read. Some bytes should go through.
  raw_transport->UnblockRead();
  rv = read_callback.WaitForResult();

  // Per the fix for http://crbug.com/249848, write failures currently break
  // reads. Change this assertion if they're changed to not collide.
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);
}

TEST_F(SSLClientSocketTest, Read_SmallChunks) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), kDefaultSSLConfig));

  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<IOBuffer> request_buffer(
      new IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(
      request_buffer.get(), arraysize(request_text) - 1, callback.callback());
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  scoped_refptr<IOBuffer> buf(new IOBuffer(1));
  for (;;) {
    rv = sock->Read(buf.get(), 1, callback.callback());
    EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;
  }
}

TEST_F(SSLClientSocketTest, Read_ManySmallRecords) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;

  scoped_ptr<StreamSocket> real_transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  scoped_ptr<ReadBufferingStreamSocket> transport(
      new ReadBufferingStreamSocket(real_transport.Pass()));
  ReadBufferingStreamSocket* raw_transport = transport.get();
  int rv = callback.GetResult(transport->Connect(callback.callback()));
  ASSERT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(
      CreateSSLClientSocket(transport.PassAs<StreamSocket>(),
                            test_server.host_port_pair(),
                            kDefaultSSLConfig));

  rv = callback.GetResult(sock->Connect(callback.callback()));
  ASSERT_EQ(OK, rv);
  ASSERT_TRUE(sock->IsConnected());

  const char request_text[] = "GET /ssl-many-small-records HTTP/1.0\r\n\r\n";
  scoped_refptr<IOBuffer> request_buffer(
      new IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = callback.GetResult(sock->Write(
      request_buffer.get(), arraysize(request_text) - 1, callback.callback()));
  ASSERT_GT(rv, 0);
  ASSERT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  // Note: This relies on SSLClientSocketNSS attempting to read up to 17K of
  // data (the max SSL record size) at a time. Ensure that at least 15K worth
  // of SSL data is buffered first. The 15K of buffered data is made up of
  // many smaller SSL records (the TestServer writes along 1350 byte
  // plaintext boundaries), although there may also be a few records that are
  // smaller or larger, due to timing and SSL False Start.
  // 15K was chosen because 15K is smaller than the 17K (max) read issued by
  // the SSLClientSocket implementation, and larger than the minimum amount
  // of ciphertext necessary to contain the 8K of plaintext requested below.
  raw_transport->SetBufferSize(15000);

  scoped_refptr<IOBuffer> buffer(new IOBuffer(8192));
  rv = callback.GetResult(sock->Read(buffer.get(), 8192, callback.callback()));
  ASSERT_EQ(rv, 8192);
}

TEST_F(SSLClientSocketTest, Read_Interrupted) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), kDefaultSSLConfig));

  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<IOBuffer> request_buffer(
      new IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(
      request_buffer.get(), arraysize(request_text) - 1, callback.callback());
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  // Do a partial read and then exit.  This test should not crash!
  scoped_refptr<IOBuffer> buf(new IOBuffer(512));
  rv = sock->Read(buf.get(), 512, callback.callback());
  EXPECT_TRUE(rv > 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_GT(rv, 0);
}

TEST_F(SSLClientSocketTest, Read_FullLogging) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  CapturingNetLog log;
  log.SetLogLevel(NetLog::LOG_ALL);
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), kDefaultSSLConfig));

  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  const char request_text[] = "GET / HTTP/1.0\r\n\r\n";
  scoped_refptr<IOBuffer> request_buffer(
      new IOBuffer(arraysize(request_text) - 1));
  memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);

  rv = sock->Write(
      request_buffer.get(), arraysize(request_text) - 1, callback.callback());
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(arraysize(request_text) - 1), rv);

  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  size_t last_index = ExpectLogContainsSomewhereAfter(
      entries, 5, NetLog::TYPE_SSL_SOCKET_BYTES_SENT, NetLog::PHASE_NONE);

  scoped_refptr<IOBuffer> buf(new IOBuffer(4096));
  for (;;) {
    rv = sock->Read(buf.get(), 4096, callback.callback());
    EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

    if (rv == ERR_IO_PENDING)
      rv = callback.WaitForResult();

    EXPECT_GE(rv, 0);
    if (rv <= 0)
      break;

    log.GetEntries(&entries);
    last_index =
        ExpectLogContainsSomewhereAfter(entries,
                                        last_index + 1,
                                        NetLog::TYPE_SSL_SOCKET_BYTES_RECEIVED,
                                        NetLog::PHASE_NONE);
  }
}

// Regression test for http://crbug.com/42538
TEST_F(SSLClientSocketTest, PrematureApplicationData) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  TestCompletionCallback callback;

  static const unsigned char application_data[] = {
      0x17, 0x03, 0x01, 0x00, 0x4a, 0x02, 0x00, 0x00, 0x46, 0x03, 0x01, 0x4b,
      0xc2, 0xf8, 0xb2, 0xc1, 0x56, 0x42, 0xb9, 0x57, 0x7f, 0xde, 0x87, 0x46,
      0xf7, 0xa3, 0x52, 0x42, 0x21, 0xf0, 0x13, 0x1c, 0x9c, 0x83, 0x88, 0xd6,
      0x93, 0x0c, 0xf6, 0x36, 0x30, 0x05, 0x7e, 0x20, 0xb5, 0xb5, 0x73, 0x36,
      0x53, 0x83, 0x0a, 0xfc, 0x17, 0x63, 0xbf, 0xa0, 0xe4, 0x42, 0x90, 0x0d,
      0x2f, 0x18, 0x6d, 0x20, 0xd8, 0x36, 0x3f, 0xfc, 0xe6, 0x01, 0xfa, 0x0f,
      0xa5, 0x75, 0x7f, 0x09, 0x00, 0x04, 0x00, 0x16, 0x03, 0x01, 0x11, 0x57,
      0x0b, 0x00, 0x11, 0x53, 0x00, 0x11, 0x50, 0x00, 0x06, 0x22, 0x30, 0x82,
      0x06, 0x1e, 0x30, 0x82, 0x05, 0x06, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02,
      0x0a};

  // All reads and writes complete synchronously (async=false).
  MockRead data_reads[] = {
      MockRead(SYNCHRONOUS,
               reinterpret_cast<const char*>(application_data),
               arraysize(application_data)),
      MockRead(SYNCHRONOUS, OK), };

  StaticSocketDataProvider data(data_reads, arraysize(data_reads), NULL, 0);

  scoped_ptr<StreamSocket> transport(
      new MockTCPClientSocket(addr, NULL, &data));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), kDefaultSSLConfig));

  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(ERR_SSL_PROTOCOL_ERROR, rv);
}

TEST_F(SSLClientSocketTest, CipherSuiteDisables) {
  // Rather than exhaustively disabling every RC4 ciphersuite defined at
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml,
  // only disabling those cipher suites that the test server actually
  // implements.
  const uint16 kCiphersToDisable[] = {0x0005,  // TLS_RSA_WITH_RC4_128_SHA
  };

  SpawnedTestServer::SSLOptions ssl_options;
  // Enable only RC4 on the test server.
  ssl_options.bulk_ciphers = SpawnedTestServer::SSLOptions::BULK_CIPHER_RC4;
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  CapturingNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  SSLConfig ssl_config;
  for (size_t i = 0; i < arraysize(kCiphersToDisable); ++i)
    ssl_config.disabled_cipher_suites.push_back(kCiphersToDisable[i]);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), ssl_config));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());
  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));

  // NSS has special handling that maps a handshake_failure alert received
  // immediately after a client_hello to be a mismatched cipher suite error,
  // leading to ERR_SSL_VERSION_OR_CIPHER_MISMATCH. When using OpenSSL or
  // Secure Transport (OS X), the handshake_failure is bubbled up without any
  // interpretation, leading to ERR_SSL_PROTOCOL_ERROR. Either way, a failure
  // indicates that no cipher suite was negotiated with the test server.
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_TRUE(rv == ERR_SSL_VERSION_OR_CIPHER_MISMATCH ||
              rv == ERR_SSL_PROTOCOL_ERROR);
  // The exact ordering differs between SSLClientSocketNSS (which issues an
  // extra read) and SSLClientSocketMac (which does not). Just make sure the
  // error appears somewhere in the log.
  log.GetEntries(&entries);
  ExpectLogContainsSomewhere(
      entries, 0, NetLog::TYPE_SSL_HANDSHAKE_ERROR, NetLog::PHASE_NONE);

  // We cannot test sock->IsConnected(), as the NSS implementation disconnects
  // the socket when it encounters an error, whereas other implementations
  // leave it connected.
  // Because this an error that the test server is mutually aware of, as opposed
  // to being an error such as a certificate name mismatch, which is
  // client-only, the exact index of the SSL connect end depends on how
  // quickly the test server closes the underlying socket. If the test server
  // closes before the IO message loop pumps messages, there may be a 0-byte
  // Read event in the NetLog due to TCPClientSocket picking up the EOF. As a
  // result, the SSL connect end event will be the second-to-last entry,
  // rather than the last entry.
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1) ||
              LogContainsSSLConnectEndEvent(entries, -2));
}

// When creating an SSLClientSocket, it is allowed to pass in a
// ClientSocketHandle that is not obtained from a client socket pool.
// Here we verify that such a simple ClientSocketHandle, not associated with any
// client socket pool, can be destroyed safely.
TEST_F(SSLClientSocketTest, ClientSocketHandleNotFromPool) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<ClientSocketHandle> socket_handle(new ClientSocketHandle());
  socket_handle->SetSocket(transport.Pass());

  scoped_ptr<SSLClientSocket> sock(
      socket_factory_->CreateSSLClientSocket(socket_handle.Pass(),
                                             test_server.host_port_pair(),
                                             kDefaultSSLConfig,
                                             context_));

  EXPECT_FALSE(sock->IsConnected());
  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

// Verifies that SSLClientSocket::ExportKeyingMaterial return a success
// code and different keying label results in different keying material.
TEST_F(SSLClientSocketTest, ExportKeyingMaterial) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;

  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), kDefaultSSLConfig));

  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());

  const int kKeyingMaterialSize = 32;
  const char* kKeyingLabel1 = "client-socket-test-1";
  const char* kKeyingContext = "";
  unsigned char client_out1[kKeyingMaterialSize];
  memset(client_out1, 0, sizeof(client_out1));
  rv = sock->ExportKeyingMaterial(
      kKeyingLabel1, false, kKeyingContext, client_out1, sizeof(client_out1));
  EXPECT_EQ(rv, OK);

  const char* kKeyingLabel2 = "client-socket-test-2";
  unsigned char client_out2[kKeyingMaterialSize];
  memset(client_out2, 0, sizeof(client_out2));
  rv = sock->ExportKeyingMaterial(
      kKeyingLabel2, false, kKeyingContext, client_out2, sizeof(client_out2));
  EXPECT_EQ(rv, OK);
  EXPECT_NE(memcmp(client_out1, client_out2, kKeyingMaterialSize), 0);
}

// Verifies that SSLClientSocket::ClearSessionCache can be called without
// explicit NSS initialization.
TEST(SSLClientSocket, ClearSessionCache) {
  SSLClientSocket::ClearSessionCache();
}

// Test that the server certificates are properly retrieved from the underlying
// SSL stack.
TEST_F(SSLClientSocketTest, VerifyServerChainProperlyOrdered) {
  // The connection does not have to be successful.
  cert_verifier_->set_default_result(ERR_CERT_INVALID);

  // Set up a test server with CERT_CHAIN_WRONG_ROOT.
  // This makes the server present redundant-server-chain.pem, which contains
  // intermediate certificates.
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_CHAIN_WRONG_ROOT);
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS, ssl_options, base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  rv = callback.GetResult(rv);
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), kDefaultSSLConfig));
  EXPECT_FALSE(sock->IsConnected());
  rv = sock->Connect(callback.callback());
  rv = callback.GetResult(rv);

  EXPECT_EQ(ERR_CERT_INVALID, rv);
  EXPECT_TRUE(sock->IsConnected());

  // When given option CERT_CHAIN_WRONG_ROOT, SpawnedTestServer will present
  // certs from redundant-server-chain.pem.
  CertificateList server_certs =
      CreateCertificateListFromFile(GetTestCertsDirectory(),
                                    "redundant-server-chain.pem",
                                    X509Certificate::FORMAT_AUTO);

  // Get the server certificate as received client side.
  scoped_refptr<X509Certificate> server_certificate =
      sock->GetUnverifiedServerCertificateChain();

  // Get the intermediates as received  client side.
  const X509Certificate::OSCertHandles& server_intermediates =
      server_certificate->GetIntermediateCertificates();

  // Check that the unverified server certificate chain is properly retrieved
  // from the underlying ssl stack.
  ASSERT_EQ(4U, server_certs.size());

  EXPECT_TRUE(X509Certificate::IsSameOSCert(
      server_certificate->os_cert_handle(), server_certs[0]->os_cert_handle()));

  ASSERT_EQ(3U, server_intermediates.size());

  EXPECT_TRUE(X509Certificate::IsSameOSCert(server_intermediates[0],
                                            server_certs[1]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(server_intermediates[1],
                                            server_certs[2]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(server_intermediates[2],
                                            server_certs[3]->os_cert_handle()));

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

// This tests that SSLInfo contains a properly re-constructed certificate
// chain. That, in turn, verifies that GetSSLInfo is giving us the chain as
// verified, not the chain as served by the server. (They may be different.)
//
// CERT_CHAIN_WRONG_ROOT is redundant-server-chain.pem. It contains A
// (end-entity) -> B -> C, and C is signed by D. redundant-validated-chain.pem
// contains a chain of A -> B -> C2, where C2 is the same public key as C, but
// a self-signed root. Such a situation can occur when a new root (C2) is
// cross-certified by an old root (D) and has two different versions of its
// floating around. Servers may supply C2 as an intermediate, but the
// SSLClientSocket should return the chain that was verified, from
// verify_result, instead.
TEST_F(SSLClientSocketTest, VerifyReturnChainProperlyOrdered) {
  // By default, cause the CertVerifier to treat all certificates as
  // expired.
  cert_verifier_->set_default_result(ERR_CERT_DATE_INVALID);

  // We will expect SSLInfo to ultimately contain this chain.
  CertificateList certs =
      CreateCertificateListFromFile(GetTestCertsDirectory(),
                                    "redundant-validated-chain.pem",
                                    X509Certificate::FORMAT_AUTO);
  ASSERT_EQ(3U, certs.size());

  X509Certificate::OSCertHandles temp_intermediates;
  temp_intermediates.push_back(certs[1]->os_cert_handle());
  temp_intermediates.push_back(certs[2]->os_cert_handle());

  CertVerifyResult verify_result;
  verify_result.verified_cert = X509Certificate::CreateFromHandle(
      certs[0]->os_cert_handle(), temp_intermediates);

  // Add a rule that maps the server cert (A) to the chain of A->B->C2
  // rather than A->B->C.
  cert_verifier_->AddResultForCert(certs[0].get(), verify_result, OK);

  // Load and install the root for the validated chain.
  scoped_refptr<X509Certificate> root_cert = ImportCertFromFile(
      GetTestCertsDirectory(), "redundant-validated-chain-root.pem");
  ASSERT_NE(static_cast<X509Certificate*>(NULL), root_cert);
  ScopedTestRoot scoped_root(root_cert.get());

  // Set up a test server with CERT_CHAIN_WRONG_ROOT.
  SpawnedTestServer::SSLOptions ssl_options(
      SpawnedTestServer::SSLOptions::CERT_CHAIN_WRONG_ROOT);
  SpawnedTestServer test_server(
      SpawnedTestServer::TYPE_HTTPS,
      ssl_options,
      base::FilePath(FILE_PATH_LITERAL("net/data/ssl")));
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  CapturingNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), kDefaultSSLConfig));
  EXPECT_FALSE(sock->IsConnected());
  rv = sock->Connect(callback.callback());

  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();

  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));

  SSLInfo ssl_info;
  sock->GetSSLInfo(&ssl_info);

  // Verify that SSLInfo contains the corrected re-constructed chain A -> B
  // -> C2.
  const X509Certificate::OSCertHandles& intermediates =
      ssl_info.cert->GetIntermediateCertificates();
  ASSERT_EQ(2U, intermediates.size());
  EXPECT_TRUE(X509Certificate::IsSameOSCert(ssl_info.cert->os_cert_handle(),
                                            certs[0]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(intermediates[0],
                                            certs[1]->os_cert_handle()));
  EXPECT_TRUE(X509Certificate::IsSameOSCert(intermediates[1],
                                            certs[2]->os_cert_handle()));

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

TEST_F(SSLClientSocketCertRequestInfoTest, NoAuthorities) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  scoped_refptr<SSLCertRequestInfo> request_info = GetCertRequest(ssl_options);
  ASSERT_TRUE(request_info.get());
  EXPECT_EQ(0u, request_info->cert_authorities.size());
}

TEST_F(SSLClientSocketCertRequestInfoTest, TwoAuthorities) {
  const base::FilePath::CharType kThawteFile[] =
      FILE_PATH_LITERAL("thawte.single.pem");
  const unsigned char kThawteDN[] = {
      0x30, 0x4c, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
      0x02, 0x5a, 0x41, 0x31, 0x25, 0x30, 0x23, 0x06, 0x03, 0x55, 0x04, 0x0a,
      0x13, 0x1c, 0x54, 0x68, 0x61, 0x77, 0x74, 0x65, 0x20, 0x43, 0x6f, 0x6e,
      0x73, 0x75, 0x6c, 0x74, 0x69, 0x6e, 0x67, 0x20, 0x28, 0x50, 0x74, 0x79,
      0x29, 0x20, 0x4c, 0x74, 0x64, 0x2e, 0x31, 0x16, 0x30, 0x14, 0x06, 0x03,
      0x55, 0x04, 0x03, 0x13, 0x0d, 0x54, 0x68, 0x61, 0x77, 0x74, 0x65, 0x20,
      0x53, 0x47, 0x43, 0x20, 0x43, 0x41};
  const size_t kThawteLen = sizeof(kThawteDN);

  const base::FilePath::CharType kDiginotarFile[] =
      FILE_PATH_LITERAL("diginotar_root_ca.pem");
  const unsigned char kDiginotarDN[] = {
      0x30, 0x5f, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
      0x02, 0x4e, 0x4c, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x0a,
      0x13, 0x09, 0x44, 0x69, 0x67, 0x69, 0x4e, 0x6f, 0x74, 0x61, 0x72, 0x31,
      0x1a, 0x30, 0x18, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x11, 0x44, 0x69,
      0x67, 0x69, 0x4e, 0x6f, 0x74, 0x61, 0x72, 0x20, 0x52, 0x6f, 0x6f, 0x74,
      0x20, 0x43, 0x41, 0x31, 0x20, 0x30, 0x1e, 0x06, 0x09, 0x2a, 0x86, 0x48,
      0x86, 0xf7, 0x0d, 0x01, 0x09, 0x01, 0x16, 0x11, 0x69, 0x6e, 0x66, 0x6f,
      0x40, 0x64, 0x69, 0x67, 0x69, 0x6e, 0x6f, 0x74, 0x61, 0x72, 0x2e, 0x6e,
      0x6c};
  const size_t kDiginotarLen = sizeof(kDiginotarDN);

  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.request_client_certificate = true;
  ssl_options.client_authorities.push_back(
      GetTestClientCertsDirectory().Append(kThawteFile));
  ssl_options.client_authorities.push_back(
      GetTestClientCertsDirectory().Append(kDiginotarFile));
  scoped_refptr<SSLCertRequestInfo> request_info = GetCertRequest(ssl_options);
  ASSERT_TRUE(request_info.get());
  ASSERT_EQ(2u, request_info->cert_authorities.size());
  EXPECT_EQ(std::string(reinterpret_cast<const char*>(kThawteDN), kThawteLen),
            request_info->cert_authorities[0]);
  EXPECT_EQ(
      std::string(reinterpret_cast<const char*>(kDiginotarDN), kDiginotarLen),
      request_info->cert_authorities[1]);
}

TEST_F(SSLClientSocketTest, ConnectSignedCertTimestampsEnabledTLSExtension) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.signed_cert_timestamps_tls_ext = "test";

  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                ssl_options,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  CapturingNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  SSLConfig ssl_config;
  ssl_config.signed_cert_timestamps_enabled = true;

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), ssl_config));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());

  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));

#if !defined(USE_OPENSSL)
  EXPECT_TRUE(sock->signed_cert_timestamps_received_);
#else
  // Enabling CT for OpenSSL is currently a noop.
  EXPECT_FALSE(sock->signed_cert_timestamps_received_);
#endif

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

// Test that enabling Signed Certificate Timestamps enables OCSP stapling.
TEST_F(SSLClientSocketTest, ConnectSignedCertTimestampsEnabledOCSP) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.staple_ocsp_response = true;
  // The test server currently only knows how to generate OCSP responses
  // for a freshly minted certificate.
  ssl_options.server_certificate = SpawnedTestServer::SSLOptions::CERT_AUTO;

  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                ssl_options,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  CapturingNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  SSLConfig ssl_config;
  // Enabling Signed Cert Timestamps ensures we request OCSP stapling for
  // Certificate Transparency verification regardless of whether the platform
  // is able to process the OCSP status itself.
  ssl_config.signed_cert_timestamps_enabled = true;

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), ssl_config));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());

  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));

#if !defined(USE_OPENSSL)
  EXPECT_TRUE(sock->stapled_ocsp_response_received_);
#else
  // OCSP stapling isn't currently supported in the OpenSSL socket.
  EXPECT_FALSE(sock->stapled_ocsp_response_received_);
#endif

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

TEST_F(SSLClientSocketTest, ConnectSignedCertTimestampsDisabled) {
  SpawnedTestServer::SSLOptions ssl_options;
  ssl_options.signed_cert_timestamps_tls_ext = "test";

  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                ssl_options,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  CapturingNetLog log;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, &log, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  SSLConfig ssl_config;
  ssl_config.signed_cert_timestamps_enabled = false;

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), ssl_config));

  EXPECT_FALSE(sock->IsConnected());

  rv = sock->Connect(callback.callback());

  CapturingNetLog::CapturedEntryList entries;
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsBeginEvent(entries, 5, NetLog::TYPE_SSL_CONNECT));
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_TRUE(sock->IsConnected());
  log.GetEntries(&entries);
  EXPECT_TRUE(LogContainsSSLConnectEndEvent(entries, -1));

  EXPECT_FALSE(sock->signed_cert_timestamps_received_);

  sock->Disconnect();
  EXPECT_FALSE(sock->IsConnected());
}

// Tests that IsConnectedAndIdle and WasEverUsed behave as expected.
TEST_F(SSLClientSocketTest, ReuseStates) {
  SpawnedTestServer test_server(SpawnedTestServer::TYPE_HTTPS,
                                SpawnedTestServer::kLocalhost,
                                base::FilePath());
  ASSERT_TRUE(test_server.Start());

  AddressList addr;
  ASSERT_TRUE(test_server.GetAddressList(&addr));

  TestCompletionCallback callback;
  scoped_ptr<StreamSocket> transport(
      new TCPClientSocket(addr, NULL, NetLog::Source()));
  int rv = transport->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  scoped_ptr<SSLClientSocket> sock(CreateSSLClientSocket(
      transport.Pass(), test_server.host_port_pair(), kDefaultSSLConfig));

  rv = sock->Connect(callback.callback());
  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  // The socket was just connected. It should be idle because it is speaking
  // HTTP. Although the transport has been used for the handshake, WasEverUsed()
  // returns false.
  EXPECT_TRUE(sock->IsConnected());
  EXPECT_TRUE(sock->IsConnectedAndIdle());
  EXPECT_FALSE(sock->WasEverUsed());

  const char kRequestText[] = "GET / HTTP/1.0\r\n\r\n";
  const size_t kRequestLen = arraysize(kRequestText) - 1;
  scoped_refptr<IOBuffer> request_buffer(new IOBuffer(kRequestLen));
  memcpy(request_buffer->data(), kRequestText, kRequestLen);

  rv = sock->Write(request_buffer.get(), kRequestLen, callback.callback());
  EXPECT_TRUE(rv >= 0 || rv == ERR_IO_PENDING);

  if (rv == ERR_IO_PENDING)
    rv = callback.WaitForResult();
  EXPECT_EQ(static_cast<int>(kRequestLen), rv);

  // The socket has now been used.
  EXPECT_TRUE(sock->WasEverUsed());

  // TODO(davidben): Read one byte to ensure the test server has responded and
  // then assert IsConnectedAndIdle is false. This currently doesn't work
  // because neither SSLClientSocketNSS nor SSLClientSocketOpenSSL check their
  // SSL implementation's internal buffers. Either call PR_Available and
  // SSL_pending, although the former isn't actually implemented or perhaps
  // attempt to read one byte extra.
}

}  // namespace net
