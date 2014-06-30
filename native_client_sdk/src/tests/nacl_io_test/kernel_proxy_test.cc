// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/stat.h>

#include <map>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "mock_fs.h"
#include "mock_node.h"

#include "nacl_io/filesystem.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"
#include "nacl_io/memfs/mem_fs.h"
#include "nacl_io/osmman.h"
#include "nacl_io/path.h"
#include "nacl_io/typed_fs_factory.h"

using namespace nacl_io;
using namespace sdk_util;

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::WithArgs;

namespace {

class KernelProxyTest_KernelProxy : public KernelProxy {
 public:
  Filesystem* RootFs() {
    ScopedFilesystem fs;
    Path path;

    AcquireFsAndRelPath("/", &fs, &path);
    return fs.get();
  }
};

class KernelProxyTest : public ::testing::Test {
 public:
  KernelProxyTest() {}

  void SetUp() {
    ASSERT_EQ(0, ki_push_state_for_testing());
    ASSERT_EQ(0, ki_init(&kp_));
    // Unmount the passthrough FS and mount a memfs.
    EXPECT_EQ(0, kp_.umount("/"));
    EXPECT_EQ(0, kp_.mount("", "/", "memfs", 0, NULL));
  }

  void TearDown() { ki_uninit(); }

 protected:
  KernelProxyTest_KernelProxy kp_;
};

}  // namespace

static int ki_fcntl_wrapper(int fd, int request, ...) {
  va_list ap;
  va_start(ap, request);
  int rtn = ki_fcntl(fd, request, ap);
  va_end(ap);
  return rtn;
}

/**
 * Test for fcntl commands F_SETFD and F_GETFD.  This
 * is tested here rather than in the mount_node tests
 * since the fd flags are not stored in the kernel_handle
 * or the filesystem node but directly in the FD mapping.
 */
TEST_F(KernelProxyTest, Fcntl_GETFD) {
  int fd = ki_open("/test", O_RDWR | O_CREAT);
  ASSERT_NE(-1, fd);

  // FD flags should start as zero.
  ASSERT_EQ(0, ki_fcntl_wrapper(fd, F_GETFD));

  // Check that setting FD_CLOEXEC works
  int flags = FD_CLOEXEC;
  ASSERT_EQ(0, ki_fcntl_wrapper(fd, F_SETFD, flags))
      << "fcntl failed with: " << strerror(errno);
  ASSERT_EQ(FD_CLOEXEC, ki_fcntl_wrapper(fd, F_GETFD));

  // Check that setting invalid flag causes EINVAL
  flags = FD_CLOEXEC + 1;
  ASSERT_EQ(-1, ki_fcntl_wrapper(fd, F_SETFD, flags));
  ASSERT_EQ(EINVAL, errno);
}

TEST_F(KernelProxyTest, FileLeak) {
  const size_t buffer_size = 1024;
  char filename[128];
  int garbage[buffer_size];

  MemFs* filesystem = (MemFs*)kp_.RootFs();
  ScopedNode root;

  ASSERT_EQ(0, filesystem->Open(Path("/"), O_RDONLY, &root));
  ASSERT_EQ(0, root->ChildCount());

  for (int file_num = 0; file_num < 4096; file_num++) {
    sprintf(filename, "/foo%i.tmp", file_num++);
    int fd = ki_open(filename, O_WRONLY | O_CREAT);
    ASSERT_GT(fd, -1);
    ASSERT_EQ(1, root->ChildCount());
    ASSERT_EQ(buffer_size, ki_write(fd, garbage, buffer_size));
    ki_close(fd);
    ASSERT_EQ(0, ki_remove(filename));
  }
  ASSERT_EQ(0, root->ChildCount());
}

static bool g_handler_called = false;
static void sighandler(int) { g_handler_called = true; }

