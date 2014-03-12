// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SOCKET_TCP_SOCKET_H_
#define EXTENSIONS_BROWSER_API_SOCKET_TCP_SOCKET_H_

#include <string>

#include "extensions/browser/api/socket/socket.h"

// This looks like it should be forward-declarable, but it does some tricky
// moves that make it easier to just include it.
#include "net/socket/tcp_client_socket.h"
#include "net/socket/tcp_server_socket.h"

namespace net {
class Socket;
}

namespace extensions {

class TCPSocket : public Socket {
 public:
  explicit TCPSocket(const std::string& owner_extension_id);
  TCPSocket(net::TCPClientSocket* tcp_client_socket,
            const std::string& owner_extension_id,
            bool is_connected = false);

  virtual ~TCPSocket();

  virtual void Connect(const std::string& address,
                       int port,
                       const CompletionCallback& callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual int Bind(const std::string& address, int port) OVERRIDE;
  virtual void Read(int count, const ReadCompletionCallback& callback) OVERRIDE;
  virtual void RecvFrom(int count,
                        const RecvFromCompletionCallback& callback) OVERRIDE;
  virtual void SendTo(scoped_refptr<net::IOBuffer> io_buffer,
                      int byte_count,
                      const std::string& address,
                      int port,
                      const CompletionCallback& callback) OVERRIDE;
  virtual bool SetKeepAlive(bool enable, int delay) OVERRIDE;
  virtual bool SetNoDelay(bool no_delay) OVERRIDE;
  virtual int Listen(const std::string& address,
                     int port,
                     int backlog,
                     std::string* error_msg) OVERRIDE;
  virtual void Accept(const AcceptCompletionCallback& callback) OVERRIDE;

  virtual bool IsConnected() OVERRIDE;

  virtual bool GetPeerAddress(net::IPEndPoint* address) OVERRIDE;
  virtual bool GetLocalAddress(net::IPEndPoint* address) OVERRIDE;
  virtual Socket::SocketType GetSocketType() const OVERRIDE;

  static TCPSocket* CreateSocketForTesting(
      net::TCPClientSocket* tcp_client_socket,
      const std::string& owner_extension_id,
      bool is_connected = false);
  static TCPSocket* CreateServerSocketForTesting(
      net::TCPServerSocket* tcp_server_socket,
      const std::string& owner_extension_id);

 protected:
  virtual int WriteImpl(net::IOBuffer* io_buffer,
                        int io_buffer_size,
                        const net::CompletionCallback& callback) OVERRIDE;

 private:
  void RefreshConnectionStatus();
  void OnConnectComplete(int result);
  void OnReadComplete(scoped_refptr<net::IOBuffer> io_buffer, int result);
  void OnAccept(int result);

  TCPSocket(net::TCPServerSocket* tcp_server_socket,
            const std::string& owner_extension_id);

  scoped_ptr<net::TCPClientSocket> socket_;
  scoped_ptr<net::TCPServerSocket> server_socket_;

  enum SocketMode { UNKNOWN = 0, CLIENT, SERVER, };
  SocketMode socket_mode_;

  CompletionCallback connect_callback_;

  ReadCompletionCallback read_callback_;

  scoped_ptr<net::StreamSocket> accept_socket_;
  AcceptCompletionCallback accept_callback_;
};

// TCP Socket instances from the "sockets.tcp" namespace. These are regular
// socket objects with additional properties related to the behavior defined in
// the "sockets.tcp" namespace.
class ResumableTCPSocket : public TCPSocket {
 public:
  explicit ResumableTCPSocket(const std::string& owner_extension_id);
  explicit ResumableTCPSocket(net::TCPClientSocket* tcp_client_socket,
                              const std::string& owner_extension_id,
                              bool is_connected);

  // Overriden from ApiResource
  virtual bool IsPersistent() const OVERRIDE;

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  bool persistent() const { return persistent_; }
  void set_persistent(bool persistent) { persistent_ = persistent; }

  int buffer_size() const { return buffer_size_; }
  void set_buffer_size(int buffer_size) { buffer_size_ = buffer_size; }

  bool paused() const { return paused_; }
  void set_paused(bool paused) { paused_ = paused; }

 private:
  friend class ApiResourceManager<ResumableTCPSocket>;
  static const char* service_name() { return "ResumableTCPSocketManager"; }

  // Application-defined string - see sockets_tcp.idl.
  std::string name_;
  // Flag indicating whether the socket is left open when the application is
  // suspended - see sockets_tcp.idl.
  bool persistent_;
  // The size of the buffer used to receive data - see sockets_tcp.idl.
  int buffer_size_;
  // Flag indicating whether a connected socket blocks its peer from sending
  // more data - see sockets_tcp.idl.
  bool paused_;
};

// TCP Socket instances from the "sockets.tcpServer" namespace. These are
// regular socket objects with additional properties related to the behavior
// defined in the "sockets.tcpServer" namespace.
class ResumableTCPServerSocket : public TCPSocket {
 public:
  explicit ResumableTCPServerSocket(const std::string& owner_extension_id);

  // Overriden from ApiResource
  virtual bool IsPersistent() const OVERRIDE;

  const std::string& name() const { return name_; }
  void set_name(const std::string& name) { name_ = name; }

  bool persistent() const { return persistent_; }
  void set_persistent(bool persistent) { persistent_ = persistent; }

  bool paused() const { return paused_; }
  void set_paused(bool paused) { paused_ = paused; }

 private:
  friend class ApiResourceManager<ResumableTCPServerSocket>;
  static const char* service_name() {
    return "ResumableTCPServerSocketManager";
  }

  // Application-defined string - see sockets_tcp_server.idl.
  std::string name_;
  // Flag indicating whether the socket is left open when the application is
  // suspended - see sockets_tcp_server.idl.
  bool persistent_;
  // Flag indicating whether a connected socket blocks its peer from sending
  // more data - see sockets_tcp_server.idl.
  bool paused_;
};

}  //  namespace extensions

#endif  // EXTENSIONS_BROWSER_API_SOCKET_TCP_SOCKET_H_
