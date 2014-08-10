// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_SOCKET_H_
#define EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_SOCKET_H_

#include <queue>
#include <string>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/common/api/cast_channel.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_log.h"

namespace net {
class AddressList;
class CertVerifier;
class SSLClientSocket;
class StreamSocket;
class TCPClientSocket;
class TransportSecurityState;
}

namespace extensions {
namespace core_api {
namespace cast_channel {

class CastMessage;
class Logger;

// This class implements a channel between Chrome and a Cast device using a TCP
// socket with SSL.  The channel may authenticate that the receiver is a genuine
// Cast device.  All CastSocket objects must be used only on the IO thread.
//
// NOTE: Not called "CastChannel" to reduce confusion with the generated API
// code.
class CastSocket : public ApiResource {
 public:
  // Object to be informed of incoming messages and errors.  The CastSocket that
  // owns the delegate must not be deleted by it, only by the ApiResourceManager
  // or in the callback to Close().
  class Delegate {
   public:
    // An error occurred on the channel.
    virtual void OnError(const CastSocket* socket, ChannelError error) = 0;
    // A message was received on the channel.
    virtual void OnMessage(const CastSocket* socket,
                           const MessageInfo& message) = 0;

   protected:
    virtual ~Delegate() {}
  };

  // Creates a new CastSocket that connects to |ip_endpoint| with
  // |channel_auth|. |owner_extension_id| is the id of the extension that opened
  // the socket.  |channel_auth| must not be CHANNEL_AUTH_NONE.
  CastSocket(const std::string& owner_extension_id,
             const net::IPEndPoint& ip_endpoint,
             ChannelAuthType channel_auth,
             CastSocket::Delegate* delegate,
             net::NetLog* net_log,
             const base::TimeDelta& connect_timeout,
             const scoped_refptr<Logger>& logger);

  // Ensures that the socket is closed.
  virtual ~CastSocket();

  // The IP endpoint for the destination of the channel.
  const net::IPEndPoint& ip_endpoint() const { return ip_endpoint_; }

  // The authentication level requested for the channel.
  ChannelAuthType channel_auth() const { return channel_auth_; }

  // Returns a cast:// or casts:// URL for the channel endpoint.
  // For backwards compatibility.
  std::string CastUrl() const;

  // Channel id for the ApiResourceManager.
  int id() const { return channel_id_; }

  // Sets the channel id.
  void set_id(int channel_id) { channel_id_ = channel_id; }

  // Returns the state of the channel.  Virtual for testing.
  virtual ReadyState ready_state() const;

  // Returns the last error that occurred on this channel, or
  // CHANNEL_ERROR_NONE if no error has occurred.  Virtual for testing.
  virtual ChannelError error_state() const;

  // Connects the channel to the peer. If successful, the channel will be in
  // READY_STATE_OPEN.  DO NOT delete the CastSocket object in |callback|.
  // Instead use Close().
  virtual void Connect(const net::CompletionCallback& callback);

  // Sends a message over a connected channel. The channel must be in
  // READY_STATE_OPEN.
  //
  // Note that if an error occurs the following happens:
  // 1. Completion callbacks for all pending writes are invoked with error.
  // 2. Delegate::OnError is called once.
  // 3. CastSocket is closed.
  //
  // DO NOT delete the CastSocket object in |callback|. Instead use Close().
  virtual void SendMessage(const MessageInfo& message,
                           const net::CompletionCallback& callback);

  // Closes the channel if not already closed. On completion, the channel will
  // be in READY_STATE_CLOSED.
  //
  // It is fine to delete the CastSocket object in |callback|.
  virtual void Close(const net::CompletionCallback& callback);

  // Internal connection states.
  enum ConnectionState {
    CONN_STATE_NONE,
    CONN_STATE_TCP_CONNECT,
    CONN_STATE_TCP_CONNECT_COMPLETE,
    CONN_STATE_SSL_CONNECT,
    CONN_STATE_SSL_CONNECT_COMPLETE,
    CONN_STATE_AUTH_CHALLENGE_SEND,
    CONN_STATE_AUTH_CHALLENGE_SEND_COMPLETE,
    CONN_STATE_AUTH_CHALLENGE_REPLY_COMPLETE,
  };

  // Internal write states.
  enum WriteState {
    WRITE_STATE_NONE,
    WRITE_STATE_WRITE,
    WRITE_STATE_WRITE_COMPLETE,
    WRITE_STATE_DO_CALLBACK,
    WRITE_STATE_ERROR,
  };

  // Internal read states.
  enum ReadState {
    READ_STATE_NONE,
    READ_STATE_READ,
    READ_STATE_READ_COMPLETE,
    READ_STATE_DO_CALLBACK,
    READ_STATE_ERROR,
  };