TEST_F(KernelProxyTest, Sigaction) {
  struct sigaction action;
  struct sigaction oaction;
  memset(&action, 0, sizeof(action));

  // Invalid signum
  ASSERT_EQ(-1, ki_sigaction(-1, NULL, &oaction));
  ASSERT_EQ(-1, ki_sigaction(SIGSTOP, NULL, &oaction));
  ASSERT_EQ(EINVAL, errno);

  // Get existing handler
  memset(&oaction, 0, sizeof(oaction));
  ASSERT_EQ(0, ki_sigaction(SIGINT, NULL, &oaction));
  ASSERT_EQ(SIG_DFL, oaction.sa_handler);

  // Attempt to set handler for unsupported signum
  action.sa_handler = sighandler;
  ASSERT_EQ(-1, ki_sigaction(SIGINT, &action, NULL));
  ASSERT_EQ(EINVAL, errno);

  // Attempt to set handler for supported signum
  action.sa_handler = sighandler;
  ASSERT_EQ(0, ki_sigaction(SIGWINCH, &action, NULL));

  memset(&oaction, 0, sizeof(oaction));
  ASSERT_EQ(0, ki_sigaction(SIGWINCH, NULL, &oaction));
  ASSERT_EQ((sighandler_t*)sighandler, (sighandler_t*)oaction.sa_handler);
}

TEST_F(KernelProxyTest, KillSignals) {
  // SIGSEGV can't be sent via kill(2)
  ASSERT_EQ(-1, ki_kill(0, SIGSEGV)) << "kill(SEGV) failed to return an error";
  ASSERT_EQ(EINVAL, errno) << "kill(SEGV) failed to set errno to EINVAL";

  // Our implemenation should understand SIGWINCH
  ASSERT_EQ(0, ki_kill(0, SIGWINCH)) << "kill(SIGWINCH) failed: " << errno;

  // And USR1/USR2
  ASSERT_EQ(0, ki_kill(0, SIGUSR1)) << "kill(SIGUSR1) failed: " << errno;
  ASSERT_EQ(0, ki_kill(0, SIGUSR2)) << "kill(SIGUSR2) failed: " << errno;
}

TEST_F(KernelProxyTest, KillPIDValues) {
  // Any PID other than 0, -1 and getpid() should yield ESRCH
  // since there is only one valid process under NaCl
  int mypid = getpid();
  ASSERT_EQ(0, ki_kill(0, SIGWINCH));
  ASSERT_EQ(0, ki_kill(-1, SIGWINCH));
  ASSERT_EQ(0, ki_kill(mypid, SIGWINCH));

  // Don't use mypid + 1 since getpid() actually returns -1
  // when the IRT interface is missing (e.g. within chrome),
  // and 0 is always a valid PID when calling kill().
  int invalid_pid = mypid + 10;
  ASSERT_EQ(-1, ki_kill(invalid_pid, SIGWINCH));
  ASSERT_EQ(ESRCH, errno);
}

TEST_F(KernelProxyTest, SignalValues) {
  ASSERT_EQ(ki_signal(SIGSEGV, sighandler), SIG_ERR)
      << "registering SEGV handler didn't fail";
  ASSERT_EQ(errno, EINVAL) << "signal(SEGV) failed to set errno to EINVAL";

  ASSERT_EQ(ki_signal(-1, sighandler), SIG_ERR)
      << "registering handler for invalid signal didn't fail";
  ASSERT_EQ(errno, EINVAL) << "signal(-1) failed to set errno to EINVAL";
}

TEST_F(KernelProxyTest, SignalHandlerValues) {
  // Unsupported signal.
  ASSERT_NE(SIG_ERR, ki_signal(SIGSEGV, SIG_DFL));
  ASSERT_EQ(SIG_ERR, ki_signal(SIGSEGV, SIG_IGN));
  ASSERT_EQ(SIG_ERR, ki_signal(SIGSEGV, sighandler));

  // Supported signal.
  ASSERT_NE(SIG_ERR, ki_signal(SIGWINCH, SIG_DFL));
  ASSERT_NE(SIG_ERR, ki_signal(SIGWINCH, SIG_IGN));
  ASSERT_NE(SIG_ERR, ki_signal(SIGWINCH, sighandler));
}

TEST_F(KernelProxyTest, SignalSigwinch) {
  g_handler_called = false;

  // Register WINCH handler
  sighandler_t newsig = sighandler;
  sighandler_t oldsig = ki_signal(SIGWINCH, newsig);
  ASSERT_NE(oldsig, SIG_ERR);

  // Send signal.
  ki_kill(0, SIGWINCH);

  // Verify that handler was called
  EXPECT_TRUE(g_handler_called);

  // Restore existing handler
  oldsig = ki_signal(SIGWINCH, oldsig);

  // Verify the our newsig was returned as previous handler
  ASSERT_EQ(oldsig, newsig);
}

