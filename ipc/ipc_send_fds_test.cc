// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(OS_POSIX)
#if defined(OS_MACOSX)
extern "C" {
#include <sandbox.h>
}
#endif
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/file_descriptor_posix.h"
#include "base/message_loop.h"
#include "base/pickle.h"
#include "base/posix/eintr_wrapper.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_test_base.h"

namespace {

const unsigned kNumFDsToSend = 20;
const char* kDevZeroPath = "/dev/zero";

static void VerifyAndCloseDescriptor(int fd, ino_t inode_num) {
  // Check that we can read from the FD.
  char buf;
  ssize_t amt_read = read(fd, &buf, 1);
  ASSERT_EQ(amt_read, 1);
  ASSERT_EQ(buf, 0);  // /dev/zero always reads 0 bytes.

  struct stat st;
  ASSERT_EQ(fstat(fd, &st), 0);

  ASSERT_EQ(close(fd), 0);

  // Compare inode numbers to check that the file sent over the wire is actually
  // the one expected.
  ASSERT_EQ(inode_num, st.st_ino);
}

class MyChannelDescriptorListener : public IPC::Listener {
 public:
  explicit MyChannelDescriptorListener(ino_t expected_inode_num)
      : expected_inode_num_(expected_inode_num),
        num_fds_received_(0) {}

  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    PickleIterator iter(message);

    ++num_fds_received_;
    base::FileDescriptor descriptor;

    IPC::ParamTraits<base::FileDescriptor>::Read(&message, &iter, &descriptor);

    VerifyAndCloseDescriptor(descriptor.fd, expected_inode_num_);
    if (num_fds_received_ == kNumFDsToSend)
      base::MessageLoop::current()->Quit();

    return true;
  }

  virtual void OnChannelError() OVERRIDE {
    base::MessageLoop::current()->Quit();
  }

  bool GotExpectedNumberOfDescriptors() const {
    return num_fds_received_ == kNumFDsToSend;
  }

 private:
  ino_t expected_inode_num_;
  unsigned num_fds_received_;
};

class IPCSendFdsTest : public IPCTestBase {
 protected:
  void RunServer() {
    // Set up IPC channel and start client.
    MyChannelDescriptorListener listener(-1);
    CreateChannel(&listener);
    ASSERT_TRUE(ConnectChannel());
    ASSERT_TRUE(StartClient());

    for (unsigned i = 0; i < kNumFDsToSend; ++i) {
      const int fd = open(kDevZeroPath, O_RDONLY);
      ASSERT_GE(fd, 0);
      base::FileDescriptor descriptor(fd, true);

      IPC::Message* message =
          new IPC::Message(0, 3, IPC::Message::PRIORITY_NORMAL);
      IPC::ParamTraits<base::FileDescriptor>::Write(message, descriptor);
      ASSERT_TRUE(sender()->Send(message));
    }

    // Run message loop.
    base::MessageLoop::current()->Run();

    // Close the channel so the client's OnChannelError() gets fired.
    channel()->Close();

    EXPECT_TRUE(WaitForClientShutdown());
    DestroyChannel();
  }
};

TEST_F(IPCSendFdsTest, DescriptorTest) {
  Init("SendFdsClient");
  RunServer();
}

int SendFdsClientCommon(const std::string& test_client_name,
                        ino_t expected_inode_num) {
  base::MessageLoopForIO main_message_loop;
  MyChannelDescriptorListener listener(expected_inode_num);

  // Set up IPC channel.
  IPC::Channel channel(IPCTestBase::GetChannelName(test_client_name),
                       IPC::Channel::MODE_CLIENT,
                       &listener);
  CHECK(channel.Connect());

  // Run message loop.
  base::MessageLoop::current()->Run();

  // Verify that the message loop was exited due to getting the correct number
  // of descriptors, and not because of the channel closing unexpectedly.
  CHECK(listener.GotExpectedNumberOfDescriptors());

  return 0;
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendFdsClient) {
  struct stat st;
  int fd = open(kDevZeroPath, O_RDONLY);
  fstat(fd, &st);
  EXPECT_GE(HANDLE_EINTR(close(fd)), 0);
  return SendFdsClientCommon("SendFdsClient", st.st_ino);
}

#if defined(OS_MACOSX)
// Test that FDs are correctly sent to a sandboxed process.
// TODO(port): Make this test cross-platform.
TEST_F(IPCSendFdsTest, DescriptorTestSandboxed) {
  Init("SendFdsSandboxedClient");
  RunServer();
}

MULTIPROCESS_IPC_TEST_CLIENT_MAIN(SendFdsSandboxedClient) {
  struct stat st;
  const int fd = open(kDevZeroPath, O_RDONLY);
  fstat(fd, &st);
  if (HANDLE_EINTR(close(fd)) < 0)
    return -1;

  // Enable the sandbox.
  char* error_buff = NULL;
  int error = sandbox_init(kSBXProfilePureComputation, SANDBOX_NAMED,
                           &error_buff);
  bool success = (error == 0 && error_buff == NULL);
  if (!success)
    return -1;

  sandbox_free_error(error_buff);

  // Make sure sandbox is really enabled.
  if (open(kDevZeroPath, O_RDONLY) != -1) {
    LOG(ERROR) << "Sandbox wasn't properly enabled";
    return -1;
  }

  // See if we can receive a file descriptor.
  return SendFdsClientCommon("SendFdsSandboxedClient", st.st_ino);
}
#endif  // defined(OS_MACOSX)

}  // namespace

#endif  // defined(OS_POSIX)