 protected:
  // Message header struct. If fields are added, be sure to update
  // header_size().  Protected to allow use of *_size() methods in unit tests.
  struct MessageHeader {
    MessageHeader();
    // Sets the message size.
    void SetMessageSize(size_t message_size);
    // Prepends this header to |str|.
    void PrependToString(std::string* str);
    // Reads |header| from the beginning of |buffer|.
    static void ReadFromIOBuffer(net::GrowableIOBuffer* buffer,
                                 MessageHeader* header);
    // Size (in bytes) of the message header.
    static uint32 header_size() { return sizeof(uint32); }

    // Maximum size (in bytes) of a message payload on the wire (does not
    // include header).
    static uint32 max_message_size() { return 65536; }

    std::string ToString();
    // The size of the following protocol message in bytes, in host byte order.
    uint32 message_size;
  };

 private:
  friend class ApiResourceManager<CastSocket>;
  friend class CastSocketTest;
  friend class TestCastSocket;

  static const char* service_name() { return "CastSocketManager"; }

  // Creates an instance of TCPClientSocket.
  virtual scoped_ptr<net::TCPClientSocket> CreateTcpSocket();
  // Creates an instance of SSLClientSocket with the given underlying |socket|.
  virtual scoped_ptr<net::SSLClientSocket> CreateSslSocket(
      scoped_ptr<net::StreamSocket> socket);
  // Extracts peer certificate from SSLClientSocket instance when the socket
  // is in cert error state.
  // Returns whether certificate is successfully extracted.
  virtual bool ExtractPeerCert(std::string* cert);
  // Verifies whether the challenge reply received from the peer is valid:
  // 1. Signature in the reply is valid.
  // 2. Certificate is rooted to a trusted CA.
  virtual bool VerifyChallengeReply();

  // Invoked by a cancelable closure when connection setup time
  // exceeds the interval specified at |connect_timeout|.
  void OnConnectTimeout();

  /////////////////////////////////////////////////////////////////////////////
  // Following methods work together to implement the following flow:
  // 1. Create a new TCP socket and connect to it
  // 2. Create a new SSL socket and try connecting to it
  // 3. If connection fails due to invalid cert authority, then extract the
  //    peer certificate from the error.
  // 4. Whitelist the peer certificate and try #1 and #2 again.
  // 5. If SSL socket is connected successfully, and if protocol is casts://
  //    then issue an auth challenge request.
  // 6. Validate the auth challenge response.
  //
  // Main method that performs connection state transitions.
  void DoConnectLoop(int result);
  // Each of the below Do* method is executed in the corresponding
  // connection state. For example when connection state is TCP_CONNECT
  // DoTcpConnect is called, and so on.
  int DoTcpConnect();
  int DoTcpConnectComplete(int result);
  int DoSslConnect();
  int DoSslConnectComplete(int result);
  int DoAuthChallengeSend();
  int DoAuthChallengeSendComplete(int result);
  void DoAuthChallengeSendWriteComplete(int result);
  int DoAuthChallengeReplyComplete(int result);
  /////////////////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////////////////
  // Following methods work together to implement write flow.
  //
  // Main method that performs write flow state transitions.
  void DoWriteLoop(int result);
  // Each of the below Do* method is executed in the corresponding
  // write state. For example when write state is WRITE_STATE_WRITE_COMPLETE
  // DowriteComplete is called, and so on.
  int DoWrite();
  int DoWriteComplete(int result);
  int DoWriteCallback();
  int DoWriteError(int result);
  /////////////////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////////////////
  // Following methods work together to implement read flow.
  //
  // Main method that performs write flow state transitions.
  void DoReadLoop(int result);
  // Each of the below Do* method is executed in the corresponding
  // write state. For example when write state is READ_STATE_READ_COMPLETE
  // DoReadComplete is called, and so on.
  int DoRead();
  int DoReadComplete(int result);
  int DoReadCallback();
  int DoReadError(int result);
  /////////////////////////////////////////////////////////////////////////////

  // Runs the external connection callback and resets it.
  void DoConnectCallback(int result);
  // Adds |message| to the write queue and starts the write loop if needed.
  void SendCastMessageInternal(const CastMessage& message,
                               const net::CompletionCallback& callback);
  void PostTaskToStartConnectLoop(int result);
  void PostTaskToStartReadLoop();
  void StartReadLoop();
  // Parses the contents of header_read_buffer_ and sets current_message_size_
  // to the size of the body of the message.
  bool ProcessHeader();
  // Parses the contents of body_read_buffer_ and sets current_message_ to
  // the message received.
  bool ProcessBody();
  // Closes socket, signaling the delegate that |error| has occurred.
  void CloseWithError();
  // Frees resources and cancels pending callbacks.  |ready_state_| will be set
  // READY_STATE_CLOSED on completion.  A no-op if |ready_state_| is already
  // READY_STATE_CLOSED.
  void CloseInternal();
  // Runs pending callbacks that are passed into us to notify API clients that
  // pending operations will fail because the socket has been closed.
  void RunPendingCallbacksOnClose();
  // Serializes the content of message_proto (with a header) to |message_data|.
  static bool Serialize(const CastMessage& message_proto,
                        std::string* message_data);