TEST_F(KernelProxyTest, Rename) {
  // Create a dummy file
  int file1 = ki_open("/test1.txt", O_RDWR | O_CREAT);
  ASSERT_GT(file1, -1);
  ASSERT_EQ(0, ki_close(file1));

  // Test the renaming works
  ASSERT_EQ(0, ki_rename("/test1.txt", "/test2.txt"));

  // Test that renaming across mount points fails
  ASSERT_EQ(0, ki_mount("", "/foo", "memfs", 0, ""));
  ASSERT_EQ(-1, ki_rename("/test2.txt", "/foo/test2.txt"));
  ASSERT_EQ(EXDEV, errno);
}

TEST_F(KernelProxyTest, WorkingDirectory) {
  char text[1024];

  text[0] = 0;
  ki_getcwd(text, sizeof(text));
  EXPECT_STREQ("/", text);

  char* alloc = ki_getwd(NULL);
  EXPECT_EQ((char*)NULL, alloc);
  EXPECT_EQ(EFAULT, errno);

  text[0] = 0;
  alloc = ki_getwd(text);
  EXPECT_STREQ("/", alloc);

  EXPECT_EQ(-1, ki_chdir("/foo"));
  EXPECT_EQ(ENOENT, errno);

  EXPECT_EQ(0, ki_chdir("/"));

  EXPECT_EQ(0, ki_mkdir("/foo", S_IREAD | S_IWRITE));
  EXPECT_EQ(-1, ki_mkdir("/foo", S_IREAD | S_IWRITE));
  EXPECT_EQ(EEXIST, errno);

  memset(text, 0, sizeof(text));
  EXPECT_EQ(0, ki_chdir("foo"));
  EXPECT_EQ(text, ki_getcwd(text, sizeof(text)));
  EXPECT_STREQ("/foo", text);

  memset(text, 0, sizeof(text));
  EXPECT_EQ(-1, ki_chdir("foo"));
  EXPECT_EQ(ENOENT, errno);
  EXPECT_EQ(0, ki_chdir(".."));
  EXPECT_EQ(0, ki_chdir("/foo"));
  EXPECT_EQ(text, ki_getcwd(text, sizeof(text)));
  EXPECT_STREQ("/foo", text);
}

TEST_F(KernelProxyTest, FDPathMapping) {
  char text[1024];

  int fd1, fd2, fd3, fd4, fd5;

  EXPECT_EQ(0, ki_mkdir("/foo", S_IREAD | S_IWRITE));
  EXPECT_EQ(0, ki_mkdir("/foo/bar", S_IREAD | S_IWRITE));
  EXPECT_EQ(0, ki_mkdir("/example", S_IREAD | S_IWRITE));
  ki_chdir("/foo");

  fd1 = ki_open("/example", O_RDONLY);
  EXPECT_NE(-1, fd1);
  EXPECT_EQ(ki_fchdir(fd1), 0);
  EXPECT_EQ(text, ki_getcwd(text, sizeof(text)));
  EXPECT_STREQ("/example", text);

  EXPECT_EQ(0, ki_chdir("/foo"));
  fd2 = ki_open("../example", O_RDONLY);
  EXPECT_NE(-1, fd2);
  EXPECT_EQ(0, ki_fchdir(fd2));
  EXPECT_EQ(text, ki_getcwd(text, sizeof(text)));
  EXPECT_STREQ("/example", text);

  EXPECT_EQ(0, ki_chdir("/foo"));
  fd3 = ki_open("../test", O_CREAT | O_RDWR);
  EXPECT_NE(-1, fd3);
  EXPECT_EQ(-1, ki_fchdir(fd3));
  EXPECT_EQ(ENOTDIR, errno);

  EXPECT_EQ(0, ki_chdir("/foo"));
  fd4 = ki_open("bar", O_RDONLY);
  EXPECT_EQ(0, ki_fchdir(fd4));
  EXPECT_EQ(text, ki_getcwd(text, sizeof(text)));
  EXPECT_STREQ("/foo/bar", text);
  EXPECT_EQ(0, ki_chdir("/example"));
  EXPECT_EQ(0, ki_fchdir(fd4));
  EXPECT_EQ(text, ki_getcwd(text, sizeof(text)));
  EXPECT_STREQ("/foo/bar", text);

  EXPECT_EQ(0, ki_chdir("/example"));
  fd5 = ki_dup(fd4);
  ASSERT_GT(fd5, -1);
  ASSERT_NE(fd4, fd5);
  EXPECT_EQ(0, ki_fchdir(fd5));
  EXPECT_EQ(text, ki_getcwd(text, sizeof(text)));
  EXPECT_STREQ("/foo/bar", text);

  fd5 = 123;

  EXPECT_EQ(0, ki_chdir("/example"));
  EXPECT_EQ(fd5, ki_dup2(fd4, fd5));
  EXPECT_EQ(0, ki_fchdir(fd5));
  EXPECT_EQ(text, ki_getcwd(text, sizeof(text)));
  EXPECT_STREQ("/foo/bar", text);
}

