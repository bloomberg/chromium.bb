// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <openssl/evp.h>
#include <openssl/mem.h>

#include <atomic>
#include <chrono>

#include "cast/common/certificate/cast_trust_store.h"
#include "cast/common/certificate/testing/test_helpers.h"
#include "cast/common/channel/connection_namespace_handler.h"
#include "cast/common/channel/message_util.h"
#include "cast/common/channel/virtual_connection_manager.h"
#include "cast/common/channel/virtual_connection_router.h"
#include "cast/common/public/cast_socket.h"
#include "cast/receiver/channel/device_auth_namespace_handler.h"
#include "cast/receiver/channel/testing/device_auth_test_helpers.h"
#include "cast/receiver/public/receiver_socket_factory.h"
#include "cast/sender/public/sender_socket_factory.h"
#include "gtest/gtest.h"
#include "platform/api/serial_delete_ptr.h"
#include "platform/api/tls_connection_factory.h"
#include "platform/base/tls_connect_options.h"
#include "platform/base/tls_credentials.h"
#include "platform/base/tls_listen_options.h"
#include "platform/impl/logging.h"
#include "platform/impl/network_interface.h"
#include "platform/impl/platform_client_posix.h"
#include "util/crypto/certificate_utils.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace cast {

using std::chrono::milliseconds;

constexpr auto kCertificateDuration = std::chrono::hours(24);

class SenderSocketsClient final
    : public SenderSocketFactory::Client,
      public VirtualConnectionRouter::SocketErrorHandler {
 public:
  SenderSocketsClient(VirtualConnectionRouter* router) : router_(router) {}
  ~SenderSocketsClient() = default;

  CastSocket* socket() const { return socket_; }

  // SenderSocketFactory::Client overrides.
  void OnConnected(SenderSocketFactory* factory,
                   const IPEndpoint& endpoint,
                   std::unique_ptr<CastSocket> socket) {
    OSP_CHECK(!socket_);
    OSP_LOG_INFO << "\tSender connected to endpoint: " << endpoint;
    socket_ = socket.get();
    router_->TakeSocket(this, std::move(socket));
  }

  void OnError(SenderSocketFactory* factory,
               const IPEndpoint& endpoint,
               Error error) override {
    OSP_NOTREACHED() << error;
  }

  // VirtualConnectionRouter::SocketErrorHandler overrides.
  void OnClose(CastSocket* socket) override {}
  void OnError(CastSocket* socket, Error error) override {
    OSP_NOTREACHED() << error;
  }

 private:
  VirtualConnectionRouter* const router_;
  std::atomic<CastSocket*> socket_{nullptr};
};

class ReceiverSocketsClient final
    : public ReceiverSocketFactory::Client,
      public VirtualConnectionRouter::SocketErrorHandler {
 public:
  explicit ReceiverSocketsClient(VirtualConnectionRouter* router)
      : router_(router) {}
  ~ReceiverSocketsClient() = default;

  const IPEndpoint& endpoint() const { return endpoint_; }
  CastSocket* socket() const { return socket_; }

  // ReceiverSocketFactory::Client overrides.
  void OnConnected(ReceiverSocketFactory* factory,
                   const IPEndpoint& endpoint,
                   std::unique_ptr<CastSocket> socket) override {
    OSP_CHECK(!socket_);
    OSP_LOG_INFO << "\tReceiver got connection from endpoint: " << endpoint;
    endpoint_ = endpoint;
    socket_ = socket.get();
    router_->TakeSocket(this, std::move(socket));
  }

  void OnError(ReceiverSocketFactory* factory, Error error) override {
    OSP_NOTREACHED() << error;
  }

  // VirtualConnectionRouter::SocketErrorHandler overrides.
  void OnClose(CastSocket* socket) override {}
  void OnError(CastSocket* socket, Error error) override {
    OSP_NOTREACHED() << error;
  }

 private:
  VirtualConnectionRouter* router_;
  IPEndpoint endpoint_;
  std::atomic<CastSocket*> socket_{nullptr};
};

