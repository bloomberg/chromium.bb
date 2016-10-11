// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/test/fake_engine/fake_engine.h"

#include <fcntl.h>

#include "base/logging.h"
#include "blimp/test/fake_engine/proto/engine.grpc.pb.h"
#include "blimp/test/fake_engine/proto/lifetime.grpc.pb.h"
#include "blimp/test/fake_engine/proto/logging.grpc.pb.h"
#include "third_party/grpc/include/grpc++/create_channel_posix.h"
#include "third_party/grpc/include/grpc++/grpc++.h"
#include "third_party/grpc/include/grpc++/server_posix.h"

namespace blimp {

grpc::Status EngineServiceImpl::CheckHealth(
    grpc::ServerContext* context,
    const CheckHealthRequest* request,
    CheckHealthResponse* response) {
  response->set_status(CheckHealthResponse::OK);
  return grpc::Status::OK;
}

grpc::Status EngineServiceImpl::ShutDown(
    grpc::ServerContext* context,
    const  ShutDownRequest* request,
    ShutDownResponse* response) {
  exit(0);
  return grpc::Status::OK;
}

FakeEngine::FakeEngine() = default;
FakeEngine::~FakeEngine() = default;

void FakeEngine::Start() {
  DCHECK(!grpc_server_) << "FakeEngine already started.";

  grpc::ServerBuilder builder;
  builder.RegisterService(&engine_service_);
  grpc_server_ = builder.BuildAndStart();

  // TODO(xyzzyz): Remove fcntl when https://github.com/grpc/grpc/pull/8051 is
  // merged and added to Chromium.
  int flags = fcntl(kEngineListenFd, F_GETFL, 0);
  CHECK_EQ(0, fcntl(kEngineListenFd, F_SETFL, flags | O_NONBLOCK));

  grpc::AddInsecureChannelFromFd(grpc_server_.get(), kEngineListenFd);
  VLOG(1) << "Started engine server.";

  rendering_server_channel_ = grpc::CreateInsecureChannelFromFd(
      "rendering_server", kRenderingServerListenFd);
  lifetime_stub_ = Lifetime::NewStub(rendering_server_channel_);
  logging_stub_ = Logging::NewStub(rendering_server_channel_);

  EngineReadyRequest request;
  EngineReadyResponse response;

  grpc::ClientContext context;
  grpc::Status status = lifetime_stub_->EngineReady(
      &context, request, &response);

  CHECK(status.ok()) << "EngineReady RPC failed: "
                     << status.error_code() << ": " << status.error_message();
  VLOG(1) << "EngineReady RPC success";
}

void FakeEngine::WaitForShutdown() {
  DCHECK(grpc_server_) << "FakeEngine not started.";
  grpc_server_->Wait();
}

Lifetime::Stub* FakeEngine::GetLifetimeStub() {
  return lifetime_stub_.get();
}

Logging::Stub* FakeEngine::GetLoggingStub() {
  return logging_stub_.get();
}

}  // namespace blimp