TEST_F(KernelProxyTest, MemMountIO) {
  char text[1024];
  int fd1, fd2, fd3;
  int len;

  // Fail to delete non existant "/foo"
  EXPECT_EQ(-1, ki_rmdir("/foo"));
  EXPECT_EQ(ENOENT, errno);

  // Create "/foo"
  EXPECT_EQ(0, ki_mkdir("/foo", S_IREAD | S_IWRITE));
  EXPECT_EQ(-1, ki_mkdir("/foo", S_IREAD | S_IWRITE));
  EXPECT_EQ(EEXIST, errno);

  // Delete "/foo"
  EXPECT_EQ(0, ki_rmdir("/foo"));

  // Recreate "/foo"
  EXPECT_EQ(0, ki_mkdir("/foo", S_IREAD | S_IWRITE));

  // Fail to open "/foo/bar"
  EXPECT_EQ(-1, ki_open("/foo/bar", O_RDONLY));
  EXPECT_EQ(ENOENT, errno);

  // Create bar "/foo/bar"
  fd1 = ki_open("/foo/bar", O_RDWR | O_CREAT);
  ASSERT_NE(-1, fd1);

  // Open (optionally create) bar "/foo/bar"
  fd2 = ki_open("/foo/bar", O_RDWR | O_CREAT);
  ASSERT_NE(-1, fd2);

  // Fail to exclusively create bar "/foo/bar"
  EXPECT_EQ(-1, ki_open("/foo/bar", O_RDONLY | O_CREAT | O_EXCL));
  EXPECT_EQ(EEXIST, errno);

  // Write hello and world to same node with different descriptors
  // so that we overwrite each other
  EXPECT_EQ(5, ki_write(fd2, "WORLD", 5));
  EXPECT_EQ(5, ki_write(fd1, "HELLO", 5));

  fd3 = ki_open("/foo/bar", O_RDONLY);
  ASSERT_NE(-1, fd3);

  len = ki_read(fd3, text, sizeof(text));
  ASSERT_EQ(5, len);
  text[len] = 0;
  EXPECT_STREQ("HELLO", text);
  EXPECT_EQ(0, ki_close(fd1));
  EXPECT_EQ(0, ki_close(fd2));

  fd1 = ki_open("/foo/bar", O_WRONLY | O_APPEND);
  ASSERT_NE(-1, fd1);
  EXPECT_EQ(5, ki_write(fd1, "WORLD", 5));

  len = ki_read(fd3, text, sizeof(text));
  ASSERT_EQ(5, len);
  text[len] = 0;
  EXPECT_STREQ("WORLD", text);

  fd2 = ki_open("/foo/bar", O_RDONLY);
  ASSERT_NE(-1, fd2);
  len = ki_read(fd2, text, sizeof(text));
  if (len > 0)
    text[len] = 0;
  EXPECT_EQ(10, len);
  EXPECT_STREQ("HELLOWORLD", text);
}

