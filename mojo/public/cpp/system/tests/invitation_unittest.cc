// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/system/invitation.h"
#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/test/bind_test_util.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "mojo/public/cpp/system/wait.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace mojo {
namespace {

enum class TransportType {
  kChannel,
  kChannelServer,
};

// Switch and values to tell clients of parameterized test runs what mode they
// should be testing against.
const char kTransportTypeSwitch[] = "test-transport-type";
const char kTransportTypeChannel[] = "channel";
const char kTransportTypeChannelServer[] = "channel-server";

class InvitationCppTest : public testing::Test,
                          public testing::WithParamInterface<TransportType> {
 public:
  InvitationCppTest() = default;
  ~InvitationCppTest() override = default;

 protected:
  void LaunchChildTestClient(const std::string& test_client_name,
                             ScopedMessagePipeHandle* primordial_pipes,
                             size_t num_primordial_pipes,
                             TransportType transport_type,
                             const ProcessErrorCallback& error_callback = {}) {
    base::CommandLine command_line(
        base::GetMultiProcessTestChildBaseCommandLine());

    base::LaunchOptions launch_options;
    base::Optional<PlatformChannel> channel;
    PlatformChannelEndpoint channel_endpoint;
    PlatformChannelServerEndpoint server_endpoint;
    if (transport_type == TransportType::kChannel) {
      command_line.AppendSwitchASCII(kTransportTypeSwitch,
                                     kTransportTypeChannel);
      channel.emplace();
#if defined(OS_FUCHSIA)
      channel->PrepareToPassRemoteEndpoint(&launch_options.handles_to_transfer,
                                           &command_line);
#elif defined(OS_POSIX)
      channel->PrepareToPassRemoteEndpoint(&launch_options.fds_to_remap,
                                           &command_line);
#elif defined(OS_WIN)
      launch_options.start_hidden = true;
      channel->PrepareToPassRemoteEndpoint(&launch_options.handles_to_inherit,
                                           &command_line);
#else
#error "Unsupported platform."
#endif
      channel_endpoint = channel->TakeLocalEndpoint();
    } else if (transport_type == TransportType::kChannelServer) {
      command_line.AppendSwitchASCII(kTransportTypeSwitch,
                                     kTransportTypeChannelServer);
#if defined(OS_FUCHSIA)
      NOTREACHED() << "Named pipe support does not exist for Mojo on Fuchsia.";
#else
      NamedPlatformChannel::Options named_channel_options;
#if !defined(OS_WIN)
      CHECK(base::PathService::Get(base::DIR_TEMP,
                                   &named_channel_options.socket_dir));
#endif
      NamedPlatformChannel named_channel(named_channel_options);
      named_channel.PassServerNameOnCommandLine(&command_line);
      server_endpoint = named_channel.TakeServerEndpoint();
#endif
    }

    child_process_ = base::SpawnMultiProcessTestChild(
        test_client_name, command_line, launch_options);
    if (channel)
      channel->RemoteProcessLaunched();

    OutgoingInvitation invitation;
    for (uint64_t name = 0; name < num_primordial_pipes; ++name)
      primordial_pipes[name] = invitation.AttachMessagePipe(name);
    if (transport_type == TransportType::kChannel) {
      DCHECK(channel_endpoint.is_valid());
      OutgoingInvitation::Send(std::move(invitation), child_process_.Handle(),
                               std::move(channel_endpoint), error_callback);
    } else if (transport_type == TransportType::kChannelServer) {
      DCHECK(server_endpoint.is_valid());
      OutgoingInvitation::Send(std::move(invitation), child_process_.Handle(),
                               std::move(server_endpoint), error_callback);
    }
  }

  void WaitForChildExit() {
    int wait_result = -1;
    base::WaitForMultiprocessTestChildExit(
        child_process_, TestTimeouts::action_timeout(), &wait_result);
    child_process_.Close();
    EXPECT_EQ(0, wait_result);
  }

  static void WriteMessage(const ScopedMessagePipeHandle& pipe,
                           base::StringPiece message) {
    CHECK_EQ(MOJO_RESULT_OK,
             WriteMessageRaw(pipe.get(), message.data(), message.size(),
                             nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE));
  }

  static std::string ReadMessage(const ScopedMessagePipeHandle& pipe) {
    CHECK_EQ(MOJO_RESULT_OK, Wait(pipe.get(), MOJO_HANDLE_SIGNAL_READABLE));

    std::vector<uint8_t> payload;
    std::vector<ScopedHandle> handles;
    CHECK_EQ(MOJO_RESULT_OK, ReadMessageRaw(pipe.get(), &payload, &handles,
                                            MOJO_READ_MESSAGE_FLAG_NONE));
    return std::string(payload.begin(), payload.end());
  }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  base::Process child_process_;

  DISALLOW_COPY_AND_ASSIGN(InvitationCppTest);
};

class TestClientBase : public InvitationCppTest {
 public:
  static IncomingInvitation AcceptInvitation() {
    const auto& command_line = *base::CommandLine::ForCurrentProcess();

    std::string transport_type_string =
        command_line.GetSwitchValueASCII(kTransportTypeSwitch);
    CHECK(!transport_type_string.empty());

    if (transport_type_string == kTransportTypeChannel) {
      return IncomingInvitation::Accept(
          PlatformChannel::RecoverPassedEndpointFromCommandLine(command_line));
    } else if (transport_type_string == kTransportTypeChannelServer) {
      return IncomingInvitation::Accept(
          NamedPlatformChannel::ConnectToServer(command_line));
    }

    NOTREACHED();
    return IncomingInvitation();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestClientBase);
};

#define DEFINE_TEST_CLIENT(name)             \
  class name##Impl : public TestClientBase { \
   public:                                   \
    static void Run();                       \
  };                                         \
  MULTIPROCESS_TEST_MAIN(name) {             \
    name##Impl::Run();                       \
    return 0;                                \
  }                                          \
  void name##Impl::Run()

