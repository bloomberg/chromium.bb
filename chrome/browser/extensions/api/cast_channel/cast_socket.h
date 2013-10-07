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
class TCPClientSocket;
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
    virtual void OnStringMessage(const CastSocket* socket,
                                 const std::string& namespace_,
                                 const std::string& data) = 0;
   protected:
    virtual ~Delegate() {}
  };

  // Creates a new CastSocket to |url|. |owner_extension_id| is the id of the
  // extension that opened the socket.
  CastSocket(const std::string& owner_extension_id,
             const GURL& url, CastSocket::Delegate* delegate,
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

  // Sends a message with a string payload over a connected channel. The
  // channel must be in READY_STATE_OPEN.
  virtual void SendString(const std::string& name_space,
                          const std::string& value,
                          const net::CompletionCallback& callback);

  // Closes the channel. On completion, the channel will be in
  // READY_STATE_CLOSED.
  virtual void Close(const net::CompletionCallback& callback);

  // Fills |channel_info| with the status of this channel.
  void FillChannelInfo(ChannelInfo* channel_info) const;

 protected:
  // Factory method for sockets.
  virtual net::TCPClientSocket* CreateSocket(const net::AddressList& addresses,
                                             net::NetLog* net_log,
                                             const net::NetLog::Source& source);

 private:
  friend class ApiResourceManager<CastSocket>;
  static const char* service_name() {
    return "CastSocketManager";
  }

  // Verifies that the URL is a valid cast:// or casts:// URL and sets url_ to
  // the result.
  bool ParseChannelUrl(const GURL& url);

  // Called when the socket is connected.
  void OnConnectComplete(int result);

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
  // Parses the message held in body_read_buffer_ and returns true on success.
  // Notifies |delegate_| if a message was extracted from the buffer.
  bool ParseMessage();

  // Creates a protocol buffer to hold |namespace_| and |message| and serializes
  // it (with a header) to |message_data|.
  static bool SerializeStringMessage(const std::string& name_space,
                                     const std::string& message,
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

  // Owned ptr to the underlying socket.
  //
  // NOTE(mfoltz): We'll have to refactor this to allow substitution of an
  // SSLClientSocket, since the APIs are different.
  scoped_ptr<net::TCPClientSocket> socket_;

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
    // Sets the content of the message. Must only be called once.
    bool SetStringMessage(const std::string& name_space,
                          const std::string& message);

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