TEST_F(KernelProxyTest, MemMountFTruncate) {
  char text[1024];
  int fd1, fd2;

  // Open a file write only, write some text, then test that using a
  // separate file descriptor pointing to it that it is correctly
  // truncated at a specified number of bytes (2).
  fd1 = ki_open("/trunc", O_WRONLY | O_CREAT);
  ASSERT_NE(-1, fd1);
  fd2 = ki_open("/trunc", O_RDONLY);
  ASSERT_NE(-1, fd2);
  EXPECT_EQ(5, ki_write(fd1, "HELLO", 5));
  EXPECT_EQ(0, ki_ftruncate(fd1, 2));
  // Verify the remaining file (using fd2, opened pre-truncation) is
  // only 2 bytes in length.
  EXPECT_EQ(2, ki_read(fd2, text, sizeof(text)));
  EXPECT_EQ(0, ki_close(fd1));
  EXPECT_EQ(0, ki_close(fd2));
}

TEST_F(KernelProxyTest, MemMountTruncate) {
  char text[1024];
  int fd1;

  // Open a file write only, write some text, then test that by
  // referring to it by its path and truncating it we correctly truncate
  // it at a specified number of bytes (2).
  fd1 = ki_open("/trunc", O_WRONLY | O_CREAT);
  ASSERT_NE(-1, fd1);
  EXPECT_EQ(5, ki_write(fd1, "HELLO", 5));
  EXPECT_EQ(0, ki_close(fd1));
  EXPECT_EQ(0, ki_truncate("/trunc", 2));
  // Verify the text is only 2 bytes long with new file descriptor.
  fd1 = ki_open("/trunc", O_RDONLY);
  ASSERT_NE(-1, fd1);
  EXPECT_EQ(2, ki_read(fd1, text, sizeof(text)));
  EXPECT_EQ(0, ki_close(fd1));
}

TEST_F(KernelProxyTest, MemMountLseek) {
  int fd = ki_open("/foo", O_CREAT | O_RDWR);
  ASSERT_GT(fd, -1);
  ASSERT_EQ(9, ki_write(fd, "Some text", 9));

  ASSERT_EQ(9, ki_lseek(fd, 0, SEEK_CUR));
  ASSERT_EQ(9, ki_lseek(fd, 0, SEEK_END));
  ASSERT_EQ(-1, ki_lseek(fd, -1, SEEK_SET));
  ASSERT_EQ(EINVAL, errno);

  // Seek past end of file.
  ASSERT_EQ(13, ki_lseek(fd, 13, SEEK_SET));
  char buffer[4];
  memset(&buffer[0], 0xfe, 4);
  ASSERT_EQ(9, ki_lseek(fd, -4, SEEK_END));
  ASSERT_EQ(9, ki_lseek(fd, 0, SEEK_CUR));
  ASSERT_EQ(4, ki_read(fd, &buffer[0], 4));
  ASSERT_EQ(0, memcmp("\0\0\0\0", buffer, 4));
}

TEST_F(KernelProxyTest, CloseTwice) {
  int fd = ki_open("/foo", O_CREAT | O_RDWR);
  ASSERT_GT(fd, -1);

  EXPECT_EQ(9, ki_write(fd, "Some text", 9));

  int fd2 = ki_dup(fd);
  ASSERT_GT(fd2, -1);

  EXPECT_EQ(0, ki_close(fd));
  EXPECT_EQ(0, ki_close(fd2));
}

TEST_F(KernelProxyTest, MemMountDup) {
  int fd = ki_open("/foo", O_CREAT | O_RDWR);
  ASSERT_GT(fd, -1);

  int dup_fd = ki_dup(fd);
  ASSERT_NE(-1, dup_fd);

  ASSERT_EQ(9, ki_write(fd, "Some text", 9));
  ASSERT_EQ(9, ki_lseek(fd, 0, SEEK_CUR));
  ASSERT_EQ(9, ki_lseek(dup_fd, 0, SEEK_CUR));

  int dup2_fd = 123;
  ASSERT_EQ(dup2_fd, ki_dup2(fd, dup2_fd));
  ASSERT_EQ(9, ki_lseek(dup2_fd, 0, SEEK_CUR));

  int new_fd = ki_open("/bar", O_CREAT | O_RDWR);

  ASSERT_EQ(fd, ki_dup2(new_fd, fd));
  // fd, new_fd -> "/bar"
  // dup_fd, dup2_fd -> "/foo"

  // We should still be able to write to dup_fd (i.e. it should not be closed).
  ASSERT_EQ(4, ki_write(dup_fd, "more", 4));

  ASSERT_EQ(0, ki_close(dup2_fd));
  // fd, new_fd -> "/bar"
  // dup_fd -> "/foo"

  ASSERT_EQ(dup_fd, ki_dup2(fd, dup_fd));
  // fd, new_fd, dup_fd -> "/bar"
}

