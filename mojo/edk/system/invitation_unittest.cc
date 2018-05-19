// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdint>
#include <string>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/test/multiprocess_test.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/platform_handle_utils.h"
#include "mojo/edk/test/mojo_test_base.h"
#include "mojo/public/c/system/invitation.h"

namespace mojo {
namespace edk {
namespace {

class InvitationTest : public test::MojoTestBase {
 public:
  InvitationTest() = default;
  ~InvitationTest() override = default;

 protected:
  static base::Process LaunchChildTestClient(
      const std::string& test_client_name,
      MojoHandle* primordial_pipes,
      size_t num_primordial_pipes);

 private:
  DISALLOW_COPY_AND_ASSIGN(InvitationTest);
};

TEST_F(InvitationTest, Create) {
  MojoHandle invitation;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateInvitation(nullptr, &invitation));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));

  MojoCreateInvitationOptions options;
  options.struct_size = sizeof(options);
  options.flags = MOJO_CREATE_INVITATION_FLAG_NONE;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateInvitation(&options, &invitation));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));
}

TEST_F(InvitationTest, InvalidArguments) {
  MojoHandle invitation;
  MojoCreateInvitationOptions invalid_create_options;
  invalid_create_options.struct_size = 0;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoCreateInvitation(&invalid_create_options, &invitation));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoCreateInvitation(nullptr, nullptr));

  // We need a valid invitation handle to exercise some of the other invalid
  // argument cases below.
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateInvitation(nullptr, &invitation));

  MojoHandle pipe;
  MojoAttachMessagePipeToInvitationOptions invalid_attach_options;
  invalid_attach_options.struct_size = 0;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoAttachMessagePipeToInvitation(MOJO_HANDLE_INVALID, 0, nullptr,
                                              &pipe));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoAttachMessagePipeToInvitation(invitation, 0,
                                              &invalid_attach_options, &pipe));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoAttachMessagePipeToInvitation(invitation, 0, nullptr, nullptr));

  MojoExtractMessagePipeFromInvitationOptions invalid_extract_options;
  invalid_extract_options.struct_size = 0;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoExtractMessagePipeFromInvitation(MOJO_HANDLE_INVALID, 0,
                                                 nullptr, &pipe));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoExtractMessagePipeFromInvitation(
                invitation, 0, &invalid_extract_options, &pipe));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoExtractMessagePipeFromInvitation(
                                              invitation, 0, nullptr, nullptr));

  PlatformChannelPair channel;
  MojoPlatformHandle endpoint_handle;
  endpoint_handle.struct_size = sizeof(endpoint_handle);
  EXPECT_EQ(MOJO_RESULT_OK, ScopedPlatformHandleToMojoPlatformHandle(
                                channel.PassServerHandle(), &endpoint_handle));
  MojoInvitationTransportEndpoint valid_endpoint;
  valid_endpoint.struct_size = sizeof(valid_endpoint);
  valid_endpoint.num_platform_handles = 1;
  valid_endpoint.platform_handles = &endpoint_handle;

  MojoSendInvitationOptions invalid_send_options;
  invalid_send_options.struct_size = 0;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSendInvitation(MOJO_HANDLE_INVALID, nullptr, &valid_endpoint,
                               nullptr, 0, nullptr));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSendInvitation(invitation, nullptr, &valid_endpoint, nullptr, 0,
                               &invalid_send_options));

  MojoInvitationTransportEndpoint invalid_endpoint;
  invalid_endpoint.struct_size = 0;
  invalid_endpoint.num_platform_handles = 1;
  invalid_endpoint.platform_handles = &endpoint_handle;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSendInvitation(invitation, nullptr, &invalid_endpoint, nullptr,
                               0, nullptr));

  invalid_endpoint.struct_size = sizeof(invalid_endpoint);
  invalid_endpoint.num_platform_handles = 0;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSendInvitation(invitation, nullptr, &invalid_endpoint, nullptr,
                               0, nullptr));

  MojoPlatformHandle invalid_platform_handle;
  invalid_platform_handle.struct_size = 0;
  invalid_endpoint.num_platform_handles = 1;
  invalid_endpoint.platform_handles = &invalid_platform_handle;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSendInvitation(invitation, nullptr, &invalid_endpoint, nullptr,
                               0, nullptr));
  invalid_platform_handle.struct_size = sizeof(invalid_platform_handle);
  invalid_platform_handle.type = MOJO_PLATFORM_HANDLE_TYPE_INVALID;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSendInvitation(invitation, nullptr, &invalid_endpoint, nullptr,
                               0, nullptr));

  invalid_endpoint.num_platform_handles = 1;
  invalid_endpoint.platform_handles = nullptr;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoSendInvitation(invitation, nullptr, &invalid_endpoint, nullptr,
                               0, nullptr));

  MojoHandle accepted_invitation;
  MojoAcceptInvitationOptions invalid_accept_options;
  invalid_accept_options.struct_size = 0;
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoAcceptInvitation(nullptr, nullptr, &accepted_invitation));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoAcceptInvitation(&valid_endpoint, &invalid_accept_options,
                                 &accepted_invitation));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT,
            MojoAcceptInvitation(&valid_endpoint, nullptr, nullptr));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));
}

