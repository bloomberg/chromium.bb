/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>

#include <map>
#include <string>

#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"
#include "nacl_io/mount.h"
#include "nacl_io/mount_mem.h"
#include "nacl_io/osmman.h"
#include "nacl_io/path.h"

#include "gtest/gtest.h"


class KernelProxyTest : public ::testing::Test {
 public:
  KernelProxyTest()
      : kp_(new KernelProxy) {
    ki_init(kp_);
    // Unmount the passthrough FS and mount a memfs.
    EXPECT_EQ(0, kp_->umount("/"));
    EXPECT_EQ(0, kp_->mount("", "/", "memfs", 0, NULL));
  }

  ~KernelProxyTest() {
    ki_uninit();
    delete kp_;
  }

 private:
  KernelProxy* kp_;
};


TEST_F(KernelProxyTest, WorkingDirectory) {
  char text[1024];

  text[0] = 0;
  ki_getcwd(text, sizeof(text));
  EXPECT_STREQ("/", text);

  char* alloc = ki_getwd(NULL);
  EXPECT_EQ((char *) NULL, alloc);
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

  // Create "/foo"
  EXPECT_EQ(0, ki_mkdir("/foo", S_IREAD | S_IWRITE));

  // Fail to open "/foo/bar"
  EXPECT_EQ(-1, ki_open("/foo/bar", O_RDONLY));
  EXPECT_EQ(ENOENT, errno);

  // Create bar "/foo/bar"
  fd1 = ki_open("/foo/bar", O_RDONLY | O_CREAT);
  EXPECT_NE(-1, fd1);

  // Open (optionally create) bar "/foo/bar"
  fd2 = ki_open("/foo/bar", O_RDONLY | O_CREAT);
  EXPECT_NE(-1, fd2);

  // Fail to exclusively create bar "/foo/bar"
  EXPECT_EQ(-1, ki_open("/foo/bar", O_RDONLY | O_CREAT | O_EXCL));
  EXPECT_EQ(EEXIST, errno);

  // Write hello and world to same node with different descriptors
  // so that we overwrite each other
  EXPECT_EQ(5, ki_write(fd2, "WORLD", 5));
  EXPECT_EQ(5, ki_write(fd1, "HELLO", 5));

  fd3 = ki_open("/foo/bar", O_WRONLY);
  EXPECT_NE(-1, fd3);

  len = ki_read(fd3, text, sizeof(text));
  if (len > -0) text[len] = 0;
  EXPECT_EQ(5, len);
  EXPECT_STREQ("HELLO", text);
  EXPECT_EQ(0, ki_close(fd1));
  EXPECT_EQ(0, ki_close(fd2));

  fd1 = ki_open("/foo/bar", O_WRONLY | O_APPEND);
  EXPECT_NE(-1, fd1);
  EXPECT_EQ(5, ki_write(fd1, "WORLD", 5));

  len = ki_read(fd3, text, sizeof(text));
  if (len >= 0) text[len] = 0;

  EXPECT_EQ(5, len);
  EXPECT_STREQ("WORLD", text);

  fd2 = ki_open("/foo/bar", O_RDONLY);
  EXPECT_NE(-1, fd2);
  len = ki_read(fd2, text, sizeof(text));
  if (len > 0) text[len] = 0;
  EXPECT_EQ(10, len);
  EXPECT_STREQ("HELLOWORLD", text);
}

TEST_F(KernelProxyTest, MemMountLseek) {
  int fd = ki_open("/foo", O_CREAT | O_RDWR);
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
  EXPECT_EQ(4, ki_read(fd, &buffer[0], 4));
  EXPECT_EQ(0, memcmp("\0\0\0\0", buffer, 4));
}

TEST_F(KernelProxyTest, CloseTwice) {
  int fd = ki_open("/foo", O_CREAT | O_RDWR);
  EXPECT_EQ(9, ki_write(fd, "Some text", 9));

  int fd2 = ki_dup(fd);
  EXPECT_NE(-1, fd2);

  EXPECT_EQ(0, ki_close(fd));
  EXPECT_EQ(0, ki_close(fd2));
}

TEST_F(KernelProxyTest, MemMountDup) {
  int fd = ki_open("/foo", O_CREAT | O_RDWR);

  int dup_fd = ki_dup(fd);
  EXPECT_NE(-1, dup_fd);

  EXPECT_EQ(9, ki_write(fd, "Some text", 9));
  EXPECT_EQ(9, ki_lseek(fd, 0, SEEK_CUR));
  EXPECT_EQ(9, ki_lseek(dup_fd, 0, SEEK_CUR));

  int dup2_fd = 123;
  EXPECT_EQ(dup2_fd, ki_dup2(fd, dup2_fd));
  EXPECT_EQ(9, ki_lseek(dup2_fd, 0, SEEK_CUR));

  int new_fd = ki_open("/bar", O_CREAT | O_RDWR);

  EXPECT_EQ(fd, ki_dup2(new_fd, fd));
  // fd, new_fd -> "/bar"
  // dup_fd, dup2_fd -> "/foo"

  // We should still be able to write to dup_fd (i.e. it should not be closed).
  EXPECT_EQ(4, ki_write(dup_fd, "more", 4));

  EXPECT_EQ(0, ki_close(dup2_fd));
  // fd, new_fd -> "/bar"
  // dup_fd -> "/foo"

  EXPECT_EQ(dup_fd, ki_dup2(fd, dup_fd));
  // fd, new_fd, dup_fd -> "/bar"
}