TEST_F(KernelProxyTest, Lstat) {
  int fd = ki_open("/foo", O_CREAT | O_RDWR);
  ASSERT_GT(fd, -1);
  ASSERT_EQ(0, ki_mkdir("/bar", S_IREAD | S_IWRITE));

  struct stat buf;
  EXPECT_EQ(0, ki_lstat("/foo", &buf));
  EXPECT_EQ(0, buf.st_size);
  EXPECT_TRUE(S_ISREG(buf.st_mode));

  EXPECT_EQ(0, ki_lstat("/bar", &buf));
  EXPECT_EQ(0, buf.st_size);
  EXPECT_TRUE(S_ISDIR(buf.st_mode));

  EXPECT_EQ(-1, ki_lstat("/no-such-file", &buf));
  EXPECT_EQ(ENOENT, errno);
}

TEST_F(KernelProxyTest, UseAfterClose) {
  int fd = ki_open("/dummy", O_CREAT | O_WRONLY);
  ASSERT_GT(fd, -1);
  EXPECT_EQ(5, ki_write(fd, "hello", 5));
  EXPECT_EQ(0, ki_close(fd));
  EXPECT_EQ(-1, ki_write(fd, "hello", 5));
  EXPECT_EQ(EBADF, errno);
}

namespace {

StringMap_t g_string_map;
bool g_fs_ioctl_called;
int g_fs_dev;

class KernelProxyMountTest_Filesystem : public MemFs {
 public:
  virtual Error Init(const FsInitArgs& args) {
    MemFs::Init(args);

    g_string_map = args.string_map;
    g_fs_dev = args.dev;

    if (g_string_map.find("false") != g_string_map.end())
      return EINVAL;
    return 0;
  }

  virtual Error Filesystem_VIoctl(int request, va_list arglist) {
    g_fs_ioctl_called = true;
    return 0;
  }

  friend class TypedFsFactory<KernelProxyMountTest_Filesystem>;
};

class KernelProxyMountTest_KernelProxy : public KernelProxy {
  virtual Error Init(PepperInterface* ppapi) {
    KernelProxy::Init(NULL);
    factories_["initfs"] = new TypedFsFactory<KernelProxyMountTest_Filesystem>;
    return 0;
  }
};

class KernelProxyMountTest : public ::testing::Test {
 public:
  KernelProxyMountTest() {}

  void SetUp() {
    g_string_map.clear();
    g_fs_dev = -1;
    g_fs_ioctl_called = false;

    ASSERT_EQ(0, ki_push_state_for_testing());
    ASSERT_EQ(0, ki_init(&kp_));
  }

  void TearDown() {
    g_string_map.clear();
    ki_uninit();
  }

 protected:
  KernelProxyMountTest_KernelProxy kp_;
};

// Helper function for calling ki_ioctl without having
// to construct a va_list.
int ki_ioctl_wrapper(int fd, int request, ...) {
  va_list ap;
  va_start(ap, request);
  int rtn = ki_ioctl(fd, request, ap);
  va_end(ap);
  return rtn;
}

}  // namespace

TEST_F(KernelProxyMountTest, MountInit) {
  int res1 = ki_mount("/", "/mnt1", "initfs", 0, "false,foo=bar");

  EXPECT_EQ("bar", g_string_map["foo"]);
  EXPECT_EQ(-1, res1);
  EXPECT_EQ(EINVAL, errno);

  int res2 = ki_mount("/", "/mnt2", "initfs", 0, "true,bar=foo,x=y");
  EXPECT_NE(-1, res2);
  EXPECT_EQ("y", g_string_map["x"]);
}

TEST_F(KernelProxyMountTest, MountAndIoctl) {
  ASSERT_EQ(0, ki_mount("/", "/mnt1", "initfs", 0, ""));
  ASSERT_NE(-1, g_fs_dev);

  char path[100];
  snprintf(path, 100, "dev/fs/%d", g_fs_dev);

  int fd = ki_open(path, O_RDONLY);
  ASSERT_GT(fd, -1);

  EXPECT_EQ(0, ki_ioctl_wrapper(fd, 0xdeadbeef));
  EXPECT_EQ(true, g_fs_ioctl_called);
}

