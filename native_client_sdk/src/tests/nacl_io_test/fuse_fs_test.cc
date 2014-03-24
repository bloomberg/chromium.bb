// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "nacl_io/fuse.h"
#include "nacl_io/fusefs/fuse_fs.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_intercept.h"
#include "nacl_io/kernel_proxy.h"

using namespace nacl_io;

namespace {

class FuseFsForTesting : public FuseFs {
 public:
  explicit FuseFsForTesting(fuse_operations* fuse_ops) {
    FsInitArgs args;
    args.fuse_ops = fuse_ops;
    EXPECT_EQ(0, Init(args));
  }
};

// Implementation of a simple flat memory filesystem.
struct File {
  std::string name;
  std::vector<uint8_t> data;
};

typedef std::vector<File> Files;
Files g_files;

bool IsValidPath(const char* path) {
  if (path == NULL)
    return false;

  if (strlen(path) <= 1)
    return false;

  if (path[0] != '/')
    return false;

  return true;
}

File* FindFile(const char* path) {
  if (!IsValidPath(path))
    return NULL;

  for (Files::iterator iter = g_files.begin(); iter != g_files.end(); ++iter) {
    if (iter->name == &path[1])
      return &*iter;
  }

  return NULL;
}

int testfs_getattr(const char* path, struct stat* stbuf) {
  memset(stbuf, 0, sizeof(struct stat));

  if (strcmp(path, "/") == 0) {
    stbuf->st_mode = S_IFDIR | 0755;
    return 0;
  }

  File* file = FindFile(path);
  if (file == NULL)
    return -ENOENT;

  stbuf->st_mode = S_IFREG | 0666;
  stbuf->st_size = file->data.size();
  return 0;
}

int testfs_readdir(const char* path,
                   void* buf,
                   fuse_fill_dir_t filler,
                   off_t offset,
                   struct fuse_file_info*) {
  if (strcmp(path, "/") != 0)
    return -ENOENT;

  filler(buf, ".", NULL, 0);
  filler(buf, "..", NULL, 0);
  for (Files::iterator iter = g_files.begin(); iter != g_files.end(); ++iter) {
    filler(buf, iter->name.c_str(), NULL, 0);
  }
  return 0;
}

int testfs_create(const char* path, mode_t, struct fuse_file_info* fi) {
  if (!IsValidPath(path))
    return -ENOENT;

  File* file = FindFile(path);
  if (file != NULL) {
    if (fi->flags & O_EXCL)
      return -EEXIST;
  } else {
    g_files.push_back(File());
    file = &g_files.back();
    file->name = &path[1];  // Skip initial /
  }

  return 0;
}

int testfs_open(const char* path, struct fuse_file_info*) {
  // open is only called to open an existing file, otherwise create is
  // called. We don't need to do any additional work here, the path will be
  // passed to any other operations.
  return FindFile(path) != NULL;
}

int testfs_read(const char* path,
                char* buf,
                size_t size,
                off_t offset,
                struct fuse_file_info* fi) {
  File* file = FindFile(path);
  if (file == NULL)
    return -ENOENT;

  size_t filesize = file->data.size();
  // Trying to read past the end of the file.
  if (offset >= filesize)
    return 0;

  if (offset + size > filesize)
    size = filesize - offset;

  memcpy(buf, file->data.data() + offset, size);
  return size;
}

int testfs_write(const char* path,
                 const char* buf,
                 size_t size,
                 off_t offset,
                 struct fuse_file_info*) {
  File* file = FindFile(path);
  if (file == NULL)
    return -ENOENT;

  size_t filesize = file->data.size();

  if (offset + size > filesize)
    file->data.resize(offset + size);

  memcpy(file->data.data() + offset, buf, size);
  return size;
}

const char hello_world[] = "Hello, World!\n";

fuse_operations g_fuse_operations = {
  0,  // flag_nopath
  0,  // flag_reserved
  NULL,  // init
  NULL,  // destroy
  NULL,  // access
  testfs_create,  // create
  NULL,  // fgetattr
  NULL,  // fsync
  NULL,  // ftruncate
  testfs_getattr,  // getattr
  NULL,  // mkdir
  NULL,  // mknod
  testfs_open,  // open
  NULL,  // opendir
  testfs_read,  // read
  testfs_readdir,  // readdir
  NULL,  // release
  NULL,  // releasedir
  NULL,  // rename
  NULL,  // rmdir
  NULL,  // truncate
  NULL,  // unlink
  testfs_write,  // write
};

class FuseFsTest : public ::testing::Test {
 public:
  FuseFsTest();

  void SetUp();

 protected:
  FuseFsForTesting fs_;
};

FuseFsTest::FuseFsTest() : fs_(&g_fuse_operations) {}

void FuseFsTest::SetUp() {
  // Reset the filesystem.
  g_files.clear();

  // Add a built-in file.
  size_t hello_len = strlen(hello_world);

  File hello;
  hello.name = "hello";
  hello.data.resize(hello_len);
  memcpy(hello.data.data(), hello_world, hello_len);
  g_files.push_back(hello);
}

}  // namespace

TEST_F(FuseFsTest, OpenAndRead) {
  ScopedNode node;
  ASSERT_EQ(0, fs_.Open(Path("/hello"), O_RDONLY, &node));

  char buffer[15] = {0};
  int bytes_read = 0;
  HandleAttr attr;
  ASSERT_EQ(0, node->Read(attr, &buffer[0], sizeof(buffer), &bytes_read));
  // FUSE always fills the buffer (padding with \0) unless in direct_io mode.
  ASSERT_EQ(sizeof(buffer), bytes_read);
  ASSERT_STREQ(hello_world, buffer);
}

