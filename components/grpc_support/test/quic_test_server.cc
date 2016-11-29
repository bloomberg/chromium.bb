// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/grpc_support/test/quic_test_server.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/chromium/crypto/proof_source_chromium.h"
#include "net/spdy/spdy_header_block.h"
#include "net/test/test_data_directory.h"
#include "net/tools/quic/quic_in_memory_cache.h"
#include "net/tools/quic/quic_simple_server.h"

namespace grpc_support {

// This must match the certificate used (quic_test.example.com.crt and
// quic_test.example.com.key.pkcs8).
const char kTestServerHost[] = "test.example.com";
const char kTestServerUrl[] = "https://test.example.com/hello.txt";

const char kStatusHeader[] = ":status";

const char kHelloPath[] = "/hello.txt";
const char kHelloBodyValue[] = "Hello from QUIC Server";
const char kHelloStatus[] = "200";

const char kHelloHeaderName[] = "hello_header";
const char kHelloHeaderValue[] = "hello header value";

const char kHelloTrailerName[] = "hello_trailer";
const char kHelloTrailerValue[] = "hello trailer value";

base::Thread* g_quic_server_thread = nullptr;
net::QuicInMemoryCache* g_quic_in_memory_cache = nullptr;
net::QuicSimpleServer* g_quic_server = nullptr;
int g_quic_server_port = 0;

void SetupQuicInMemoryCache() {
  net::SpdyHeaderBlock headers;
  headers[kHelloHeaderName] = kHelloHeaderValue;
  headers[kStatusHeader] =  kHelloStatus;
  net::SpdyHeaderBlock trailers;
  trailers[kHelloTrailerName] = kHelloTrailerValue;
  g_quic_in_memory_cache = new net::QuicInMemoryCache();
  g_quic_in_memory_cache->AddResponse(base::StringPrintf("%s", kTestServerHost),
                                      kHelloPath, std::move(headers),
                                      kHelloBodyValue, std::move(trailers));
}

void StartQuicServerOnServerThread(const base::FilePath& test_files_root,
                                   base::WaitableEvent* server_started_event) {
  DCHECK(g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  DCHECK(!g_quic_server);

  net::QuicConfig config;
  // Set up server certs.
  base::FilePath directory;
  directory = test_files_root;
  std::unique_ptr<net::ProofSourceChromium> proof_source(
      new net::ProofSourceChromium());
  CHECK(proof_source->Initialize(
      directory.AppendASCII("quic_test.example.com.crt"),
      directory.AppendASCII("quic_test.example.com.key.pkcs8"),
      directory.AppendASCII("quic_test.example.com.key.sct")));

  SetupQuicInMemoryCache();

  g_quic_server = new net::QuicSimpleServer(
      std::move(proof_source), config,
      net::QuicCryptoServerConfig::ConfigOptions(), net::AllSupportedVersions(),
      g_quic_in_memory_cache);

  // Start listening on an unbound port.
  int rv = g_quic_server->Listen(
      net::IPEndPoint(net::IPAddress::IPv4AllZeros(), 0));
  CHECK_GE(rv, 0) << "Quic server fails to start";
  g_quic_server_port = g_quic_server->server_address().port();

  server_started_event->Signal();
}

void ShutdownOnServerThread(base::WaitableEvent* server_stopped_event) {
  DCHECK(g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  g_quic_server->Shutdown();
  delete g_quic_server;
  g_quic_server = nullptr;
  delete g_quic_in_memory_cache;
  g_quic_in_memory_cache = nullptr;
  server_stopped_event->Signal();
}

bool StartQuicTestServer() {
  LOG(INFO) << g_quic_server_thread;
  DCHECK(!g_quic_server_thread);
  g_quic_server_thread = new base::Thread("quic server thread");
  base::Thread::Options thread_options;
  thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  bool started = g_quic_server_thread->StartWithOptions(thread_options);
  DCHECK(started);
  base::FilePath test_files_root = net::GetTestCertsDirectory();

  base::WaitableEvent server_started_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  g_quic_server_thread->task_runner()->PostTask(
      FROM_HERE, base::Bind(&StartQuicServerOnServerThread, test_files_root,
                            &server_started_event));
  server_started_event.Wait();
  return true;
}

void ShutdownQuicTestServer() {
  if (!g_quic_server_thread)
    return;
  DCHECK(!g_quic_server_thread->task_runner()->BelongsToCurrentThread());
  base::WaitableEvent server_stopped_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  g_quic_server_thread->task_runner()->PostTask(
      FROM_HERE, base::Bind(&ShutdownOnServerThread, &server_stopped_event));
  server_stopped_event.Wait();
  delete g_quic_server_thread;
  g_quic_server_thread = nullptr;
}

int GetQuicTestServerPort() {
  return g_quic_server_port;
}

}  // namespace grpc_support