static void mount_callback(const char* source,
                           const char* target,
                           const char* filesystemtype,
                           unsigned long mountflags,
                           const void* data,
                           dev_t dev,
                           void* user_data) {
  EXPECT_STREQ("/", source);
  EXPECT_STREQ("/mnt1", target);
  EXPECT_STREQ("initfs", filesystemtype);
  EXPECT_EQ(0, mountflags);
  EXPECT_STREQ("", (const char*) data);
  EXPECT_EQ(g_fs_dev, dev);

  bool* callback_called = static_cast<bool*>(user_data);
  *callback_called = true;
}

TEST_F(KernelProxyMountTest, MountCallback) {
  bool callback_called = false;
  kp_.SetMountCallback(&mount_callback, &callback_called);
  ASSERT_EQ(0, ki_mount("/", "/mnt1", "initfs", 0, ""));
  ASSERT_NE(-1, g_fs_dev);
  EXPECT_EQ(true, callback_called);
}

namespace {

int g_MMapCount = 0;

class KernelProxyMMapTest_Node : public Node {
 public:
  KernelProxyMMapTest_Node(Filesystem* filesystem)
      : Node(filesystem), node_mmap_count_(0) {
    EXPECT_EQ(0, Init(0));
  }

  virtual Error MMap(void* addr,
                     size_t length,
                     int prot,
                     int flags,
                     size_t offset,
                     void** out_addr) {
    node_mmap_count_++;
    switch (g_MMapCount++) {
      case 0:
        *out_addr = reinterpret_cast<void*>(0x1000);
        break;
      case 1:
        *out_addr = reinterpret_cast<void*>(0x2000);
        break;
      case 2:
        *out_addr = reinterpret_cast<void*>(0x3000);
        break;
      default:
        return EPERM;
    }

    return 0;
  }

 private:
  int node_mmap_count_;
};

class KernelProxyMMapTest_Filesystem : public Filesystem {
 public:
  virtual Error Access(const Path& path, int a_mode) { return 0; }
  virtual Error Open(const Path& path, int mode, ScopedNode* out_node) {
    out_node->reset(new KernelProxyMMapTest_Node(this));
    return 0;
  }

  virtual Error OpenResource(const Path& path, ScopedNode* out_node) {
    out_node->reset(NULL);
    return ENOSYS;
  }
  virtual Error Unlink(const Path& path) { return ENOSYS; }
  virtual Error Mkdir(const Path& path, int permissions) { return ENOSYS; }
  virtual Error Rmdir(const Path& path) { return ENOSYS; }
  virtual Error Remove(const Path& path) { return ENOSYS; }
  virtual Error Rename(const Path& path, const Path& newpath) { return ENOSYS; }

  friend class TypedFsFactory<KernelProxyMMapTest_Filesystem>;
};

class KernelProxyMMapTest_KernelProxy : public KernelProxy {
  virtual Error Init(PepperInterface* ppapi) {
    KernelProxy::Init(NULL);
    factories_["mmapfs"] = new TypedFsFactory<KernelProxyMMapTest_Filesystem>;
    return 0;
  }
};

class KernelProxyMMapTest : public ::testing::Test {
 public:
  KernelProxyMMapTest() {}

  void SetUp() {
    ASSERT_EQ(0, ki_push_state_for_testing());
    ASSERT_EQ(0, ki_init(&kp_));
  }

  void TearDown() { ki_uninit(); }

 private:
  KernelProxyMMapTest_KernelProxy kp_;
};

}  // namespace