TEST_F(FuseFsTest, CreateAndWrite) {
  ScopedNode node;
  ASSERT_EQ(0, fs_.Open(Path("/foobar"), O_RDWR | O_CREAT, &node));

  HandleAttr attr;
  const char message[] = "Something interesting";
  int bytes_written;
  ASSERT_EQ(0, node->Write(attr, &message[0], strlen(message), &bytes_written));
  ASSERT_EQ(bytes_written, strlen(message));

  // Now try to read the data back.
  char buffer[40] = {0};
  int bytes_read = 0;
  ASSERT_EQ(0, node->Read(attr, &buffer[0], sizeof(buffer), &bytes_read));
  // FUSE always fills the buffer (padding with \0) unless in direct_io mode.
  ASSERT_EQ(sizeof(buffer), bytes_read);
  ASSERT_STREQ(message, buffer);
}

TEST_F(FuseFsTest, GetStat) {
  struct stat statbuf;
  ScopedNode node;

  ASSERT_EQ(0, fs_.Open(Path("/hello"), O_RDONLY, &node));
  EXPECT_EQ(0, node->GetStat(&statbuf));
  EXPECT_EQ(S_IFREG, statbuf.st_mode & S_IFMT);
  EXPECT_EQ(0666, statbuf.st_mode & ~S_IFMT);
  EXPECT_EQ(strlen(hello_world), statbuf.st_size);

  ASSERT_EQ(0, fs_.Open(Path("/"), O_RDONLY, &node));
  EXPECT_EQ(0, node->GetStat(&statbuf));
  EXPECT_EQ(S_IFDIR, statbuf.st_mode & S_IFMT);
  EXPECT_EQ(0755, statbuf.st_mode & ~S_IFMT);

  // Create a file and stat.
  ASSERT_EQ(0, fs_.Open(Path("/foobar"), O_RDWR | O_CREAT, &node));
  EXPECT_EQ(0, node->GetStat(&statbuf));
  EXPECT_EQ(S_IFREG, statbuf.st_mode & S_IFMT);
  EXPECT_EQ(0666, statbuf.st_mode & ~S_IFMT);
  EXPECT_EQ(0, statbuf.st_size);
}

TEST_F(FuseFsTest, GetDents) {
  ScopedNode root;

  ASSERT_EQ(0, fs_.Open(Path("/"), O_RDONLY, &root));

  struct dirent entries[4];
  int bytes_read;

  // Try reading everything.
  ASSERT_EQ(0, root->GetDents(0, &entries[0], sizeof(entries), &bytes_read));
  ASSERT_EQ(3 * sizeof(dirent), bytes_read);
  EXPECT_STREQ(".", entries[0].d_name);
  EXPECT_STREQ("..", entries[1].d_name);
  EXPECT_STREQ("hello", entries[2].d_name);

  // Try reading from an offset.
  memset(&entries, 0, sizeof(entries));
  ASSERT_EQ(0, root->GetDents(sizeof(dirent), &entries[0], 2 * sizeof(dirent),
                              &bytes_read));
  ASSERT_EQ(2 * sizeof(dirent), bytes_read);
  EXPECT_STREQ("..", entries[0].d_name);
  EXPECT_STREQ("hello", entries[1].d_name);

  // Add a file and read again.
  ScopedNode node;
  ASSERT_EQ(0, fs_.Open(Path("/foobar"), O_RDWR | O_CREAT, &node));
  ASSERT_EQ(0, root->GetDents(0, &entries[0], sizeof(entries), &bytes_read));
  ASSERT_EQ(4 * sizeof(dirent), bytes_read);
  EXPECT_STREQ(".", entries[0].d_name);
  EXPECT_STREQ("..", entries[1].d_name);
  EXPECT_STREQ("hello", entries[2].d_name);
  EXPECT_STREQ("foobar", entries[3].d_name);
}

namespace {

class KernelProxyFuseTest : public ::testing::Test {
 public:
  KernelProxyFuseTest() {}

  void SetUp();
  void TearDown();

 private:
  KernelProxy kp_;
};

void KernelProxyFuseTest::SetUp() {
  ASSERT_EQ(0, ki_push_state_for_testing());
  ASSERT_EQ(0, ki_init(&kp_));

  // Register a fuse filesystem.
  nacl_io_register_fs_type("flatfs", &g_fuse_operations);

  // Unmount the passthrough FS and mount our fuse filesystem.
  EXPECT_EQ(0, kp_.umount("/"));
  EXPECT_EQ(0, kp_.mount("", "/", "flatfs", 0, NULL));
}

void KernelProxyFuseTest::TearDown() {
  nacl_io_unregister_fs_type("flatfs");
  ki_uninit();
}

}  // namespace

TEST_F(KernelProxyFuseTest, Basic) {
  // Write a file.
  int fd = ki_open("/hello", O_WRONLY | O_CREAT);
  ASSERT_GT(fd, -1);
  ASSERT_EQ(sizeof(hello_world),
            ki_write(fd, hello_world, sizeof(hello_world)));
  EXPECT_EQ(0, ki_close(fd));

  // Then read it back in.
  fd = ki_open("/hello", O_RDONLY);
  ASSERT_GT(fd, -1);

  char buffer[30];
  memset(buffer, 0, sizeof(buffer));
  ASSERT_EQ(sizeof(buffer), ki_read(fd, buffer, sizeof(buffer)));
  EXPECT_STREQ(hello_world, buffer);
  EXPECT_EQ(0, ki_close(fd));
}
