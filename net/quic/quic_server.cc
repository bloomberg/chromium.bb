// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_server.h"

#include <string.h>

#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/quic/crypto/crypto_handshake.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/quic_crypto_stream.h"
#include "net/quic/quic_data_reader.h"
#include "net/quic/quic_per_connection_packet_writer.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_server_packet_writer.h"
#include "net/tools/quic/quic_dispatcher.h"
#include "net/udp/udp_server_socket.h"

namespace net {

using tools::QuicDispatcher;

namespace {

const char kSourceAddressTokenSecret[] = "secret";

// Allocate some extra space so we can send an error if the client goes over
// the limit.
const int kReadBufferSize = 2 * kMaxPacketSize;

// A packet writer factory which wraps a shared QuicServerPacketWriter
// inside of a QuicPerConnectionPacketWriter. Instead of checking that
// the shared_writer is the expected writer, this could instead cast
// from QuicPacketWriter to QuicServerPacketWriter.
class CustomPacketWriterFactory : public QuicDispatcher::PacketWriterFactory {
 public:
  ~CustomPacketWriterFactory() override {}

  QuicPacketWriter* Create(QuicPacketWriter* writer,
                           QuicConnection* connection) override {
    if (writer == nullptr) {
      LOG(DFATAL) << "shared_writer not initialized";
      return nullptr;
    }
    if (writer != shared_writer_) {
      LOG(DFATAL) << "writer mismatch";
      return nullptr;
    }
    return new QuicPerConnectionPacketWriter(shared_writer_, connection);
  }

  void set_shared_writer(QuicServerPacketWriter* shared_writer) {
    shared_writer_ = shared_writer;
  }

 private:
  QuicServerPacketWriter* shared_writer_;  // Not owned.
};

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
#if MMSG_MORE
  use_recvmmsg_ = true;
#endif

  // If an initial flow control window has not explicitly been set, then use a
  // sensible value for a server: 1 MB for session, 64 KB for each stream.
  const uint32 kInitialSessionFlowControlWindow = 1 * 1024 * 1024;  // 1 MB
  const uint32 kInitialStreamFlowControlWindow = 64 * 1024;         // 64 KB
  if (config_.GetInitialStreamFlowControlWindowToSend() ==
      kMinimumFlowControlSendWindow) {
    config_.SetInitialStreamFlowControlWindowToSend(
        kInitialStreamFlowControlWindow);
  }
  if (config_.GetInitialSessionFlowControlWindowToSend() ==
      kMinimumFlowControlSendWindow) {
    config_.SetInitialSessionFlowControlWindowToSend(
        kInitialSessionFlowControlWindow);
  }

  scoped_ptr<CryptoHandshakeMessage> scfg(
      crypto_config_.AddDefaultConfig(
          helper_.GetRandomGenerator(), helper_.GetClock(),
          QuicCryptoServerConfig::ConfigOptions()));
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
  rc = socket->SetReceiveBufferSize(
      static_cast<int32>(kDefaultSocketReceiveBuffer));
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

  CustomPacketWriterFactory* factory = new CustomPacketWriterFactory();
  dispatcher_.reset(
      new QuicDispatcher(config_,
                         crypto_config_,
                         supported_versions_,
                         factory,
                         &helper_));
  QuicServerPacketWriter* writer = new QuicServerPacketWriter(
      socket_.get(),
      dispatcher_.get());
  factory->set_shared_writer(writer);
  dispatcher_->InitializeWithWriter(writer);

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
