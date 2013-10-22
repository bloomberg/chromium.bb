// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CAST_CHANNEL_CAST_SOCKET_H_
#define CHROME_BROWSER_EXTENSIONS_API_CAST_CHANNEL_CAST_SOCKET_H_

#include <queue>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/extensions/api/api_resource.h"
#include "chrome/browser/extensions/api/api_resource_manager.h"
#include "chrome/common/extensions/api/cast_channel.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_log.h"
#include "url/gurl.h"

namespace net {
class AddressList;
class CertVerifier;
class SSLClientSocket;
class TCPClientSocket;
class TransportSecurityState;
}

namespace extensions {
namespace api {
namespace cast_channel {

class CastMessage;

// Size, in bytes, of the largest allowed message payload on the wire (without
// the header).
extern const uint32 kMaxMessageSize;

// This class implements a channel between Chrome and a Cast device using a TCP
// socket. The channel may be unauthenticated (cast://) or authenticated
// (casts://). All CastSocket objects must be used only on the IO thread.
//
// NOTE: Not called "CastChannel" to reduce confusion with the generated API
// code.
class CastSocket : public ApiResource,
                   public base::SupportsWeakPtr<CastSocket>,
                   public base::NonThreadSafe {
 public:
  // Object to be informed of incoming messages and errors.
  class Delegate {
   public:
    // An error occurred on the channel.
    virtual void OnError(const CastSocket* socket,
                         ChannelError error) = 0;
    // A string message was received on the channel.
    virtual void OnMessage(const CastSocket* socket,
                           const MessageInfo& message) = 0;
   protected:
    virtual ~Delegate() {}
  };

  // Creates a new CastSocket to |url|. |owner_extension_id| is the id of the
  // extension that opened the socket.
  CastSocket(const std::string& owner_extension_id,
             const GURL& url,
             CastSocket::Delegate* delegate,
             net::NetLog* net_log);
  virtual ~CastSocket();

  // The URL for the channel.
  const GURL& url() const;

  // True if the protocol is casts:
  bool is_secure() const { return is_secure_; }

  // Channel id for the ApiResourceManager.
  long id() const { return channel_id_; }

  // Sets the channel id.
  void set_id(long channel_id) { channel_id_ = channel_id; }

  // Returns the state of the channel.
  const ReadyState& ready_state() const { return ready_state_; }

  // Returns the last error that occurred on this channel, or CHANNEL_ERROR_NONE
  // if no error has occurred..
  const ChannelError& error_state() const { return error_state_; }

  // Connects the channel to the peer. If successful, the channel will be in
  // READY_STATE_OPEN.
  virtual void Connect(const net::CompletionCallback& callback);

  // Sends a message over a connected channel. The channel must be in
  // READY_STATE_OPEN.
  virtual void SendMessage(const MessageInfo& message,
                           const net::CompletionCallback& callback);

  // Closes the channel. On completion, the channel will be in
  // READY_STATE_CLOSED.
  virtual void Close(const net::CompletionCallback& callback);

  // Fills |channel_info| with the status of this channel.
  void FillChannelInfo(ChannelInfo* channel_info) const;

 protected:
  // Creates an instance of TCPClientSocket.
  virtual scoped_ptr<net::TCPClientSocket> CreateTcpSocket();
  // Creates an instance of SSLClientSocket.
  virtual scoped_ptr<net::SSLClientSocket> CreateSslSocket();
  // Extracts peer certificate from SSLClientSocket instance when the socket
  // is in cert error state.
  // Returns whether certificate is successfully extracted.
  virtual bool ExtractPeerCert(std::string* cert);

 private:
  friend class ApiResourceManager<CastSocket>;
  static const char* service_name() {
    return "CastSocketManager";
  }

  // Internal connection states.
  enum ConnectionState {
    CONN_STATE_NONE,
    CONN_STATE_TCP_CONNECT,
    CONN_STATE_TCP_CONNECT_COMPLETE,
    CONN_STATE_SSL_CONNECT,
    CONN_STATE_SSL_CONNECT_COMPLETE,
  };

  /////////////////////////////////////////////////////////////////////////////
  // Following methods work together to implement the following flow:
  // 1. Create a new TCP socket and connect to it
  // 2. Create a new SSL socket and try connecting to it
  // 3. If connection fails due to invalid cert authority, then extract the
  //    peer certificate from the error.
  // 4. Whitelist the peer certificate and try #1 and #2 again.

  // Main method that performs connection state transitions.
  int DoConnectLoop(int result);
  // Each of the below Do* method is executed in the corresponding
  // connection state. For e.g. when connection state is TCP_CONNECT
  // DoTcpConnect is called, and so on.
  int DoTcpConnect();
  int DoTcpConnectComplete(int result);
  int DoSslConnect();
  int DoSslConnectComplete(int result);
  int DoSslConnectRetry();
  /////////////////////////////////////////////////////////////////////////////

  // Callback method for callbacks from underlying sockets.
  void OnConnectComplete(int result);

  // Runs the external connection callback and resets it.
  void DoConnectCallback(int result);

  // Verifies that the URL is a valid cast:// or casts:// URL and sets url_ to
  // the result.
  bool ParseChannelUrl(const GURL& url);

  // Writes data to the socket from the WriteRequest at the head of the queue.
  // Calls OnWriteData() on completion.
  void WriteData();
  void OnWriteData(int result);

  // Reads data from the socket into one of the read buffers. Calls
  // OnReadData() on completion.
  void ReadData();
  void OnReadData(int result);

  // Processes the contents of header_read_buffer_ and returns true on success.
  bool ProcessHeader();
  // Processes the contents of body_read_buffer_ and returns true on success.
  bool ProcessBody();
  // Parses the message held in body_read_buffer_ and notifies |delegate_| if a
  // message was extracted from the buffer.  Returns true on success.
  bool ParseMessageFromBody();

  // Serializes the content of message_proto (with a header) to |message_data|.
  static bool Serialize(const CastMessage& message_proto,
                        std::string* message_data);

  // Closes the socket and sets |error_state_|. Also signals |error| via
  // |delegate_|.
  void CloseWithError(ChannelError error);

  // The id of the channel.
  long channel_id_;

  // The URL of the peer (cast:// or casts://).
  GURL url_;
  // Delegate to inform of incoming messages and errors.
  Delegate* delegate_;
  // True if the channel is using a secure transport.
  bool is_secure_;
  // The IP endpoint of the peer.
  net::IPEndPoint ip_endpoint_;
  // The last error encountered by the channel.
  ChannelError error_state_;
  // The current status of the channel.
  ReadyState ready_state_;

  // True when there is a write callback pending.
  bool write_callback_pending_;
  // True when there is a read callback pending.
  bool read_callback_pending_;

  // IOBuffer for reading the message header.
  scoped_refptr<net::GrowableIOBuffer> header_read_buffer_;
  // IOBuffer for reading the message body.
  scoped_refptr<net::GrowableIOBuffer> body_read_buffer_;
  // IOBuffer we are currently reading into.
  scoped_refptr<net::GrowableIOBuffer> current_read_buffer_;
  // The number of bytes in the current message body.
  uint32 current_message_size_;

  // The NetLog for this service.
  net::NetLog* net_log_;
  // The NetLog source for this service.
  net::NetLog::Source net_log_source_;

  // Next connection state to transition to.
  ConnectionState next_state_;
  // Owned ptr to the underlying TCP socket.
  scoped_ptr<net::TCPClientSocket> tcp_socket_;
  // Owned ptr to the underlying SSL socket.
  scoped_ptr<net::SSLClientSocket> socket_;
  // Certificate of the peer. This field may be empty if the peer
  // certificate is not yet fetched.
  std::string peer_cert_;
  scoped_ptr<net::CertVerifier> cert_verifier_;
  scoped_ptr<net::TransportSecurityState> transport_security_state_;

  // Callback invoked when the socket is connected.
  net::CompletionCallback connect_callback_;

  // Message header struct. If fields are added, be sure to update
  // kMessageHeaderSize in the .cc.
  struct MessageHeader {
    MessageHeader();
    // Sets the message size.
    void SetMessageSize(size_t message_size);
    // Prepends this header to |str|.
    void PrependToString(std::string* str);
    // Reads |header| from the beginning of |buffer|.
    static void ReadFromIOBuffer(net::GrowableIOBuffer* buffer,
                                 MessageHeader* header);
    std::string ToString();
    // The size of the following protocol message in bytes, in host byte order.
    uint32 message_size;
  };

  // Holds a message to be written to the socket. |callback| is invoked when the
  // message is fully written or an error occurrs.
  struct WriteRequest {
    explicit WriteRequest(const net::CompletionCallback& callback);
    ~WriteRequest();
    // Sets the content of the request by serializing |message| into |io_buffer|
    // and prepending the header.  Must only be called once.
    bool SetContent(const CastMessage& message_proto);

    net::CompletionCallback callback;
    scoped_refptr<net::DrainableIOBuffer> io_buffer;
  };
  // Queue of pending writes. The message at the front of the queue is the one
  // being written.
  std::queue<WriteRequest> write_queue_;

  FRIEND_TEST_ALL_PREFIXES(CastSocketTest, TestCastURLs);
  FRIEND_TEST_ALL_PREFIXES(CastSocketTest, TestRead);
  FRIEND_TEST_ALL_PREFIXES(CastSocketTest, TestReadMany);
  DISALLOW_COPY_AND_ASSIGN(CastSocket);
};

}  // namespace cast_channel
}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CAST_CHANNEL_CAST_SOCKET_H_
