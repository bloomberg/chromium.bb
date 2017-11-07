// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/chromium/quic_stream_factory.h"

#include "base/test/fuzzed_data_provider.h"

#include "net/base/test_completion_callback.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/multi_log_ct_verifier.h"
#include "net/dns/fuzzed_host_resolver.h"
#include "net/http/http_server_properties_impl.h"
#include "net/http/transport_security_state.h"
#include "net/quic/chromium/mock_crypto_client_stream_factory.h"
#include "net/quic/chromium/test_task_runner.h"
#include "net/quic/test_tools/mock_clock.h"
#include "net/quic/test_tools/mock_random.h"
#include "net/socket/fuzzed_datagram_client_socket.h"
#include "net/socket/fuzzed_socket_factory.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/default_channel_id_store.h"

namespace net {

namespace {

class MockSSLConfigService : public SSLConfigService {
 public:
  MockSSLConfigService() {}

  void GetSSLConfig(SSLConfig* config) override { *config = config_; }

 private:
  ~MockSSLConfigService() override {}

  SSLConfig config_;
};

}  // namespace

namespace test {

const char kServerHostName[] = "www.example.org";
const int kServerPort = 443;
const char kUrl[] = "https://www.example.org/";
// TODO(nedwilliamson): Add POST here after testing
// whether that can lead blocking while waiting for
// the callbacks.
const char kMethod[] = "GET";
const size_t kBufferSize = 4096;
const int kCertVerifyFlags = 0;

// Static initialization for persistent factory data
struct Env {
  Env() : host_port_pair(kServerHostName, kServerPort), random_generator(0) {
    clock.AdvanceTime(QuicTime::Delta::FromSeconds(1));
    ssl_config_service = new MockSSLConfigService;
    crypto_client_stream_factory.set_use_mock_crypter(true);
    cert_verifier = CertVerifier::CreateDefault();
    channel_id_service =
        std::make_unique<ChannelIDService>(new DefaultChannelIDStore(nullptr));
    cert_transparency_verifier = std::make_unique<MultiLogCTVerifier>();
  }

  MockClock clock;
  scoped_refptr<SSLConfigService> ssl_config_service;
  MockCryptoClientStreamFactory crypto_client_stream_factory;
  HostPortPair host_port_pair;
  MockRandom random_generator;
  NetLogWithSource net_log;
  std::unique_ptr<CertVerifier> cert_verifier;
  std::unique_ptr<ChannelIDService> channel_id_service;
  TransportSecurityState transport_security_state;
  QuicTagVector connection_options;
  QuicTagVector client_connection_options;
  std::unique_ptr<CTVerifier> cert_transparency_verifier;
  CTPolicyEnforcer ct_policy_enforcer;
};

static struct Env* env = new Env();

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  base::FuzzedDataProvider data_provider(data, size);

  FuzzedHostResolver host_resolver(HostResolver::Options(), nullptr,
                                   &data_provider);
  FuzzedSocketFactory socket_factory(&data_provider);

  // Initialize this on each loop since some options mutate this.
  HttpServerPropertiesImpl http_server_properties;

  std::unique_ptr<QuicStreamFactory> factory =
      std::make_unique<QuicStreamFactory>(
          env->net_log.net_log(), &host_resolver, env->ssl_config_service.get(),
          &socket_factory, &http_server_properties, env->cert_verifier.get(),
          &env->ct_policy_enforcer, env->channel_id_service.get(),
          &env->transport_security_state, env->cert_transparency_verifier.get(),
          nullptr, &env->crypto_client_stream_factory, &env->random_generator,
          &env->clock, kDefaultMaxPacketSize, std::string(),
          data_provider.ConsumeBool(), data_provider.ConsumeBool(),
          kIdleConnectionTimeoutSeconds, kPingTimeoutSecs,
          data_provider.ConsumeBool(), data_provider.ConsumeBool(),
          data_provider.ConsumeBool(), data_provider.ConsumeBool(),
          data_provider.ConsumeBool(), data_provider.ConsumeBool(),
          env->connection_options, env->client_connection_options,
          data_provider.ConsumeBool());

  QuicStreamRequest request(factory.get());
  TestCompletionCallback callback;
  NetErrorDetails net_error_details;
  request.Request(env->host_port_pair,
                  data_provider.PickValueInArray(kSupportedTransportVersions),
                  PRIVACY_MODE_DISABLED, kCertVerifyFlags, GURL(kUrl), kMethod,
                  env->net_log, &net_error_details, callback.callback());

  callback.WaitForResult();
  std::unique_ptr<HttpStream> stream = request.CreateStream();
  if (!stream.get())
    return 0;

  HttpRequestInfo request_info;
  request_info.method = kMethod;
  request_info.url = GURL(kUrl);
  stream->InitializeStream(&request_info, DEFAULT_PRIORITY, env->net_log,
                           CompletionCallback());

  HttpResponseInfo response;
  HttpRequestHeaders request_headers;
  if (OK !=
      stream->SendRequest(request_headers, &response, callback.callback()))
    return 0;

  // TODO(nedwilliamson): attempt connection migration here
  stream->ReadResponseHeaders(callback.callback());
  callback.WaitForResult();

  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(kBufferSize);
  int rv =
      stream->ReadResponseBody(buffer.get(), kBufferSize, callback.callback());
  if (rv == ERR_IO_PENDING)
    callback.WaitForResult();

  return 0;
}

}  // namespace test
}  // namespace net