  virtual bool CalledOnValidThread() const;

  virtual base::Timer* GetTimer();

  void SetConnectState(ConnectionState connect_state);
  void SetReadyState(ReadyState ready_state);
  void SetErrorState(ChannelError error_state);
  void SetReadState(ReadState read_state);
  void SetWriteState(WriteState write_state);

  base::ThreadChecker thread_checker_;

  // The id of the channel.
  int channel_id_;

  // The IP endpoint that the the channel is connected to.
  net::IPEndPoint ip_endpoint_;
  // Receiver authentication requested for the channel.
  ChannelAuthType channel_auth_;
  // Delegate to inform of incoming messages and errors.
  Delegate* delegate_;

  // IOBuffer for reading the message header.
  scoped_refptr<net::GrowableIOBuffer> header_read_buffer_;
  // IOBuffer for reading the message body.
  scoped_refptr<net::GrowableIOBuffer> body_read_buffer_;
  // IOBuffer to currently read into.
  scoped_refptr<net::GrowableIOBuffer> current_read_buffer_;
  // The number of bytes in the current message body.
  uint32 current_message_size_;
  // Last message received on the socket.
  scoped_ptr<CastMessage> current_message_;

  // The NetLog for this service.
  net::NetLog* net_log_;
  // The NetLog source for this service.
  net::NetLog::Source net_log_source_;

  // Logger used to track multiple CastSockets. Does NOT own this object.
  scoped_refptr<Logger> logger_;

  // CertVerifier is owned by us but should be deleted AFTER SSLClientSocket
  // since in some cases the destructor of SSLClientSocket may call a method
  // to cancel a cert verification request.
  scoped_ptr<net::CertVerifier> cert_verifier_;
  scoped_ptr<net::TransportSecurityState> transport_security_state_;

  // Owned ptr to the underlying TCP socket.
  scoped_ptr<net::TCPClientSocket> tcp_socket_;
  // Owned ptr to the underlying SSL socket.
  scoped_ptr<net::SSLClientSocket> socket_;
  // Certificate of the peer. This field may be empty if the peer
  // certificate is not yet fetched.
  std::string peer_cert_;
  // Reply received from the receiver to a challenge request.
  scoped_ptr<CastMessage> challenge_reply_;

  // Callback invoked when the socket is connected or fails to connect.
  net::CompletionCallback connect_callback_;

  // Callback invoked by |connect_timeout_timer_| to cancel the connection.
  base::CancelableClosure connect_timeout_callback_;
  // Duration to wait before timing out.
  base::TimeDelta connect_timeout_;
  // Timer invoked when the connection has timed out.
  scoped_ptr<base::Timer> connect_timeout_timer_;
  // Set when a timeout is triggered and the connection process has
  // canceled.
  bool is_canceled_;

  // Connection flow state machine state.
  ConnectionState connect_state_;
  // Write flow state machine state.
  WriteState write_state_;
  // Read flow state machine state.
  ReadState read_state_;
  // The last error encountered by the channel.
  ChannelError error_state_;
  // The current status of the channel.
  ReadyState ready_state_;

  // Task invoked to (re)start the connect loop.  Canceled on entry to the
  // connect loop.
  base::CancelableClosure connect_loop_callback_;
  // Task invoked to send the auth challenge.  Canceled when the auth challenge
  // has been sent.
  base::CancelableClosure send_auth_challenge_callback_;
  // Callback invoked to (re)start the read loop.  Canceled on entry to the read
  // loop.
  base::CancelableClosure read_loop_callback_;

  // Holds a message to be written to the socket. |callback| is invoked when the
  // message is fully written or an error occurrs.
  struct WriteRequest {
    explicit WriteRequest(const net::CompletionCallback& callback);
    ~WriteRequest();
    // Sets the content of the request by serializing |message| into |io_buffer|
    // and prepending the header.  Must only be called once.
    bool SetContent(const CastMessage& message_proto);

    net::CompletionCallback callback;
    std::string message_namespace;
    scoped_refptr<net::DrainableIOBuffer> io_buffer;
  };
  // Queue of pending writes. The message at the front of the queue is the one
  // being written.
  std::queue<WriteRequest> write_queue_;

  FRIEND_TEST_ALL_PREFIXES(CastSocketTest, TestFullSecureConnectionFlowAsync);
  FRIEND_TEST_ALL_PREFIXES(CastSocketTest, TestRead);
  FRIEND_TEST_ALL_PREFIXES(CastSocketTest, TestReadHeaderParseError);
  FRIEND_TEST_ALL_PREFIXES(CastSocketTest, TestReadMany);
  FRIEND_TEST_ALL_PREFIXES(CastSocketTest, TestWriteErrorLargeMessage);
  DISALLOW_COPY_AND_ASSIGN(CastSocket);
};

}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_CAST_CHANNEL_CAST_SOCKET_H_