StringMap_t g_StringMap;

class MountMockInit : public MountMem {
 public:
  virtual bool Init(int dev, StringMap_t& args, PepperInterface* ppapi) {
    g_StringMap = args;
    if (args.find("false") != args.end())
      return false;
    return true;
  };
};

class KernelProxyMountMock : public KernelProxy {
  virtual void Init(PepperInterface* ppapi) {
    KernelProxy::Init(NULL);
    factories_["initfs"] = MountMockInit::Create<MountMockInit>;
  }
};

class KernelProxyMountTest : public ::testing::Test {
 public:
  KernelProxyMountTest()
      : kp_(new KernelProxyMountMock) {
    ki_init(kp_);
  }

  ~KernelProxyMountTest() {
    ki_uninit();
    delete kp_;
  }

 private:
  KernelProxy* kp_;
};

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
  MountNodeMockMMap(Mount* mount)
      : MountNode(mount),
        node_mmap_count_(0) {
    Init(0);
  }

  virtual void* MMap(void* addr, size_t length, int prot, int flags,
                     size_t offset) {
    node_mmap_count_++;
    switch (g_MMapCount++) {
      case 0: return reinterpret_cast<void*>(0x1000);
      case 1: return reinterpret_cast<void*>(0x2000);
      case 2: return reinterpret_cast<void*>(0x3000);
      default: return MAP_FAILED;
    }
  }

  virtual int Munmap(void* addr, size_t length) {
    EXPECT_GT(node_mmap_count_, 0);
    --node_mmap_count_;
    --g_MMapCount;
    return 0;
  }

 private:
  int node_mmap_count_;
};

class MountMockMMap : public Mount {
 public:
  virtual MountNode* Open(const Path& path, int mode) {
    MountNodeMockMMap* node = new MountNodeMockMMap(this);
    return node;
  }

  virtual MountNode* OpenResource(const Path& path) { return NULL; }
  virtual int Unlink(const Path& path) { return -1; }
  virtual int Mkdir(const Path& path, int permissions) { return -1; }
  virtual int Rmdir(const Path& path) { return -1; }
  virtual int Remove(const Path& path) { return -1; }
};

class KernelProxyMockMMap : public KernelProxy {
  virtual void Init(PepperInterface* ppapi) {
    KernelProxy::Init(NULL);
    factories_["mmapfs"] = MountMockInit::Create<MountMockMMap>;
  }
};

class KernelProxyMMapTest : public ::testing::Test {
 public:
  KernelProxyMMapTest()
      : kp_(new KernelProxyMockMMap) {
    ki_init(kp_);
  }

  ~KernelProxyMMapTest() {
    ki_uninit();
    delete kp_;
  }

 private:
  KernelProxy* kp_;
};

}  // namespace

TEST_F(KernelProxyMMapTest, MMap) {
  EXPECT_EQ(0, ki_umount("/"));
  EXPECT_EQ(0, ki_mount("", "/", "mmapfs", 0, NULL));
  int fd = ki_open("/file", O_RDWR | O_CREAT);
  EXPECT_NE(-1, fd);

  void* addr1 = ki_mmap(NULL, 0x800, PROT_READ, MAP_PRIVATE, fd, 0);
  EXPECT_EQ(reinterpret_cast<void*>(0x1000), addr1);
  EXPECT_EQ(1, g_MMapCount);

  void* addr2 = ki_mmap(NULL, 0x800, PROT_READ, MAP_PRIVATE, fd, 0);
  EXPECT_EQ(reinterpret_cast<void*>(0x2000), addr2);
  EXPECT_EQ(2, g_MMapCount);

  void* addr3 = ki_mmap(NULL, 0x800, PROT_READ, MAP_PRIVATE, fd, 0);
  EXPECT_EQ(reinterpret_cast<void*>(0x3000), addr3);
  EXPECT_EQ(3, g_MMapCount);

  // Unmap 0x2000 and 0x3000.
  EXPECT_EQ(0, ki_munmap(reinterpret_cast<void*>(0x2400), 0x1000));
  EXPECT_EQ(1, g_MMapCount);

  ki_close(fd);
  EXPECT_EQ(1, g_MMapCount);  // Closing the file doesn't unmap it.

  // Open a new file, should have the same fd.
  int fd2 = ki_open("/foo", O_RDWR | O_CREAT);
  EXPECT_EQ(fd, fd2);

  // Unmap 0x1000. This should unmap the old file, not the new file with the
  // same fd.
  EXPECT_EQ(0, ki_munmap(reinterpret_cast<void*>(0x1000), 0x800));
  EXPECT_EQ(0, g_MMapCount);
}