class CastSocketE2ETest : public ::testing::Test {
 public:
  void SetUp() override {
    PlatformClientPosix::Create(milliseconds{10}, Clock::duration{0});
    task_runner_ = PlatformClientPosix::GetInstance()->GetTaskRunner();

    sender_router_ = MakeSerialDelete<VirtualConnectionRouter>(
        task_runner_, &sender_vc_manager_);
    sender_client_ =
        std::make_unique<SenderSocketsClient>(sender_router_.get());
    sender_factory_ = MakeSerialDelete<SenderSocketFactory>(
        task_runner_, sender_client_.get(), task_runner_);
    sender_tls_factory_ = SerialDeletePtr<TlsConnectionFactory>(
        task_runner_,
        TlsConnectionFactory::CreateFactory(sender_factory_.get(), task_runner_)
            .release());
    sender_factory_->set_factory(sender_tls_factory_.get());

    // NOTE: Device cert chain generation.
    bssl::UniquePtr<EVP_PKEY> root_key = GenerateRsaKeyPair();
    bssl::UniquePtr<EVP_PKEY> intermediate_key = GenerateRsaKeyPair();
    bssl::UniquePtr<EVP_PKEY> device_key = GenerateRsaKeyPair();
    ASSERT_TRUE(root_key);
    ASSERT_TRUE(intermediate_key);
    ASSERT_TRUE(device_key);

    ErrorOr<bssl::UniquePtr<X509>> root_cert_or_error =
        CreateSelfSignedX509CertificateForTest(
            "Cast Root CA", kCertificateDuration, *root_key,
            GetWallTimeSinceUnixEpoch(), true);
    ASSERT_TRUE(root_cert_or_error);
    bssl::UniquePtr<X509> root_cert = std::move(root_cert_or_error.value());

    ErrorOr<bssl::UniquePtr<X509>> intermediate_cert_or_error =
        CreateSelfSignedX509CertificateForTest(
            "Cast Intermediate", kCertificateDuration, *intermediate_key,
            GetWallTimeSinceUnixEpoch(), true, root_cert.get(), root_key.get());
    ASSERT_TRUE(intermediate_cert_or_error);
    bssl::UniquePtr<X509> intermediate_cert =
        std::move(intermediate_cert_or_error.value());

    ErrorOr<bssl::UniquePtr<X509>> device_cert_or_error =
        CreateSelfSignedX509CertificateForTest(
            "Test Device", kCertificateDuration, *device_key,
            GetWallTimeSinceUnixEpoch(), false, intermediate_cert.get(),
            intermediate_key.get());
    ASSERT_TRUE(device_cert_or_error);
    bssl::UniquePtr<X509> device_cert = std::move(device_cert_or_error.value());

    // NOTE: Device cert chain plumbing + serialization.
    receiver_creds_provider_.device_creds.private_key = std::move(device_key);

    int cert_length = i2d_X509(device_cert.get(), nullptr);
    std::string cert_serial(cert_length, 0);
    uint8_t* out = reinterpret_cast<uint8_t*>(&cert_serial[0]);
    i2d_X509(device_cert.get(), &out);
    receiver_creds_provider_.device_creds.certs.emplace_back(
        std::move(cert_serial));

    cert_length = i2d_X509(intermediate_cert.get(), nullptr);
    cert_serial.resize(cert_length);
    out = reinterpret_cast<uint8_t*>(&cert_serial[0]);
    i2d_X509(intermediate_cert.get(), &out);
    receiver_creds_provider_.device_creds.certs.emplace_back(
        std::move(cert_serial));

    cert_length = i2d_X509(root_cert.get(), nullptr);
    std::vector<uint8_t> trust_anchor_der(cert_length);
    out = &trust_anchor_der[0];
    i2d_X509(root_cert.get(), &out);
    CastTrustStore::CreateInstanceForTest(trust_anchor_der);

    // NOTE: TLS key pair + certificate generation.
    bssl::UniquePtr<EVP_PKEY> tls_key = GenerateRsaKeyPair();
    ASSERT_EQ(EVP_PKEY_id(tls_key.get()), EVP_PKEY_RSA);
    ErrorOr<bssl::UniquePtr<X509>> tls_cert_or_error =
        CreateSelfSignedX509Certificate("Test Device TLS", kCertificateDuration,
                                        *tls_key, GetWallTimeSinceUnixEpoch());
    ASSERT_TRUE(tls_cert_or_error);
    bssl::UniquePtr<X509> tls_cert = std::move(tls_cert_or_error.value());

    // NOTE: TLS private key serialization.
    RSA* rsa_key = EVP_PKEY_get0_RSA(tls_key.get());
    size_t pkey_len = 0;
    uint8_t* pkey_bytes = nullptr;
    ASSERT_TRUE(RSA_private_key_to_bytes(&pkey_bytes, &pkey_len, rsa_key));
    ASSERT_GT(pkey_len, 0u);
    std::vector<uint8_t> tls_key_serial(pkey_bytes, pkey_bytes + pkey_len);
    OPENSSL_free(pkey_bytes);
    receiver_tls_creds_.der_rsa_private_key = std::move(tls_key_serial);

    // NOTE: TLS public key serialization.
    pkey_len = 0;
    pkey_bytes = nullptr;
    ASSERT_TRUE(RSA_public_key_to_bytes(&pkey_bytes, &pkey_len, rsa_key));
    ASSERT_GT(pkey_len, 0u);
    std::vector<uint8_t> tls_pub_serial(pkey_bytes, pkey_bytes + pkey_len);
    OPENSSL_free(pkey_bytes);
    receiver_tls_creds_.der_rsa_public_key = std::move(tls_pub_serial);

    // NOTE: TLS cert serialization.
    cert_length = 0;
    cert_length = i2d_X509(tls_cert.get(), nullptr);
    ASSERT_GT(cert_length, 0);
    std::vector<uint8_t> tls_cert_serial(cert_length);
    out = &tls_cert_serial[0];
    i2d_X509(tls_cert.get(), &out);
    receiver_creds_provider_.tls_cert_der = tls_cert_serial;
    receiver_tls_creds_.der_x509_cert = std::move(tls_cert_serial);

    auth_handler_ = MakeSerialDelete<DeviceAuthNamespaceHandler>(
        task_runner_, &receiver_creds_provider_);
    receiver_router_ = MakeSerialDelete<VirtualConnectionRouter>(
        task_runner_, &receiver_vc_manager_);
    receiver_router_->AddHandlerForLocalId(kPlatformReceiverId,
                                           auth_handler_.get());
    receiver_client_ =
        std::make_unique<ReceiverSocketsClient>(receiver_router_.get());
    receiver_factory_ = MakeSerialDelete<ReceiverSocketFactory>(
        task_runner_, receiver_client_.get(), receiver_router_.get());

    receiver_tls_factory_ = SerialDeletePtr<TlsConnectionFactory>(
        task_runner_, TlsConnectionFactory::CreateFactory(
                          receiver_factory_.get(), task_runner_)
                          .release());
  }