const char kTestMessage1[] = "hello";
const char kTestMessage2[] = "hello";

TEST_P(InvitationCppTest, Send) {
  ScopedMessagePipeHandle pipe;
  LaunchChildTestClient("CppSendClient", &pipe, 1, GetParam());
  WriteMessage(pipe, kTestMessage1);
  WaitForChildExit();
}

DEFINE_TEST_CLIENT(CppSendClient) {
  auto invitation = AcceptInvitation();
  auto pipe = invitation.ExtractMessagePipe(0);
  CHECK_EQ(kTestMessage1, ReadMessage(pipe));
}

TEST_P(InvitationCppTest, SendWithMultiplePipes) {
  ScopedMessagePipeHandle pipes[2];
  LaunchChildTestClient("CppSendWithMultiplePipesClient", pipes, 2, GetParam());
  WriteMessage(pipes[0], kTestMessage1);
  WriteMessage(pipes[1], kTestMessage2);
  WaitForChildExit();
}

DEFINE_TEST_CLIENT(CppSendWithMultiplePipesClient) {
  auto invitation = AcceptInvitation();
  auto pipe0 = invitation.ExtractMessagePipe(0);
  auto pipe1 = invitation.ExtractMessagePipe(1);
  CHECK_EQ(kTestMessage1, ReadMessage(pipe0));
  CHECK_EQ(kTestMessage2, ReadMessage(pipe1));
}

const char kErrorMessage[] = "ur bad :{{";
const char kDisconnectMessage[] = "go away plz";

TEST_P(InvitationCppTest, ProcessErrors) {
  ProcessErrorCallback actual_error_callback;

  ScopedMessagePipeHandle pipe;
  LaunchChildTestClient(
      "CppProcessErrorsClient", &pipe, 1, GetParam(),
      base::BindLambdaForTesting([&](const std::string& error_message) {
        ASSERT_TRUE(actual_error_callback);
        actual_error_callback.Run(error_message);
      }));

  MojoMessageHandle message;
  Wait(pipe.get(), MOJO_HANDLE_SIGNAL_READABLE);
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoReadMessage(pipe.get().value(), nullptr, &message));

  // Report the message as bad and expect to be notified through the process
  // error callback.
  base::RunLoop error_loop;
  actual_error_callback =
      base::BindLambdaForTesting([&](const std::string& error_message) {
        EXPECT_NE(error_message.find(kErrorMessage), std::string::npos);
        error_loop.Quit();
      });
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoNotifyBadMessage(message, kErrorMessage, sizeof(kErrorMessage),
                                 nullptr));
  error_loop.Run();
  EXPECT_EQ(MOJO_RESULT_OK, MojoDestroyMessage(message));

  // Now tell the child to die and wait for a disconnection notification.
  base::RunLoop disconnect_loop;
  actual_error_callback = base::BindLambdaForTesting(
      [&](const std::string& error_message) { disconnect_loop.Quit(); });
  WriteMessage(pipe, kDisconnectMessage);
  disconnect_loop.Run();

  WaitForChildExit();
}

DEFINE_TEST_CLIENT(CppProcessErrorsClient) {
  auto invitation = AcceptInvitation();
  auto pipe = invitation.ExtractMessagePipe(0);
  WriteMessage(pipe, "ignored");
  EXPECT_EQ(kDisconnectMessage, ReadMessage(pipe));
}

INSTANTIATE_TEST_CASE_P(,
                        InvitationCppTest,
                        testing::Values(TransportType::kChannel
#if !defined(OS_FUCHSIA)
                                        ,
                                        TransportType::kChannelServer
#endif
                                        ));

}  // namespace
}  // namespace mojo
