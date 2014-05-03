// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>

#include <set>
#include <string>

#include "gtest/gtest.h"

#include "nacl_io/devfs/dev_fs.h"
#include "nacl_io/dir_node.h"
#include "nacl_io/error.h"
#include "nacl_io/ioctl.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/kernel_proxy.h"
#include "nacl_io/memfs/mem_fs.h"
#include "nacl_io/memfs/mem_fs_node.h"
#include "nacl_io/node.h"
#include "nacl_io/osdirent.h"

#define NULL_NODE ((Node*)NULL)

using namespace nacl_io;

static int s_alloc_num = 0;

namespace {

class MemFsForTesting : public MemFs {
 public:
  MemFsForTesting() {
    FsInitArgs args(1);
    EXPECT_EQ(0, Init(args));
  }

  int num_nodes() { return inode_pool_.size(); }
};

class MemFsNodeForTesting : public MemFsNode {
 public:
  MemFsNodeForTesting() : MemFsNode(NULL) { s_alloc_num++; }

  ~MemFsNodeForTesting() { s_alloc_num--; }

  using MemFsNode::Init;
  using MemFsNode::AddChild;
  using MemFsNode::RemoveChild;
  using MemFsNode::FindChild;
};

class DirNodeForTesting : public DirNode {
 public:
  DirNodeForTesting() : DirNode(NULL) { s_alloc_num++; }

  ~DirNodeForTesting() { s_alloc_num--; }

  using DirNode::Init;
  using DirNode::AddChild;
  using DirNode::RemoveChild;
  using DirNode::FindChild;
};

}  // namespace

TEST(MemFsNodeTest, File) {
  MemFsNodeForTesting file;
  ScopedNode result_node;
  off_t result_size = 0;
  int result_bytes = 0;

  EXPECT_EQ(0, file.Init(0));

  // Test properties
  EXPECT_EQ(0, file.GetLinks());
  EXPECT_EQ(S_IRALL | S_IWALL, file.GetMode());
  EXPECT_EQ(S_IFREG, file.GetType());
  EXPECT_FALSE(file.IsaDir());
  EXPECT_TRUE(file.IsaFile());
  EXPECT_EQ(ENOTTY, file.Isatty());
  EXPECT_EQ(0, file.RefCount());

  // Test IO
  char buf1[1024];
  char buf2[1024 * 2];
  for (size_t a = 0; a < sizeof(buf1); a++)
    buf1[a] = a;
  memset(buf2, 0, sizeof(buf2));
  HandleAttr attr;

  EXPECT_EQ(0, file.GetSize(&result_size));
  EXPECT_EQ(0, result_size);
  EXPECT_EQ(0, file.Read(attr, buf2, sizeof(buf2), &result_bytes));
  EXPECT_EQ(0, result_bytes);
  EXPECT_EQ(0, file.GetSize(&result_size));
  EXPECT_EQ(0, result_size);
  EXPECT_EQ(0, file.Write(attr, buf1, sizeof(buf1), &result_bytes));
  EXPECT_EQ(sizeof(buf1), result_bytes);
  EXPECT_EQ(0, file.GetSize(&result_size));
  EXPECT_EQ(sizeof(buf1), result_size);
  EXPECT_EQ(0, file.Read(attr, buf2, sizeof(buf2), &result_bytes));
  EXPECT_EQ(sizeof(buf1), result_bytes);
  EXPECT_EQ(0, memcmp(buf1, buf2, sizeof(buf1)));

  struct stat s;
  EXPECT_EQ(0, file.GetStat(&s));
  EXPECT_LT(0, s.st_ino);  // 0 is an invalid inode number.
  EXPECT_EQ(sizeof(buf1), s.st_size);

  // Directory operations should fail
  struct dirent d;
  EXPECT_EQ(ENOTDIR, file.GetDents(0, &d, sizeof(d), &result_bytes));
  EXPECT_EQ(ENOTDIR, file.AddChild("", result_node));
  EXPECT_EQ(ENOTDIR, file.RemoveChild(""));
  EXPECT_EQ(ENOTDIR, file.FindChild("", &result_node));
  EXPECT_EQ(NULL_NODE, result_node.get());
}