  void TearDown() override {
    OSP_LOG_INFO << "Shutting down";
    sender_router_.reset();
    receiver_router_.reset();
    receiver_tls_factory_.reset();
    receiver_factory_.reset();
    auth_handler_.reset();
    sender_tls_factory_.reset();
    sender_factory_.reset();
    CastTrustStore::ResetInstance();
    PlatformClientPosix::ShutDown();
  }

 protected:
  IPAddress GetLoopbackV4Address() {
    absl::optional<InterfaceInfo> loopback = GetLoopbackInterfaceForTesting();
    OSP_CHECK(loopback);
    IPAddress address = loopback->GetIpAddressV4();
    OSP_CHECK(address);
    return address;
  }

  IPAddress GetLoopbackV6Address() {
    absl::optional<InterfaceInfo> loopback = GetLoopbackInterfaceForTesting();
    OSP_CHECK(loopback);
    IPAddress address = loopback->GetIpAddressV6();
    return address;
  }

  void WaitForCastSocket() {
    int attempts = 1;
    constexpr int kMaxAttempts = 8;
    constexpr std::chrono::milliseconds kSocketWaitDelay(250);
    do {
      OSP_LOG_INFO << "\tChecking for CastSocket, attempt " << attempts << "/"
                   << kMaxAttempts;
      if (sender_client_->socket()) {
        break;
      }
      std::this_thread::sleep_for(kSocketWaitDelay);
    } while (attempts++ < kMaxAttempts);
    ASSERT_TRUE(sender_client_->socket());
  }

