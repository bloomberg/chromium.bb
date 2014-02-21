// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string>

#include "dev_fs_for_testing.h"
#include "fake_ppapi/fake_messaging_interface.h"
#include "gtest/gtest.h"
#include "nacl_io/devfs/dev_fs.h"
#include "nacl_io/filesystem.h"
#include "nacl_io/ioctl.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"
#include "nacl_io/osdirent.h"

using namespace nacl_io;

namespace {

class JSPipeTest : public ::testing::Test {
 public:
  void SetUp() {
    ASSERT_EQ(0, ki_push_state_for_testing());
    ASSERT_EQ(0, ki_init(&kp_));
    ASSERT_EQ(0, fs_.Access(Path("/jspipe1"), R_OK | W_OK));
    ASSERT_EQ(EACCES, fs_.Access(Path("/jspipe1"), X_OK));
    ASSERT_EQ(0, fs_.Open(Path("/jspipe1"), O_RDWR, &pipe_dev_));
    ASSERT_NE(NULL_NODE, pipe_dev_.get());
  }

  void TearDown() { ki_uninit(); }

 protected:
  KernelProxy kp_;
  DevFsForTesting fs_;
  ScopedNode pipe_dev_;
};

TEST_F(JSPipeTest, InvalidIoctl) {
  // 123 is not a valid ioctl request.
  EXPECT_EQ(EINVAL, pipe_dev_->Ioctl(123));
}

TEST_F(JSPipeTest, JSPipeInput) {
  // First we send some data into the pipe.  This is how messages
  // from javascript are injected into the pipe nodes.
  std::string message("hello, how are you?\n");
  struct tioc_nacl_input_string packaged_message;
  packaged_message.length = message.size();
  packaged_message.buffer = message.data();
  ASSERT_EQ(0, pipe_dev_->Ioctl(TIOCNACLINPUT, &packaged_message));

  // Now we make buffer we'll read into.
  // We fill the buffer and a backup buffer with arbitrary data
  // and compare them after reading to make sure read doesn't
  // clobber parts of the buffer it shouldn't.
  int bytes_read;
  char buffer[100];
  char backup_buffer[100];
  memset(buffer, 'a', sizeof(buffer));
  memset(backup_buffer, 'a', sizeof(backup_buffer));

  // We read a small chunk first to ensure it doesn't give us
  // more than we ask for.
  HandleAttr attrs;
  ASSERT_EQ(0, pipe_dev_->Read(attrs, buffer, 5, &bytes_read));
  EXPECT_EQ(5, bytes_read);
  EXPECT_EQ(0, memcmp(message.data(), buffer, 5));
  EXPECT_EQ(0, memcmp(buffer + 5, backup_buffer + 5, sizeof(buffer)-5));

  // Now we ask for more data than is left in the pipe, to ensure
  // it doesn't give us more than there is.
  ASSERT_EQ(0, pipe_dev_->Read(attrs, buffer + 5, sizeof(buffer)-5,
                               &bytes_read));
  EXPECT_EQ(bytes_read, message.size() - 5);
  EXPECT_EQ(0, memcmp(message.data(), buffer, message.size()));
  EXPECT_EQ(0, memcmp(buffer + message.size(),
                      backup_buffer + message.size(),
                      100 - message.size()));
}

TEST_F(JSPipeTest, JSPipeOutput) {
  const char* message = "hello";
  const int message_len = strlen(message);

  int bytes_written = 999;
  HandleAttr attrs;
  ASSERT_EQ(0, pipe_dev_->Write(attrs, message, message_len, &bytes_written));
  ASSERT_EQ(message_len, bytes_written);

  // Verify that the correct messages was sent via PostMessage.
  FakeMessagingInterface* iface =
      (FakeMessagingInterface*)fs_.ppapi()->GetMessagingInterface();
  VarArrayInterface* array_iface = fs_.ppapi()->GetVarArrayInterface();
  VarInterface* var_iface = fs_.ppapi()->GetVarInterface();
  VarArrayBufferInterface* buffer_iface =
      fs_.ppapi()->GetVarArrayBufferInterface();

  // Verify that exaclty one message was sent of type PP_VARTYPE_ARRAY
  ASSERT_EQ(1, iface->messages.size());
  PP_Var array = iface->messages[0];
  ASSERT_EQ(PP_VARTYPE_ARRAY, array.type);

  // Verify that the array contains two element, the prefix,
  // and an ArrayBuffer containing the message.
  ASSERT_EQ(2, array_iface->GetLength(array));
  PP_Var item0 = array_iface->Get(array, 0);
  PP_Var item1 = array_iface->Get(array, 1);
  ASSERT_EQ(PP_VARTYPE_STRING, item0.type);
  ASSERT_EQ(PP_VARTYPE_ARRAY_BUFFER, item1.type);
  uint32_t len = 0;
  const char* item0_string = var_iface->VarToUtf8(item0, &len);
  ASSERT_STREQ("jspipe1", std::string(item0_string, len).c_str());
  ASSERT_EQ(0, memcmp(message, buffer_iface->Map(item1), strlen(message)));
  var_iface->Release(item0);
  var_iface->Release(item1);
}

TEST_F(JSPipeTest, JSPipeOutputWithNulls) {
  char message[20];
  int message_len = sizeof(message);

  // Construct a 20-byte  message containing the string 'hello' but with
  // null chars on either end.
  memset(message, 0 , message_len);
  memcpy(message+10, "hello", 5);

  int bytes_written = 999;
  HandleAttr attrs;
  EXPECT_EQ(0, pipe_dev_->Write(attrs, message, message_len, &bytes_written));
  EXPECT_EQ(message_len, bytes_written);

  // Verify that the correct messages was sent via PostMessage.
  FakeMessagingInterface* iface =
      (FakeMessagingInterface*)fs_.ppapi()->GetMessagingInterface();
  VarArrayInterface* array_iface = fs_.ppapi()->GetVarArrayInterface();
  VarInterface* var_iface = fs_.ppapi()->GetVarInterface();
  VarArrayBufferInterface* buffer_iface =
      fs_.ppapi()->GetVarArrayBufferInterface();

  // Verify that exactly one message was sent of type PP_VARTYPE_ARRAY
  EXPECT_EQ(1, iface->messages.size());
  PP_Var array = iface->messages[0];
  ASSERT_EQ(PP_VARTYPE_ARRAY, array.type);

  // Verify that the array contains two element, the prefix,
  // and an ArrayBuffer containing the message.
  ASSERT_EQ(2, array_iface->GetLength(array));
  PP_Var item0 = array_iface->Get(array, 0);
  PP_Var item1 = array_iface->Get(array, 1);
  ASSERT_EQ(PP_VARTYPE_STRING, item0.type);
  ASSERT_EQ(PP_VARTYPE_ARRAY_BUFFER, item1.type);
  uint32_t len = 0;
  ASSERT_STREQ("jspipe1", var_iface->VarToUtf8(item0, &len));
  ASSERT_EQ(0, memcmp(message, buffer_iface->Map(item1), strlen(message)));
  var_iface->Release(item0);
  var_iface->Release(item1);
}

static int ki_ioctl_wrapper(int fd, int request, ...) {
  va_list ap;
  va_start(ap, request);
  int rtn = ki_ioctl(fd, request, ap);
  va_end(ap);
  return rtn;
}

static int JSPipeWrite(int fd, const char* string) {
  struct tioc_nacl_input_string input;
  input.buffer = string;
  input.length = strlen(input.buffer);
  return ki_ioctl_wrapper(fd, TIOCNACLINPUT, &input);
}

// Returns:
//   0 -> Not readable
//   1 -> Readable
//  -1 -> Error occured
static int IsReadable(int fd) {
  struct timeval timeout = {0, 0};
  fd_set readfds;
  fd_set errorfds;
  FD_ZERO(&readfds);
  FD_ZERO(&errorfds);
  FD_SET(fd, &readfds);
  FD_SET(fd, &errorfds);
  int rtn = ki_select(fd + 1, &readfds, NULL, &errorfds, &timeout);
  if (rtn == 0)
    return 0;  // not readable
  if (rtn != 1)
    return -1;  // error
  if (FD_ISSET(fd, &errorfds))
    return -2;  // error
  if (!FD_ISSET(fd, &readfds))
    return -3;  // error
  return 1;     // readable
}

TEST_F(JSPipeTest, JSPipeSelect) {
  struct timeval timeout;
  fd_set readfds;
  fd_set writefds;
  fd_set errorfds;

  int pipe_fd = ki_open("/dev/jspipe1", O_RDONLY);
  ASSERT_GT(pipe_fd, 0) << "jspipe1 open failed: " << errno;

  FD_ZERO(&readfds);
  FD_ZERO(&errorfds);
  FD_SET(pipe_fd, &readfds);
  FD_SET(pipe_fd, &errorfds);
  // 10 millisecond timeout
  timeout.tv_sec = 0;
  timeout.tv_usec = 10 * 1000;
  // Should timeout when no input is available.
  int rtn = ki_select(pipe_fd + 1, &readfds, NULL, &errorfds, &timeout);
  ASSERT_EQ(0, rtn) << "select failed: " << rtn << " err=" << strerror(errno);
  ASSERT_FALSE(FD_ISSET(pipe_fd, &readfds));
  ASSERT_FALSE(FD_ISSET(pipe_fd, &errorfds));

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&errorfds);
  FD_SET(pipe_fd, &readfds);
  FD_SET(pipe_fd, &writefds);
  FD_SET(pipe_fd, &errorfds);
  // Pipe should be writable on startup.
  rtn = ki_select(pipe_fd + 1, &readfds, &writefds, &errorfds, NULL);
  ASSERT_EQ(1, rtn);
  ASSERT_TRUE(FD_ISSET(pipe_fd, &writefds));
  ASSERT_FALSE(FD_ISSET(pipe_fd, &readfds));
  ASSERT_FALSE(FD_ISSET(pipe_fd, &errorfds));

  // Send 4 bytes to the pipe
  ASSERT_EQ(0, JSPipeWrite(pipe_fd, "test"));

  // Pipe should now be readable
  ASSERT_EQ(1, IsReadable(pipe_fd));

  ki_close(pipe_fd);
}

}
