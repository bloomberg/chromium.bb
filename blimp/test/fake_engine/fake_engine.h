// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_TEST_FAKE_ENGINE_FAKE_ENGINE_H_
#define BLIMP_TEST_FAKE_ENGINE_FAKE_ENGINE_H_

#include "blimp/test/fake_engine/proto/engine.grpc.pb.h"
#include "blimp/test/fake_engine/proto/lifetime.grpc.pb.h"
#include "blimp/test/fake_engine/proto/logging.grpc.pb.h"
#include "third_party/grpc/include/grpc++/grpc++.h"

namespace blimp {

// FD to serve the Engine API on
const int kEngineListenFd = 3;

// FD on which the Rendering Server API is served
const int kRenderingServerListenFd = 4;

class EngineServiceImpl final : public Engine::Service {
 public:
  grpc::Status CheckHealth(
      grpc::ServerContext* context,
      const CheckHealthRequest* request,
      CheckHealthResponse* response) override;

  grpc::Status ShutDown(grpc::ServerContext* context,
                        const ShutDownRequest* request,
                        ShutDownResponse* response) override;
};

class FakeEngine {
 public:
  FakeEngine();
  ~FakeEngine();

  // Starts the FakeEngine.
  void Start();

  // Waits until the FakeEngine gRPC service is shut down. As gRPC++ doesn't
  // support clean shut down yet, this actually never returns.
  void WaitForShutdown();

  // The following methods return the stubs for communication with rendering
  // server. They must not be called before Start() method.
  Lifetime::Stub* GetLifetimeStub();
  Logging::Stub* GetLoggingStub();

 private:
  std::unique_ptr<grpc::Server> grpc_server_;
  EngineServiceImpl engine_service_;

  std::shared_ptr<grpc::Channel> rendering_server_channel_;
  std::unique_ptr<Lifetime::Stub> lifetime_stub_;
  std::unique_ptr<Logging::Stub> logging_stub_;
};

}  // namespace blimp

#endif  // BLIMP_TEST_FAKE_ENGINE_FAKE_ENGINE_H_
