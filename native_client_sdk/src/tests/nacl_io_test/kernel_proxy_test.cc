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

#include "mount_mock.h"
#include "mount_node_mock.h"

#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"
#include "nacl_io/mount.h"
#include "nacl_io/mount_mem.h"
#include "nacl_io/osmman.h"
#include "nacl_io/path.h"
#include "nacl_io/typed_mount_factory.h"

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

class KernelProxyFriend : public KernelProxy {
 public:
  Mount* RootMount() {
    ScopedMount mnt;
    Path path;

    AcquireMountAndRelPath("/", &mnt, &path);
    return mnt.get();
  }
};

class KernelProxyTest : public ::testing::Test {
 public:
  KernelProxyTest() {}

  void SetUp() {
    ki_init(&kp_);
    // Unmount the passthrough FS and mount a memfs.
    EXPECT_EQ(0, kp_.umount("/"));
    EXPECT_EQ(0, kp_.mount("", "/", "memfs", 0, NULL));
  }

  void TearDown() {
    ki_uninit();
  }

 protected:
  KernelProxyFriend kp_;
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
 * or the mount node but directly in the FD mapping.
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

  MountMem* mount = (MountMem*)kp_.RootMount();
  ScopedMountNode root;

  ASSERT_EQ(0, mount->Open(Path("/"), O_RDONLY, &root));
  ASSERT_EQ(0, root->ChildCount());

