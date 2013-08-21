// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string>

#include "gtest/gtest.h"
#include "mount_dev_mock.h"
#include "nacl_io/ioctl.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"
#include "nacl_io/mount.h"
#include "nacl_io/mount_dev.h"
#include "nacl_io/mount_mem.h"
#include "nacl_io/osdirent.h"

using namespace nacl_io;

namespace {

class TtyTest : public ::testing::Test {
 public:
  void SetUp() {
    ki_init(&kp_);
    ASSERT_EQ(0, mnt_.Access(Path("/tty"), R_OK | W_OK));
    ASSERT_EQ(EACCES, mnt_.Access(Path("/tty"), X_OK));
    ASSERT_EQ(0, mnt_.Open(Path("/tty"), O_RDWR, &dev_tty_));
    ASSERT_NE(NULL_NODE, dev_tty_.get());
  }

  void TearDown() {
    ki_uninit();
  }

 protected:
  KernelProxy kp_;
  MountDevMock mnt_;
  ScopedMountNode dev_tty_;
};

TEST_F(TtyTest, InvalidIoctl) {
  // 123 is not a valid ioctl request.
  EXPECT_EQ(EINVAL, dev_tty_->Ioctl(123, NULL));
}

TEST_F(TtyTest, TtyInput) {
  // TIOCNACLPREFIX is, it should set the prefix.
  std::string prefix("__my_awesome_prefix__");
  EXPECT_EQ(0, dev_tty_->Ioctl(TIOCNACLPREFIX,
                               const_cast<char*>(prefix.c_str())));

  // Now let's try sending some data over.
  // First we create the message.
  std::string content("hello, how are you?\n");
  std::string message = prefix.append(content);
  struct tioc_nacl_input_string packaged_message;
  packaged_message.length = message.size();
  packaged_message.buffer = message.data();

  // Now we make buffer we'll read into.
  // We fill the buffer and a backup buffer with arbitrary data
  // and compare them after reading to make sure read doesn't
  // clobber parts of the buffer it shouldn't.
  int bytes_read;
  char buffer[100];
  char backup_buffer[100];
  memset(buffer, 'a', 100);
  memset(backup_buffer, 'a', 100);

  // Now we actually send the data
  EXPECT_EQ(0, dev_tty_->Ioctl(TIOCNACLINPUT,
                               reinterpret_cast<char*>(&packaged_message)));

  // We read a small chunk first to ensure it doesn't give us
  // more than we ask for.
  EXPECT_EQ(0, dev_tty_->Read(0, buffer, 5, &bytes_read));
  EXPECT_EQ(bytes_read, 5);
  EXPECT_EQ(0, memcmp(content.data(), buffer, 5));
  EXPECT_EQ(0, memcmp(buffer + 5, backup_buffer + 5, 95));

  // Now we ask for more data than is left in the tty, to ensure
  // it doesn't give us more than is there.
  EXPECT_EQ(0, dev_tty_->Read(0, buffer + 5, 95, &bytes_read));
  EXPECT_EQ(bytes_read, content.size() - 5);
  EXPECT_EQ(0, memcmp(content.data(), buffer, content.size()));
  EXPECT_EQ(0, memcmp(buffer + content.size(),
                      backup_buffer + content.size(),
                      100 - content.size()));
}

TEST_F(TtyTest, InvalidPrefix) {
  std::string prefix("__my_awesome_prefix__");
  ASSERT_EQ(0, dev_tty_->Ioctl(TIOCNACLPREFIX,
                               const_cast<char*>(prefix.c_str())));

  // Now we try to send something with an invalid prefix
  std::string bogus_message("Woah there, this message has no valid prefix");
  struct tioc_nacl_input_string bogus_pack;
  bogus_pack.length = bogus_message.size();
  bogus_pack.buffer = bogus_message.data();
  ASSERT_EQ(ENOTTY, dev_tty_->Ioctl(TIOCNACLINPUT,
                                    reinterpret_cast<char*>(&bogus_pack)));
}

// Returns:
//   0 -> Not readable
//   1 -> Readable
//  -1 -> Error occured
static int IsReadable(int fd) {
  struct timeval timeout = { 0, 0 };
  fd_set readfds;
  fd_set errorfds;
  FD_ZERO(&readfds);
  FD_ZERO(&errorfds);
  FD_SET(fd, &readfds);
  FD_SET(fd, &errorfds);
  int rtn = ki_select(fd + 1, &readfds, NULL, &errorfds, &timeout);
  if (rtn == 0)
    return 0; // not readable
  if (rtn != 1)
    return -1; // error
  if (FD_ISSET(fd, &errorfds))
    return -1; // error
  if (!FD_ISSET(fd, &readfds))
    return -1; // error
  return 1; // readable
}

TEST_F(TtyTest, TtySelect) {
  struct timeval timeout;
  fd_set readfds;
  fd_set writefds;
  fd_set errorfds;

  int tty_fd = ki_open("/dev/tty", O_RDONLY);
  ASSERT_TRUE(tty_fd >= 0) << "tty open failed: " << errno;

  FD_ZERO(&readfds);
  FD_ZERO(&errorfds);
  FD_SET(tty_fd, &readfds);
  FD_SET(tty_fd, &errorfds);
  // 10 millisecond timeout
  timeout.tv_sec = 0;
  timeout.tv_usec = 10 * 1000;
  // Should timeout when no input is available.
  int rtn = ki_select(tty_fd + 1, &readfds, NULL, &errorfds, &timeout);
  ASSERT_EQ(rtn, 0) << "select failed: " << rtn << " err=" << strerror(errno);
  ASSERT_FALSE(FD_ISSET(tty_fd, &readfds));
  ASSERT_FALSE(FD_ISSET(tty_fd, &errorfds));

  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&errorfds);
  FD_SET(tty_fd, &readfds);
  FD_SET(tty_fd, &writefds);
  FD_SET(tty_fd, &errorfds);
  // TTY should be writable on startup.
  rtn = ki_select(tty_fd + 1, &readfds, &writefds, &errorfds, NULL);
  ASSERT_EQ(rtn, 1);
  ASSERT_TRUE(FD_ISSET(tty_fd, &writefds));
  ASSERT_FALSE(FD_ISSET(tty_fd, &readfds));
  ASSERT_FALSE(FD_ISSET(tty_fd, &errorfds));

  // Send 4 bytes to TTY input
  struct tioc_nacl_input_string input;
  input.buffer = "test";
  input.length = 4;
  char* ioctl_arg = reinterpret_cast<char*>(&input);
  ASSERT_EQ(0, ki_ioctl(tty_fd, TIOCNACLINPUT, ioctl_arg));

  // TTY should not be readable until newline in written
  ASSERT_EQ(IsReadable(tty_fd), 0);

  input.buffer = "\n";
  input.length = 1;
  ASSERT_EQ(0, ki_ioctl(tty_fd, TIOCNACLINPUT, ioctl_arg));

  // TTY should now be readable
  ASSERT_EQ(IsReadable(tty_fd), 1);

  ki_close(tty_fd);
}

}