  void Connect(const IPAddress& address) {
    uint16_t port = 65321;
    OSP_LOG_INFO << "\tStarting socket factories";
    task_runner_->PostTask([this, &address, port]() {
      OSP_LOG_INFO << "\tReceiver TLS factory Listen()";
      receiver_tls_factory_->SetListenCredentials(receiver_tls_creds_);
      receiver_tls_factory_->Listen(IPEndpoint{address, port},
                                    TlsListenOptions{1u});
    });

    task_runner_->PostTask([this, &address, port]() {
      OSP_LOG_INFO << "\tSender CastSocket factory Connect()";
      sender_factory_->Connect(IPEndpoint{address, port},
                               SenderSocketFactory::DeviceMediaPolicy::kNone,
                               sender_router_.get());
    });

    WaitForCastSocket();
  }

  TaskRunner* task_runner_;

  // NOTE: Sender components.
  VirtualConnectionManager sender_vc_manager_;
  SerialDeletePtr<VirtualConnectionRouter> sender_router_;
  std::unique_ptr<SenderSocketsClient> sender_client_;
  SerialDeletePtr<SenderSocketFactory> sender_factory_;
  SerialDeletePtr<TlsConnectionFactory> sender_tls_factory_;

  // NOTE: Receiver components.
  VirtualConnectionManager receiver_vc_manager_;
  SerialDeletePtr<VirtualConnectionRouter> receiver_router_;
  StaticCredentialsProvider receiver_creds_provider_;
  SerialDeletePtr<DeviceAuthNamespaceHandler> auth_handler_;
  std::unique_ptr<ReceiverSocketsClient> receiver_client_;
  SerialDeletePtr<ReceiverSocketFactory> receiver_factory_;
  TlsCredentials receiver_tls_creds_;
  SerialDeletePtr<TlsConnectionFactory> receiver_tls_factory_;
};

// These test the most basic setup of a complete CastSocket.  This means
// constructing both a SenderSocketFactory and ReceiverSocketFactory, making a
// TLS connection to a known port over the loopback device, and checking device
// authentication.
TEST_F(CastSocketE2ETest, ConnectV4) {
  OSP_LOG_INFO << "Getting loopback IPv4 address";
  IPAddress loopback_address = GetLoopbackV4Address();
  OSP_LOG_INFO << "Connecting CastSockets";
  Connect(loopback_address);
}

TEST_F(CastSocketE2ETest, ConnectV6) {
  OSP_LOG_INFO << "Getting loopback IPv6 address";
  IPAddress loopback_address = GetLoopbackV6Address();
  if (loopback_address) {
    OSP_LOG_INFO << "Connecting CastSockets";
    Connect(loopback_address);
  } else {
    OSP_LOG_WARN << "Test skipped due to missing IPv6 loopback address";
  }
}

}  // namespace cast
}  // namespace openscreen