TEST_F(InvitationTest, AttachAndExtractLocally) {
  MojoHandle invitation;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateInvitation(nullptr, &invitation));

  MojoHandle pipe0 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAttachMessagePipeToInvitation(invitation, 0, nullptr, &pipe0));
  EXPECT_NE(MOJO_HANDLE_INVALID, pipe0);

  MojoHandle pipe1 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK, MojoExtractMessagePipeFromInvitation(
                                invitation, 0, nullptr, &pipe1));
  EXPECT_NE(MOJO_HANDLE_INVALID, pipe1);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));

  // Should be able to communicate over the pipe.
  const std::string kMessage = "RSVP LOL";
  WriteMessage(pipe0, kMessage);
  EXPECT_EQ(kMessage, ReadMessage(pipe1));

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe0));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe1));
}

TEST_F(InvitationTest, ClosedInvitationClosesAttachments) {
  MojoHandle invitation;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateInvitation(nullptr, &invitation));

  MojoHandle pipe = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAttachMessagePipeToInvitation(invitation, 0, nullptr, &pipe));
  EXPECT_NE(MOJO_HANDLE_INVALID, pipe);

  // Closing the invitation should close |pipe|'s peer.
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));

  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe));
}

TEST_F(InvitationTest, AttachNameInUse) {
  MojoHandle invitation;
  EXPECT_EQ(MOJO_RESULT_OK, MojoCreateInvitation(nullptr, &invitation));

  MojoHandle pipe0 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAttachMessagePipeToInvitation(invitation, 0, nullptr, &pipe0));
  EXPECT_NE(MOJO_HANDLE_INVALID, pipe0);

  MojoHandle pipe1 = MOJO_HANDLE_INVALID;
  EXPECT_EQ(MOJO_RESULT_ALREADY_EXISTS,
            MojoAttachMessagePipeToInvitation(invitation, 0, nullptr, &pipe1));
  EXPECT_EQ(MOJO_HANDLE_INVALID, pipe1);
  EXPECT_EQ(MOJO_RESULT_OK,
            MojoAttachMessagePipeToInvitation(invitation, 1, nullptr, &pipe1));
  EXPECT_NE(MOJO_HANDLE_INVALID, pipe1);

  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(invitation));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe0));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(pipe1));
}

// static
base::Process InvitationTest::LaunchChildTestClient(
    const std::string& test_client_name,
    MojoHandle* primordial_pipes,
    size_t num_primordial_pipes) {
  base::CommandLine command_line(
      base::GetMultiProcessTestChildBaseCommandLine());

  PlatformChannelPair channel;
  base::LaunchOptions options;
#if defined(OS_FUCHSIA)
  channel.PrepareToPassClientHandleToChildProcess(&command_line,
                                                  &options.handles_to_transfer);
#elif defined(OS_POSIX)
  channel.PrepareToPassClientHandleToChildProcess(&command_line,
                                                  &options.fds_to_remap);
#elif defined(OS_WIN)
  options.start_hidden = true;
  channel.PrepareToPassClientHandleToChildProcess(&command_line,
                                                  &options.handles_to_inherit);
#else
#error "Platform not yet supported."
#endif

  MojoPlatformHandle endpoint_handle;
  endpoint_handle.struct_size = sizeof(endpoint_handle);
  CHECK_EQ(MOJO_RESULT_OK, ScopedPlatformHandleToMojoPlatformHandle(
                               channel.PassServerHandle(), &endpoint_handle));

  MojoHandle invitation;
  CHECK_EQ(MOJO_RESULT_OK, MojoCreateInvitation(nullptr, &invitation));
  for (uint64_t name = 0; name < num_primordial_pipes; ++name) {
    CHECK_EQ(MOJO_RESULT_OK,
             MojoAttachMessagePipeToInvitation(invitation, name, nullptr,
                                               &primordial_pipes[name]));
  }

  base::Process child_process =
      base::SpawnMultiProcessTestChild(test_client_name, command_line, options);
  channel.ChildProcessLaunched();

  MojoPlatformProcessHandle process_handle;
  process_handle.struct_size = sizeof(process_handle);
#if defined(OS_WIN)
  process_handle.value = static_cast<uint64_t>(
      reinterpret_cast<uintptr_t>(child_process.Handle()));
#else
  process_handle.value = static_cast<uint64_t>(child_process.Handle());
#endif

  MojoInvitationTransportEndpoint transport_endpoint;
  transport_endpoint.struct_size = sizeof(transport_endpoint);
  transport_endpoint.num_platform_handles = 1;
  transport_endpoint.platform_handles = &endpoint_handle;
  CHECK_EQ(MOJO_RESULT_OK,
           MojoSendInvitation(invitation, &process_handle, &transport_endpoint,
                              nullptr, 0, nullptr));
  return child_process;
}

