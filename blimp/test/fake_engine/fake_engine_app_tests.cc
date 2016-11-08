// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/test/fake_engine/fake_engine.h"

#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "blimp/test/fake_engine/proto/engine.grpc.pb.h"
#include "blimp/test/fake_engine/proto/lifetime.grpc.pb.h"
#include "blimp/test/fake_engine/proto/logging.grpc.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/grpc/include/grpc++/create_channel_posix.h"
#include "third_party/grpc/include/grpc++/grpc++.h"
#include "third_party/grpc/include/grpc++/server_posix.h"

using ::testing::_;

namespace blimp {

namespace {

bool SocketPair(int* fd1, int* fd2) {
  int pipe_fds[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipe_fds) != 0) {
    PLOG(ERROR) << "socketpair()";
    return false;
  }

  // Set both ends to be non-blocking.
  if (fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK) == -1 ||
      fcntl(pipe_fds[1], F_SETFL, O_NONBLOCK) == -1) {
    PLOG(ERROR) << "fcntl(O_NONBLOCK)";
    if (IGNORE_EINTR(close(pipe_fds[0])) < 0)
      PLOG(ERROR) << "close";
    if (IGNORE_EINTR(close(pipe_fds[1])) < 0)
      PLOG(ERROR) << "close";
    return false;
  }

  *fd1 = pipe_fds[0];
  *fd2 = pipe_fds[1];

  return true;
}

class MockLifetimeService : public Lifetime::Service {
 public:
  MOCK_METHOD3(EngineReady, grpc::Status(
      grpc::ServerContext*, const EngineReadyRequest*, EngineReadyResponse*));
};

class MockLoggingService : public Logging::Service {
 public:
  MOCK_METHOD3(Log, grpc::Status(
      grpc::ServerContext*, const LogRequest*, LogResponse*));
};

// The FakeEngineAppTest suite sets up a simple interface resembling the
// Service, which is required by the Fake Engine. This allows testing whether
// the Fake Engine serves its purpose of simulating the real Engine as spawned
// by the Service.
class FakeEngineAppTest : public testing::Test {
 public:
  FakeEngineAppTest()
      : engine_ready_(base::WaitableEvent::ResetPolicy::MANUAL,
                      base::WaitableEvent::InitialState::NOT_SIGNALED) {}

 protected:
  void SetUp() override {
    CHECK(SocketPair(&rendering_server_connect_fd_, &engine_listen_fd_));
    CHECK(SocketPair(&rendering_server_listen_fd_, &engine_connect_fd_));

    grpc::ServerBuilder builder;
    builder.RegisterService(&mock_lifetime_service_);
    builder.RegisterService(&mock_logging_service_);
    grpc_server_ = builder.BuildAndStart();

    // TODO(xyzzyz): Remove fcntl when https://github.com/grpc/grpc/pull/8051 is
    // merged and added to Chromium.
    int flags = fcntl(rendering_server_listen_fd_, F_GETFL, 0);
    CHECK_EQ(0, fcntl(rendering_server_listen_fd_, F_SETFL,
                      flags | O_NONBLOCK));

    grpc::AddInsecureChannelFromFd(grpc_server_.get(),
                                   rendering_server_listen_fd_);
  }

  // This function spawns a Fake Engine as a child process, and sets up the file
  // descriptor mappings for it, resembling the mappings in the Service
  // environment. It also sets up the handler for the EngineReady function that
  // will be called by the Fake Engine upon startup.
  base::Process SpawnEngine() {
    // Find the path of the Engine.
    base::CommandLine current_cmd = *base::CommandLine::ForCurrentProcess();
    base::FilePath current_program_path = base::MakeAbsoluteFilePath(
        current_cmd.GetProgram());
    base::FilePath current_dir = current_program_path.DirName();
    base::FilePath fake_engine_path = current_dir.Append("fake_engine_app");

    base::CommandLine engine_cmd(fake_engine_path);

    base::LaunchOptions options;
    base::FileHandleMappingVector fds_to_remap = {
      { engine_listen_fd_, kEngineListenFd },
      { engine_connect_fd_, kRenderingServerListenFd }
    };
    options.fds_to_remap = &fds_to_remap;

    ON_CALL(mock_lifetime_service_, EngineReady(_, _, _))
        .WillByDefault(
            testing::Invoke(this, &FakeEngineAppTest::EngineReadyHandler));
    return base::LaunchProcess(engine_cmd, options);
  }

  grpc::Status EngineReadyHandler(
      grpc::ServerContext*, const EngineReadyRequest*, EngineReadyResponse*) {
    engine_ready_.Signal();
    return grpc::Status::OK;
  }

  bool WaitForEngineReady(base::TimeDelta timeout) {
    return engine_ready_.TimedWait(timeout);
  }

  std::unique_ptr<Engine::Stub> GetEngineStub() {
    CHECK(!engine_channel_);
    engine_channel_ = grpc::CreateInsecureChannelFromFd(
        "fake_engine", rendering_server_connect_fd_);
    CHECK(engine_channel_);
    return Engine::NewStub(engine_channel_);
  }

 private:
  int engine_listen_fd_;
  int rendering_server_connect_fd_;

  int rendering_server_listen_fd_;
  int engine_connect_fd_;

  std::unique_ptr<grpc::Server> grpc_server_;
  MockLifetimeService mock_lifetime_service_;
  MockLoggingService mock_logging_service_;

  std::shared_ptr<grpc::Channel> engine_channel_;
  base::WaitableEvent engine_ready_;

  DISALLOW_COPY_AND_ASSIGN(FakeEngineAppTest);
};


TEST_F(FakeEngineAppTest, Basic) {
  base::Process engine_process = SpawnEngine();
  EXPECT_TRUE(WaitForEngineReady(base::TimeDelta::FromSeconds(5)));

  std::unique_ptr<Engine::Stub> engine_stub = GetEngineStub();
  {
    CheckHealthRequest request;
    CheckHealthResponse response;

    grpc::ClientContext context;
    grpc::Status status = engine_stub->CheckHealth(
        &context, request, &response);
    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.status(), CheckHealthResponse::OK);
  }

  {
    ShutDownRequest request;
    ShutDownResponse response;

    grpc::ClientContext context;
    grpc::CompletionQueue cq;
    // We do async call, as the Fake Engine doesn't send any reply to the
    // ShutDown RPC, so it makes no sense to wait for it.
    auto rpc = engine_stub->AsyncShutDown(&context, request, &cq);

    EXPECT_TRUE(engine_process.WaitForExitWithTimeout(
        base::TimeDelta::FromSeconds(5), nullptr /* exit_code */));
  }
}

class FakeEngineTestSuite : public base::TestSuite {
 public:
  FakeEngineTestSuite(int argc, char** argv) : base::TestSuite(argc, argv) {}

 protected:
  void Initialize() override {
    base::TestSuite::Initialize();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeEngineTestSuite);
};

}  // namespace

}  // namespace blimp

int main(int argc, char** argv) {
  blimp::FakeEngineTestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc, argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
}