TEST(MemFsNodeTest, FTruncate) {
  MemFsNodeForTesting file;
  off_t result_size = 0;
  int result_bytes = 0;

  char data[1024];
  char buffer[1024];
  char zero[1024];

  for (size_t a = 0; a < sizeof(data); a++)
    data[a] = a;
  memset(buffer, 0, sizeof(buffer));
  memset(zero, 0, sizeof(zero));
  HandleAttr attr;

  // Write the data to the file.
  ASSERT_EQ(0, file.Write(attr, data, sizeof(data), &result_bytes));
  ASSERT_EQ(sizeof(data), result_bytes);

  // Double the size of the file.
  EXPECT_EQ(0, file.FTruncate(sizeof(data) * 2));
  EXPECT_EQ(0, file.GetSize(&result_size));
  EXPECT_EQ(sizeof(data) * 2, result_size);

  // Read the first half of the file, it shouldn't have changed.
  EXPECT_EQ(0, file.Read(attr, buffer, sizeof(buffer), &result_bytes));
  EXPECT_EQ(sizeof(buffer), result_bytes);
  EXPECT_EQ(0, memcmp(buffer, data, sizeof(buffer)));

  // Read the second half of the file, it should be all zeroes.
  attr.offs = sizeof(data);
  EXPECT_EQ(0, file.Read(attr, buffer, sizeof(buffer), &result_bytes));
  EXPECT_EQ(sizeof(buffer), result_bytes);
  EXPECT_EQ(0, memcmp(buffer, zero, sizeof(buffer)));

  // Decrease the size of the file.
  EXPECT_EQ(0, file.FTruncate(100));
  EXPECT_EQ(0, file.GetSize(&result_size));
  EXPECT_EQ(100, result_size);

  // Data should still be there.
  attr.offs = 0;
  EXPECT_EQ(0, file.Read(attr, buffer, sizeof(buffer), &result_bytes));
  EXPECT_EQ(100, result_bytes);
  EXPECT_EQ(0, memcmp(buffer, data, 100));
}

TEST(MemFsNodeTest, Fcntl_GETFL) {
  MemFsNodeForTesting* node = new MemFsNodeForTesting();
  ScopedFilesystem fs(new MemFsForTesting());
  ScopedNode file(node);
  KernelHandle handle(fs, file);
  ASSERT_EQ(0, handle.Init(O_CREAT | O_APPEND));

  // Test invalid fcntl command.
  ASSERT_EQ(ENOSYS, handle.Fcntl(-1, NULL));

  // Test F_GETFL
  ASSERT_EQ(0, node->Init(0));
  int flags = 0;
  ASSERT_EQ(0, handle.Fcntl(F_GETFL, &flags));
  ASSERT_EQ(O_CREAT | O_APPEND, flags);

  // Test F_SETFL
  // Test adding of O_NONBLOCK
  flags = O_NONBLOCK | O_APPEND;
  ASSERT_EQ(0, handle.Fcntl(F_SETFL, NULL, flags));
  ASSERT_EQ(0, handle.Fcntl(F_GETFL, &flags));
  ASSERT_EQ(O_CREAT | O_APPEND | O_NONBLOCK, flags);

  // Clearing of O_APPEND should generate EPERM;
  flags = O_NONBLOCK;
  ASSERT_EQ(EPERM, handle.Fcntl(F_SETFL, NULL, flags));
}