class TestClientBase : public InvitationTest {
 public:
  static MojoHandle AcceptInvitation() {
    ScopedPlatformHandle channel_endpoint =
        PlatformChannelPair::PassClientHandleFromParentProcess(
            *base::CommandLine::ForCurrentProcess());
    MojoPlatformHandle endpoint_handle;
    endpoint_handle.struct_size = sizeof(endpoint_handle);
    CHECK_EQ(MOJO_RESULT_OK,
             ScopedPlatformHandleToMojoPlatformHandle(
                 std::move(channel_endpoint), &endpoint_handle));

    MojoInvitationTransportEndpoint transport_endpoint;
    transport_endpoint.struct_size = sizeof(transport_endpoint);
    transport_endpoint.num_platform_handles = 1;
    transport_endpoint.platform_handles = &endpoint_handle;

    MojoHandle invitation;
    CHECK_EQ(MOJO_RESULT_OK,
             MojoAcceptInvitation(&transport_endpoint, nullptr, &invitation));
    return invitation;
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

const std::string kTestMessage1 = "i am the pusher robot";
const std::string kTestMessage2 = "i push the messages down the pipe";
const std::string kTestMessage3 = "i am the shover robot";
const std::string kTestMessage4 = "i shove the messages down the pipe";

TEST_F(InvitationTest, SendInvitation) {
  MojoHandle primordial_pipe;
  base::Process child_process =
      LaunchChildTestClient("SendInvitationClient", &primordial_pipe, 1);

  WriteMessage(primordial_pipe, kTestMessage1);
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(kTestMessage3, ReadMessage(primordial_pipe));
  EXPECT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));

  int wait_result = -1;
  base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &wait_result);
  child_process.Close();
  EXPECT_EQ(0, wait_result);
}

DEFINE_TEST_CLIENT(SendInvitationClient) {
  MojoHandle primordial_pipe;
  MojoHandle invitation = AcceptInvitation();
  ASSERT_EQ(MOJO_RESULT_OK, MojoExtractMessagePipeFromInvitation(
                                invitation, 0, nullptr, &primordial_pipe));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(invitation));

  WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_READABLE);
  ASSERT_EQ(kTestMessage1, ReadMessage(primordial_pipe));
  WriteMessage(primordial_pipe, kTestMessage3);
  WaitForSignals(primordial_pipe, MOJO_HANDLE_SIGNAL_PEER_CLOSED);

  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(primordial_pipe));
}

TEST_F(InvitationTest, SendInvitationMultiplePipes) {
  MojoHandle pipes[2];
  base::Process child_process =
      LaunchChildTestClient("SendInvitationMultiplePipesClient", pipes, 2);

  WriteMessage(pipes[0], kTestMessage1);
  WriteMessage(pipes[1], kTestMessage2);
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipes[0], MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(MOJO_RESULT_OK,
            WaitForSignals(pipes[1], MOJO_HANDLE_SIGNAL_READABLE));
  EXPECT_EQ(kTestMessage3, ReadMessage(pipes[0]));
  EXPECT_EQ(kTestMessage4, ReadMessage(pipes[1]));

  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(pipes[0]));
  ASSERT_EQ(MOJO_RESULT_OK, MojoClose(pipes[1]));

  int wait_result = -1;
  base::WaitForMultiprocessTestChildExit(
      child_process, TestTimeouts::action_timeout(), &wait_result);
  child_process.Close();
  EXPECT_EQ(0, wait_result);
}

DEFINE_TEST_CLIENT(SendInvitationMultiplePipesClient) {
  MojoHandle pipes[2];
  MojoHandle invitation = AcceptInvitation();
  ASSERT_EQ(MOJO_RESULT_OK, MojoExtractMessagePipeFromInvitation(
                                invitation, 0, nullptr, &pipes[0]));
  ASSERT_EQ(MOJO_RESULT_OK, MojoExtractMessagePipeFromInvitation(
                                invitation, 1, nullptr, &pipes[1]));

  WaitForSignals(pipes[0], MOJO_HANDLE_SIGNAL_READABLE);
  WaitForSignals(pipes[1], MOJO_HANDLE_SIGNAL_READABLE);
  ASSERT_EQ(kTestMessage1, ReadMessage(pipes[0]));
  ASSERT_EQ(kTestMessage2, ReadMessage(pipes[1]));
  WriteMessage(pipes[0], kTestMessage3);
  WriteMessage(pipes[1], kTestMessage4);
  WaitForSignals(pipes[0], MOJO_HANDLE_SIGNAL_PEER_CLOSED);
  WaitForSignals(pipes[1], MOJO_HANDLE_SIGNAL_PEER_CLOSED);
}

}  // namespace
}  // namespace edk
}  // namespace mojo
