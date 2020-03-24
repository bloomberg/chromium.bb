// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/standalone_receiver/cast_agent.h"

#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "cast/standalone_receiver/cast_socket_message_port.h"
#include "cast/standalone_receiver/private_key_der.h"
#include "cast/streaming/constants.h"
#include "cast/streaming/offer_messages.h"
#include "platform/base/tls_credentials.h"
#include "platform/base/tls_listen_options.h"
#include "util/crypto/certificate_utils.h"
#include "util/json/json_serialization.h"
#include "util/logging.h"

namespace openscreen {
namespace cast {
namespace {

constexpr int kDefaultMaxBacklogSize = 64;
const TlsListenOptions kDefaultListenOptions{kDefaultMaxBacklogSize};

constexpr int kThreeDaysInSeconds = 3 * 24 * 60 * 60;
constexpr auto kCertificateDuration = std::chrono::seconds(kThreeDaysInSeconds);

// Generates a valid set of credentials for use with the TLS Server socket,
// including a generated X509 certificate generated from the static private key
// stored in private_key_der.h. The certificate is valid for
// kCertificateDuration from when this function is called.
ErrorOr<TlsCredentials> CreateCredentials(const IPEndpoint& endpoint) {
  ErrorOr<bssl::UniquePtr<EVP_PKEY>> private_key =
      ImportRSAPrivateKey(kPrivateKeyDer.data(), kPrivateKeyDer.size());
  OSP_CHECK(private_key);

  ErrorOr<bssl::UniquePtr<X509>> cert = CreateSelfSignedX509Certificate(
      endpoint.ToString(), kCertificateDuration, *private_key.value());
  if (!cert) {
    return cert.error();
  }

  auto cert_bytes = ExportX509CertificateToDer(*cert.value());
  if (!cert_bytes) {
    return cert_bytes.error();
  }

  // TODO(jophba): either refactor the TLS server socket to use the public key
  // and add a valid key here, or remove from the TlsCredentials struct.
  return TlsCredentials(
      std::vector<uint8_t>(kPrivateKeyDer.begin(), kPrivateKeyDer.end()),
      std::vector<uint8_t>{}, std::move(cert_bytes.value()));
}

}  // namespace

CastAgent::CastAgent(TaskRunner* task_runner, InterfaceInfo interface)
    : task_runner_(task_runner) {
  // Create the Environment that holds the required injected dependencies
  // (clock, task runner) used throughout the system, and owns the UDP socket
  // over which all communication occurs with the Sender.
  IPEndpoint receive_endpoint{IPAddress::kV4LoopbackAddress, kDefaultCastPort};
  receive_endpoint.address = interface.GetIpAddressV4()
                                 ? interface.GetIpAddressV4()
                                 : interface.GetIpAddressV6();
  OSP_DCHECK(receive_endpoint.address);
  environment_ = std::make_unique<Environment>(&Clock::now, task_runner_,
                                               receive_endpoint);
  receive_endpoint_ = std::move(receive_endpoint);
}

CastAgent::~CastAgent() = default;

Error CastAgent::Start() {
  OSP_DCHECK(!current_session_);

  task_runner_->PostTask(
      [this] { this->wake_lock_ = ScopedWakeLock::Create(); });

  // TODO(jophba): add command line argument for setting the private key.
  ErrorOr<TlsCredentials> credentials = CreateCredentials(receive_endpoint_);
  if (!credentials) {
    return credentials.error();
  }

  // TODO(jophba, rwkeane): begin discovery process before creating TLS
  // connection factory instance.
  socket_factory_ =
      std::make_unique<ReceiverSocketFactory>(this, &message_port_);
  task_runner_->PostTask([this, creds = std::move(credentials.value())] {
    connection_factory_ = TlsConnectionFactory::CreateFactory(
        socket_factory_.get(), task_runner_);
    connection_factory_->SetListenCredentials(creds);
    connection_factory_->Listen(receive_endpoint_, kDefaultListenOptions);
  });

  OSP_LOG_INFO << "Listening for connections at: " << receive_endpoint_;
  return Error::None();
}

Error CastAgent::Stop() {
  controller_.reset();
  current_session_.reset();
  return Error::None();
}

void CastAgent::OnConnected(ReceiverSocketFactory* factory,
                            const IPEndpoint& endpoint,
                            std::unique_ptr<CastSocket> socket) {
  OSP_DCHECK(factory);

  if (current_session_) {
    OSP_LOG_WARN << "Already connected, dropping peer at: " << endpoint;
    return;
  }

  OSP_LOG_INFO << "Received connection from peer at: " << endpoint;
  message_port_.SetSocket(std::move(socket));
  controller_ =
      std::make_unique<StreamingPlaybackController>(task_runner_, this);
  current_session_ = std::make_unique<ReceiverSession>(
      controller_.get(), environment_.get(), &message_port_,
      ReceiverSession::Preferences{});
}

void CastAgent::OnError(ReceiverSocketFactory* factory, Error error) {
  OSP_LOG_ERROR << "Cast agent received socket factory error: " << error;
}

// Currently we don't do anything with the receiver output--the session
// is automatically linked to the playback controller when it is constructed, so
// we don't actually have to interface with the receivers. If we end up caring
// about the receiver configurations we will have to handle OnNegotiated here.
void CastAgent::OnNegotiated(const ReceiverSession* session,
                             ReceiverSession::ConfiguredReceivers receivers) {
  OSP_LOG_INFO << "Successfully negotiated with sender.";
}

void CastAgent::OnConfiguredReceiversDestroyed(const ReceiverSession* session) {
  OSP_LOG_INFO << "Receiver instances destroyed.";
}

// Currently, we just kill the session if an error is encountered.
void CastAgent::OnError(const ReceiverSession* session, Error error) {
  OSP_LOG_ERROR << "Cast agent received receiver session error: " << error;
  current_session_.reset();
}

void CastAgent::OnPlaybackError(StreamingPlaybackController* controller,
                                Error error) {
  OSP_LOG_ERROR << "Cast agent received playback error: " << error;
  current_session_.reset();
}

}  // namespace cast
}  // namespace openscreen
