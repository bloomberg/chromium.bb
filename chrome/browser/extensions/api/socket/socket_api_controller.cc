// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_writer.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/socket/socket_api_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/rand_callback.h"
#include "net/udp/datagram_socket.h"
#include "net/udp/udp_client_socket.h"
#include "net/udp/udp_socket.h"

using namespace net;

namespace extensions {

// A Socket wraps a low-level socket and includes housekeeping information that
// we need to manage it in the context of an extension.
class Socket {
 public:
  explicit Socket(const Profile* profile, const std::string& src_extension_id,
                  const GURL& src_url);
  ~Socket();

  bool Connect(const net::IPEndPoint& ip_end_point);
  void Close();
  int Write(const std::string message);

 private:
  // TODO(miket): this metadata will enable us to pass events back to the
  // extension that created this Socket.
  const Profile* profile_;
  int id_;
  std::string src_extension_id_;
  GURL src_url_;

  net::UDPClientSocket* udp_client_socket_;
  bool is_connected_;
  net::OldCompletionCallbackImpl<Socket> io_callback_;

  // A callback required by UDPClientSocket::Write().
  void OnIOComplete(int result);
};

Socket::Socket(const Profile* profile, const std::string& src_extension_id,
               const GURL& src_url)
    : profile_(profile),
      src_extension_id_(src_extension_id),
      src_url_(src_url),
      udp_client_socket_(new UDPClientSocket(
          DatagramSocket::DEFAULT_BIND,
          RandIntCallback(),
          NULL,
          NetLog::Source())),
      is_connected_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          io_callback_(this, &Socket::OnIOComplete)) {}

Socket::~Socket() {
  if (is_connected_) {
    Close();
  }
}

void Socket::OnIOComplete(int result) {
  // We don't need to do anything.
}

bool Socket::Connect(const net::IPEndPoint& ip_end_point) {
  is_connected_ = udp_client_socket_->Connect(ip_end_point) == net::OK;
  return is_connected_;
}

void Socket::Close() {
  is_connected_ = false;
  udp_client_socket_->Close();
}

int Socket::Write(const std::string message) {
  int length = message.length();
  scoped_refptr<StringIOBuffer> io_buffer(new StringIOBuffer(message));
  scoped_refptr<DrainableIOBuffer> buffer(
      new DrainableIOBuffer(io_buffer, length));

  int bytes_sent = 0;
  while (buffer->BytesRemaining()) {
    int rv = udp_client_socket_->Write(buffer,
                                       buffer->BytesRemaining(),
                                       &io_callback_);
    if (rv <= 0) {
      // We pass all errors, including ERROR_IO_PENDING, back to the caller.
      return bytes_sent > 0 ? bytes_sent : rv;
    }
    bytes_sent += rv;
    buffer->DidConsume(rv);
  }
  return bytes_sent;
}

SocketController* SocketController::GetInstance() {
  return Singleton<SocketController>::get();
}

SocketController::SocketController() : next_socket_id_(1) {}

SocketController::~SocketController() {
  STLDeleteValues(&socket_map_);
}

Socket* SocketController::GetSocket(int socket_id) {
  // TODO(miket): we should verify that the extension asking for the
  // socket is the same one that created it.
  SocketMap::iterator i = socket_map_.find(socket_id);
  if (i != socket_map_.end())
    return i->second;
  return NULL;
}

int SocketController::CreateUdp(const Profile* profile,
                                const std::string& extension_id,
                                const GURL& src_url) {
  Socket* socket = new Socket(profile, extension_id, src_url);
  CHECK(socket);
  socket_map_[next_socket_id_] = socket;
  return next_socket_id_++;
}

bool SocketController::DestroyUdp(int socket_id) {
  Socket* socket = GetSocket(socket_id);
  if (!socket)
    return false;
  delete socket;
  socket_map_.erase(socket_id);
  return true;
}

// TODO(miket): it *might* be nice to be able to resolve DNS. I am not putting
// in interesting error reporting for this method because we clearly can't
// leave experimental without DNS resolution.
//
// static
bool SocketController::CreateIPEndPoint(const std::string address, int port,
                                        net::IPEndPoint* ip_end_point) {
  net::IPAddressNumber ip_number;
  bool rv = net::ParseIPLiteralToNumber(address, &ip_number);
  if (!rv)
    return false;
  *ip_end_point = net::IPEndPoint(ip_number, port);
  return true;
}

bool SocketController::ConnectUdp(int socket_id, const std::string address,
                                  int port) {
  Socket* socket = GetSocket(socket_id);
  if (!socket)
    return false;
  net::IPEndPoint ip_end_point;
  if (!CreateIPEndPoint(address, port, &ip_end_point))
    return false;
  return socket->Connect(ip_end_point);
}

void SocketController::CloseUdp(int socket_id) {
  Socket* socket = GetSocket(socket_id);
  if (socket)
    socket->Close();
}

int SocketController::WriteUdp(int socket_id, const std::string message) {
  Socket* socket = GetSocket(socket_id);
  if (!socket) {
    return -1;
  }
  return socket->Write(message);
}

}  // namespace extensions