TEST_F(KernelProxyMMapTest, MMap) {
  ASSERT_EQ(0, ki_umount("/"));
  ASSERT_EQ(0, ki_mount("", "/", "mmapfs", 0, NULL));
  int fd = ki_open("/file", O_RDWR | O_CREAT);
  ASSERT_NE(-1, fd);

  void* addr1 = ki_mmap(NULL, 0x800, PROT_READ, MAP_PRIVATE, fd, 0);
  ASSERT_EQ(reinterpret_cast<void*>(0x1000), addr1);
  ASSERT_EQ(1, g_MMapCount);

  void* addr2 = ki_mmap(NULL, 0x800, PROT_READ, MAP_PRIVATE, fd, 0);
  ASSERT_EQ(reinterpret_cast<void*>(0x2000), addr2);
  ASSERT_EQ(2, g_MMapCount);

  void* addr3 = ki_mmap(NULL, 0x800, PROT_READ, MAP_PRIVATE, fd, 0);
  ASSERT_EQ(reinterpret_cast<void*>(0x3000), addr3);
  ASSERT_EQ(3, g_MMapCount);

  ki_close(fd);

  // We no longer track mmap'd regions, so munmap is a no-op.
  ASSERT_EQ(0, ki_munmap(reinterpret_cast<void*>(0x1000), 0x2800));
  // We don't track regions, so the mmap count hasn't changed.
  ASSERT_EQ(3, g_MMapCount);
}

namespace {

class SingletonFsFactory : public FsFactory {
 public:
  SingletonFsFactory(const ScopedFilesystem& filesystem) : mount_(filesystem) {}

  virtual Error CreateFilesystem(const FsInitArgs& args,
                                 ScopedFilesystem* out_fs) {
    *out_fs = mount_;
    return 0;
  }

 private:
  ScopedFilesystem mount_;
};

class KernelProxyErrorTest_KernelProxy : public KernelProxy {
 public:
  KernelProxyErrorTest_KernelProxy() : fs_(new MockFs) {}

  virtual Error Init(PepperInterface* ppapi) {
    KernelProxy::Init(ppapi);
    factories_["testfs"] = new SingletonFsFactory(fs_);

    EXPECT_CALL(*fs_, Destroy()).Times(1);
    return 0;
  }

  ScopedRef<MockFs> fs() { return fs_; }

 private:
  ScopedRef<MockFs> fs_;
};

class KernelProxyErrorTest : public ::testing::Test {
 public:
  KernelProxyErrorTest() {}

  void SetUp() {
    ASSERT_EQ(0, ki_push_state_for_testing());
    ASSERT_EQ(0, ki_init(&kp_));
    // Unmount the passthrough FS and mount a testfs.
    EXPECT_EQ(0, kp_.umount("/"));
    EXPECT_EQ(0, kp_.mount("", "/", "testfs", 0, NULL));
  }

  void TearDown() { ki_uninit(); }

  ScopedRef<MockFs> fs() { return kp_.fs(); }

 private:
  KernelProxyErrorTest_KernelProxy kp_;
};

}  // namespace

TEST_F(KernelProxyErrorTest, WriteError) {
  ScopedRef<MockFs> mock_fs(fs());
  ScopedRef<MockNode> mock_node(new MockNode(&*mock_fs));
  EXPECT_CALL(*mock_fs, Open(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_node), Return(0)));

  EXPECT_CALL(*mock_node, Write(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(0),  // Wrote 0 bytes.
                      Return(1234)));       // Returned error 1234.

  EXPECT_CALL(*mock_node, Destroy()).Times(1);

  int fd = ki_open("/dummy", O_WRONLY);
  EXPECT_NE(0, fd);

  char buf[20];
  EXPECT_EQ(-1, ki_write(fd, &buf[0], 20));
  // The Filesystem should be able to return whatever error it wants and have it
  // propagate through.
  EXPECT_EQ(1234, errno);
}

TEST_F(KernelProxyErrorTest, ReadError) {
  ScopedRef<MockFs> mock_fs(fs());
  ScopedRef<MockNode> mock_node(new MockNode(&*mock_fs));
  EXPECT_CALL(*mock_fs, Open(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_node), Return(0)));

  EXPECT_CALL(*mock_node, Read(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(0),  // Read 0 bytes.
                      Return(1234)));       // Returned error 1234.

  EXPECT_CALL(*mock_node, Destroy()).Times(1);

  int fd = ki_open("/dummy", O_RDONLY);
  EXPECT_NE(0, fd);

  char buf[20];
  EXPECT_EQ(-1, ki_read(fd, &buf[0], 20));
  // The Filesystem should be able to return whatever error it wants and have it
  // propagate through.
  EXPECT_EQ(1234, errno);
}