  for (int file_num = 0; file_num < 4096; file_num++) {
    sprintf(filename, "/foo%i.tmp", file_num++);
    FILE* f = fopen(filename, "w");
    ASSERT_NE((FILE*)NULL, f);
    ASSERT_EQ(1, root->ChildCount());
    ASSERT_EQ(buffer_size, fwrite(garbage, 1, buffer_size, f));
    fclose(f);
    ASSERT_EQ(0, remove(filename));
  }
  ASSERT_EQ(0, root->ChildCount());
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

TEST_F(KernelProxyTest, MemMountLseek) {
  int fd = ki_open("/foo", O_CREAT | O_RDWR);
  ASSERT_GT(fd, -1);
  EXPECT_EQ(9, ki_write(fd, "Some text", 9));

  EXPECT_EQ(9, ki_lseek(fd, 0, SEEK_CUR));
  EXPECT_EQ(9, ki_lseek(fd, 0, SEEK_END));
  EXPECT_EQ(-1, ki_lseek(fd, -1, SEEK_SET));
  EXPECT_EQ(EINVAL, errno);

  // Seek past end of file.
  EXPECT_EQ(13, ki_lseek(fd, 13, SEEK_SET));
  char buffer[4];
  memset(&buffer[0], 0xfe, 4);
  EXPECT_EQ(9, ki_lseek(fd, -4, SEEK_END));
  EXPECT_EQ(9, ki_lseek(fd, 0, SEEK_CUR));
  EXPECT_EQ(4, ki_read(fd, &buffer[0], 4));
  EXPECT_EQ(0, memcmp("\0\0\0\0", buffer, 4));
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

namespace {

StringMap_t g_StringMap;

class MountMockInit : public MountMem {
 public:
  virtual Error Init(int dev, StringMap_t& args, PepperInterface* ppapi) {
    g_StringMap = args;
    if (args.find("false") != args.end())
      return EINVAL;
    return 0;
  }

  friend class TypedMountFactory<MountMockInit>;
};

class KernelProxyMountMock : public KernelProxy {
  virtual Error Init(PepperInterface* ppapi) {
    KernelProxy::Init(NULL);
    factories_["initfs"] = new TypedMountFactory<MountMockInit>;
    return 0;
  }
};

class KernelProxyMountTest : public ::testing::Test {
 public:
  KernelProxyMountTest() {}

  void SetUp() {
    ki_init(&kp_);
  }

  void TearDown() {
    ki_uninit();
  }

 private:
  KernelProxyMountMock kp_;
};

}  // namespace

TEST_F(KernelProxyMountTest, MountInit) {
  int res1 = ki_mount("/", "/mnt1", "initfs", 0, "false,foo=bar");

  EXPECT_EQ("bar", g_StringMap["foo"]);
  EXPECT_EQ(-1, res1);
  EXPECT_EQ(EINVAL, errno);

  int res2 = ki_mount("/", "/mnt2", "initfs", 0, "true,bar=foo,x=y");
  EXPECT_NE(-1, res2);
  EXPECT_EQ("y", g_StringMap["x"]);
}

namespace {

int g_MMapCount = 0;

class MountNodeMockMMap : public MountNode {
 public:
  MountNodeMockMMap(Mount* mount) : MountNode(mount), node_mmap_count_(0) {
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

class MountMockMMap : public Mount {
 public:
  virtual Error Access(const Path& path, int a_mode) { return 0; }
  virtual Error Open(const Path& path, int mode, ScopedMountNode* out_node) {
    out_node->reset(new MountNodeMockMMap(this));
    return 0;
  }

  virtual Error OpenResource(const Path& path, ScopedMountNode* out_node) {
    out_node->reset(NULL);
    return ENOSYS;
  }
  virtual Error Unlink(const Path& path) { return ENOSYS; }
  virtual Error Mkdir(const Path& path, int permissions) { return ENOSYS; }
  virtual Error Rmdir(const Path& path) { return ENOSYS; }
  virtual Error Remove(const Path& path) { return ENOSYS; }
  virtual Error Rename(const Path& path, const Path& newpath) { return ENOSYS; }

  friend class TypedMountFactory<MountMockMMap>;
};

class KernelProxyMockMMap : public KernelProxy {
  virtual Error Init(PepperInterface* ppapi) {
    KernelProxy::Init(NULL);
    factories_["mmapfs"] = new TypedMountFactory<MountMockMMap>;
    return 0;
  }
};

class KernelProxyMMapTest : public ::testing::Test {
 public:
  KernelProxyMMapTest() {}

  void SetUp() {
    ki_init(&kp_);
  }

  void TearDown() {
    ki_uninit();
  }

 private:
  KernelProxyMockMMap kp_;
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

class SingletonMountFactory : public MountFactory {
 public:
  SingletonMountFactory(const ScopedMount& mount) : mount_(mount) {}

  virtual Error CreateMount(int dev,
                            StringMap_t& args,
                            PepperInterface* ppapi,
                            ScopedMount* out_mount) {
    *out_mount = mount_;
    return 0;
  }

 private:
  ScopedMount mount_;
};

class KernelProxyError : public KernelProxy {
 public:
  KernelProxyError() : mnt_(new MountMock) {}

  virtual Error Init(PepperInterface* ppapi) {
    KernelProxy::Init(ppapi);
    factories_["testfs"] = new SingletonMountFactory(mnt_);

    EXPECT_CALL(*mnt_, Destroy()).Times(1);
    return 0;
  }

  ScopedRef<MountMock> mnt() { return mnt_; }

 private:
  ScopedRef<MountMock> mnt_;
};

class KernelProxyErrorTest : public ::testing::Test {
 public:
  KernelProxyErrorTest() {}

  void SetUp() {
    ki_init(&kp_);
    // Unmount the passthrough FS and mount a testfs.
    EXPECT_EQ(0, kp_.umount("/"));
    EXPECT_EQ(0, kp_.mount("", "/", "testfs", 0, NULL));
  }

  void TearDown() {
    ki_uninit();
  }

  ScopedRef<MountMock> mnt() { return kp_.mnt(); }

 private:
  KernelProxyError kp_;
};

}  // namespace

TEST_F(KernelProxyErrorTest, WriteError) {
  ScopedRef<MountMock> mock_mnt(mnt());
  ScopedRef<MountNodeMock> mock_node(new MountNodeMock(&*mock_mnt));
  EXPECT_CALL(*mock_mnt, Open(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_node), Return(0)));

  EXPECT_CALL(*mock_node, Write(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(0),  // Wrote 0 bytes.
                      Return(1234)));       // Returned error 1234.

  EXPECT_CALL(*mock_node, Destroy()).Times(1);

  int fd = ki_open("/dummy", O_WRONLY);
  EXPECT_NE(0, fd);

  char buf[20];
  EXPECT_EQ(-1, ki_write(fd, &buf[0], 20));
  // The Mount should be able to return whatever error it wants and have it
  // propagate through.
  EXPECT_EQ(1234, errno);
}

TEST_F(KernelProxyErrorTest, ReadError) {
  ScopedRef<MountMock> mock_mnt(mnt());
  ScopedRef<MountNodeMock> mock_node(new MountNodeMock(&*mock_mnt));
  EXPECT_CALL(*mock_mnt, Open(_, _, _))
      .WillOnce(DoAll(SetArgPointee<2>(mock_node), Return(0)));

  EXPECT_CALL(*mock_node, Read(_, _, _, _))
      .WillOnce(DoAll(SetArgPointee<3>(0),  // Read 0 bytes.
                      Return(1234)));       // Returned error 1234.

  EXPECT_CALL(*mock_node, Destroy()).Times(1);

  int fd = ki_open("/dummy", O_RDONLY);
  EXPECT_NE(0, fd);

  char buf[20];
  EXPECT_EQ(-1, ki_read(fd, &buf[0], 20));
  // The Mount should be able to return whatever error it wants and have it
  // propagate through.
  EXPECT_EQ(1234, errno);
}