TEST(MemFsNodeTest, Directory) {
  s_alloc_num = 0;
  DirNodeForTesting root;
  ScopedNode result_node;
  off_t result_size = 0;
  int result_bytes = 0;

  root.Init(0);

  // Test properties
  EXPECT_EQ(0, root.GetLinks());
  // Directories are always executable.
  EXPECT_EQ(S_IRALL | S_IWALL | S_IXALL, root.GetMode());
  EXPECT_EQ(S_IFDIR, root.GetType());
  EXPECT_TRUE(root.IsaDir());
  EXPECT_FALSE(root.IsaFile());
  EXPECT_EQ(ENOTTY, root.Isatty());
  EXPECT_EQ(0, root.RefCount());

  // IO operations should fail
  char buf1[1024];
  HandleAttr attr;
  EXPECT_EQ(0, root.GetSize(&result_size));
  EXPECT_EQ(0, result_size);
  EXPECT_EQ(EISDIR, root.Read(attr, buf1, sizeof(buf1), &result_bytes));
  EXPECT_EQ(EISDIR, root.Write(attr, buf1, sizeof(buf1), &result_bytes));

  // Test directory operations
  MemFsNodeForTesting* raw_file = new MemFsNodeForTesting;
  EXPECT_EQ(0, raw_file->Init(0));
  ScopedNode file(raw_file);

  EXPECT_EQ(0, root.RefCount());
  EXPECT_EQ(1, file->RefCount());
  EXPECT_EQ(0, root.AddChild("F1", file));
  EXPECT_EQ(1, file->GetLinks());
  EXPECT_EQ(2, file->RefCount());

  // Test that the directory is there
  const size_t kMaxDirents = 4;
  struct dirent d[kMaxDirents];
  EXPECT_EQ(0, root.GetDents(0, &d[0], sizeof(d), &result_bytes));

  {
    size_t num_dirents = result_bytes / sizeof(dirent);
    EXPECT_EQ(3, num_dirents);
    EXPECT_EQ(sizeof(dirent) * num_dirents, result_bytes);

    std::multiset<std::string> dirnames;
    for (int i = 0; i < num_dirents; ++i) {
      EXPECT_LT(0, d[i].d_ino);  // 0 is an invalid inode number.
      EXPECT_EQ(sizeof(dirent), d[i].d_off);
      EXPECT_EQ(sizeof(dirent), d[i].d_reclen);
      dirnames.insert(d[i].d_name);
    }

    EXPECT_EQ(1, dirnames.count("F1"));
    EXPECT_EQ(1, dirnames.count("."));
    EXPECT_EQ(1, dirnames.count(".."));
  }

  // There should only be 3 entries. Reading past that will return 0 bytes read.
  EXPECT_EQ(0, root.GetDents(sizeof(d), &d[0], sizeof(d), &result_bytes));
  EXPECT_EQ(0, result_bytes);

  EXPECT_EQ(0, root.AddChild("F2", file));
  EXPECT_EQ(2, file->GetLinks());
  EXPECT_EQ(3, file->RefCount());
  EXPECT_EQ(EEXIST, root.AddChild("F1", file));
  EXPECT_EQ(2, file->GetLinks());
  EXPECT_EQ(3, file->RefCount());

  EXPECT_EQ(2, s_alloc_num);
  EXPECT_EQ(0, root.FindChild("F1", &result_node));
  EXPECT_NE(NULL_NODE, result_node.get());
  EXPECT_EQ(0, root.FindChild("F2", &result_node));
  EXPECT_NE(NULL_NODE, result_node.get());
  EXPECT_EQ(ENOENT, root.FindChild("F3", &result_node));
  EXPECT_EQ(NULL_NODE, result_node.get());

  EXPECT_EQ(2, s_alloc_num);
  EXPECT_EQ(0, root.RemoveChild("F1"));
  EXPECT_EQ(1, file->GetLinks());
  EXPECT_EQ(2, file->RefCount());
  EXPECT_EQ(0, root.RemoveChild("F2"));
  EXPECT_EQ(0, file->GetLinks());
  EXPECT_EQ(1, file->RefCount());
  EXPECT_EQ(2, s_alloc_num);

  file.reset();
  EXPECT_EQ(1, s_alloc_num);
}

TEST(MemFsNodeTest, OpenMode) {
  MemFsNodeForTesting* node = new MemFsNodeForTesting();
  ScopedFilesystem fs(new MemFsForTesting());
  ScopedNode file(node);

  const char write_buf[] = "hello world";
  char read_buf[10];
  int byte_count = 0;

  // Write some data to the file
  {
    KernelHandle handle(fs, file);
    ASSERT_EQ(0, handle.Init(O_CREAT | O_WRONLY));
    ASSERT_EQ(0, handle.Write(write_buf, strlen(write_buf), &byte_count));
    ASSERT_EQ(byte_count, strlen(write_buf));
  }

  // Reading from the O_WRONLY handle should be impossible
  {
    byte_count = 0;
    KernelHandle handle(fs, file);
    ASSERT_EQ(0, handle.Init(O_WRONLY));
    ASSERT_EQ(EACCES, handle.Read(read_buf, 10, &byte_count));
    ASSERT_EQ(0, handle.Write(write_buf, strlen(write_buf), &byte_count));
    ASSERT_EQ(byte_count, strlen(write_buf));
  }

  // Writing to a O_RDONLY handle should fail
  {
    byte_count = 0;
    KernelHandle handle(fs, file);
    ASSERT_EQ(0, handle.Init(O_RDONLY));
    ASSERT_EQ(EACCES, handle.Write(write_buf, strlen(write_buf), &byte_count));
    ASSERT_EQ(0, handle.Read(read_buf, sizeof(read_buf), &byte_count));
    ASSERT_EQ(byte_count, sizeof(read_buf));
  }
}
