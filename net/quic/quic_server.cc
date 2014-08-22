// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_server.h"

#include <string.h>

#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/quic/congestion_control/tcp_receiver.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_crypto_stream.h"
#include "net/quic/quic_data_reader.h"
#include "net/quic/quic_dispatcher.h"
#include "net/quic/quic_in_memory_cache.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_server_packet_writer.h"
#include "net/udp/udp_server_socket.h"

namespace net {

namespace {

const char kSourceAddressTokenSecret[] = "secret";

// Allocate some extra space so we can send an error if the client goes over
// the limit.
const int kReadBufferSize = 2 * kMaxPacketSize;

const uint32 kServerInitialFlowControlWindow = 100 * kMaxPacketSize;

} // namespace

QuicServer::QuicServer(const QuicConfig& config,
                       const QuicVersionVector& supported_versions)
    : helper_(base::MessageLoop::current()->message_loop_proxy().get(),
              &clock_,
              QuicRandom::GetInstance()),
      config_(config),
      crypto_config_(kSourceAddressTokenSecret, QuicRandom::GetInstance()),
      supported_versions_(supported_versions),
      read_pending_(false),
      synchronous_read_count_(0),
      read_buffer_(new IOBufferWithSize(kReadBufferSize)),
      weak_factory_(this) {
  Initialize();
}

void QuicServer::Initialize() {
  // Initialize the in memory cache now.
  QuicInMemoryCache::GetInstance();

  scoped_ptr<CryptoHandshakeMessage> scfg(
      crypto_config_.AddDefaultConfig(helper_.GetRandomGenerator(),
                                      helper_.GetClock(),
                                      QuicCryptoServerConfig::ConfigOptions()));

  config_.SetInitialCongestionWindowToSend(kServerInitialFlowControlWindow);
}

QuicServer::~QuicServer() {
}

int QuicServer::Listen(const IPEndPoint& address) {
  scoped_ptr<UDPServerSocket> socket(
      new UDPServerSocket(&net_log_, NetLog::Source()));

  socket->AllowAddressReuse();

  int rc = socket->Listen(address);
  if (rc < 0) {
    LOG(ERROR) << "Listen() failed: " << ErrorToString(rc);
    return rc;
  }

  // These send and receive buffer sizes are sized for a single connection,
  // because the default usage of QuicServer is as a test server with one or
  // two clients.  Adjust higher for use with many clients.
  rc = socket->SetReceiveBufferSize(TcpReceiver::kReceiveWindowTCP);
  if (rc < 0) {
    LOG(ERROR) << "SetReceiveBufferSize() failed: " << ErrorToString(rc);
    return rc;
  }

  rc = socket->SetSendBufferSize(20 * kMaxPacketSize);
  if (rc < 0) {
    LOG(ERROR) << "SetSendBufferSize() failed: " << ErrorToString(rc);
    return rc;
  }

  rc = socket->GetLocalAddress(&server_address_);
  if (rc < 0) {
    LOG(ERROR) << "GetLocalAddress() failed: " << ErrorToString(rc);
    return rc;
  }

  DVLOG(1) << "Listening on " << server_address_.ToString();

  socket_.swap(socket);

  dispatcher_.reset(
      new QuicDispatcher(config_,
                         crypto_config_,
                         supported_versions_,
                         new QuicDispatcher::DefaultPacketWriterFactory(),
                         &helper_));
  QuicServerPacketWriter* writer = new QuicServerPacketWriter(
      socket_.get(),
      dispatcher_.get());
  dispatcher_->Initialize(writer);

  StartReading();

  return OK;
}

void QuicServer::Shutdown() {
  // Before we shut down the epoll server, give all active sessions a chance to
  // notify clients that they're closing.
  dispatcher_->Shutdown();

  socket_->Close();
  socket_.reset();
}

void QuicServer::StartReading() {
  if (read_pending_) {
    return;
  }
  read_pending_ = true;

  int result = socket_->RecvFrom(
      read_buffer_.get(),
      read_buffer_->size(),
      &client_address_,
      base::Bind(&QuicServer::OnReadComplete, base::Unretained(this)));

  if (result == ERR_IO_PENDING) {
    synchronous_read_count_ = 0;
    return;
  }

  if (++synchronous_read_count_ > 32) {
    synchronous_read_count_ = 0;
    // Schedule the processing through the message loop to 1) prevent infinite
    // recursion and 2) avoid blocking the thread for too long.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&QuicServer::OnReadComplete,
                   weak_factory_.GetWeakPtr(),
                   result));
  } else {
    OnReadComplete(result);
  }
}

void QuicServer::OnReadComplete(int result) {
  read_pending_ = false;
  if (result == 0)
    result = ERR_CONNECTION_CLOSED;

  if (result < 0) {
    LOG(ERROR) << "QuicServer read failed: " << ErrorToString(result);
    Shutdown();
    return;
  }

  QuicEncryptedPacket packet(read_buffer_->data(), result, false);
  dispatcher_->ProcessPacket(server_address_, client_address_, packet);

  StartReading();
}

}  // namespace net
