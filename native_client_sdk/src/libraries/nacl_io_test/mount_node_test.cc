// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>

#include "nacl_io/error.h"
#include "nacl_io/ioctl.h"
#include "nacl_io/kernel_proxy.h"
#include "nacl_io/mount_dev.h"
#include "nacl_io/mount_node.h"
#include "nacl_io/mount_node_dir.h"
#include "nacl_io/mount_node_mem.h"
#include "nacl_io/osdirent.h"

#include "gtest/gtest.h"

#define NULL_NODE ((MountNode*) NULL)

using namespace nacl_io;

static int s_AllocNum = 0;

class MockMemory : public MountNodeMem {
 public:
  MockMemory() : MountNodeMem(NULL) { s_AllocNum++; }

  ~MockMemory() { s_AllocNum--; }

  Error Init(int mode) { return MountNodeMem::Init(mode); }
  Error AddChild(const std::string& name, const ScopedMountNode& node) {
    return MountNodeMem::AddChild(name, node);
  }
  Error RemoveChild(const std::string& name) {
    return MountNodeMem::RemoveChild(name);
  }
  Error FindChild(const std::string& name, ScopedMountNode* out_node) {
    return MountNodeMem::FindChild(name, out_node);
  }
  void Link() { MountNodeMem::Link(); }
  void Unlink() { MountNodeMem::Unlink(); }

 protected:
  using MountNodeMem::Init;
};

class MockDir : public MountNodeDir {
 public:
  MockDir() : MountNodeDir(NULL) { s_AllocNum++; }

  ~MockDir() { s_AllocNum--; }

  Error Init(int mode) { return MountNodeDir::Init(mode); }
  Error AddChild(const std::string& name, const ScopedMountNode& node) {
    return MountNodeDir::AddChild(name, node);
  }
  Error RemoveChild(const std::string& name) {
    return MountNodeDir::RemoveChild(name);
  }
  Error FindChild(const std::string& name, ScopedMountNode* out_node) {
    return MountNodeDir::FindChild(name, out_node);
  }
  void Link() { MountNodeDir::Link(); }
  void Unlink() { MountNodeDir::Unlink(); }

 protected:
  using MountNodeDir::Init;
};

TEST(MountNodeTest, File) {
  MockMemory* file = new MockMemory;
  ScopedMountNode result_node;
  size_t result_size = 0;
  int result_bytes = 0;

  EXPECT_EQ(0, file->Init(S_IREAD | S_IWRITE));

  // Test properties
  EXPECT_EQ(0, file->GetLinks());
  EXPECT_EQ(S_IREAD | S_IWRITE, file->GetMode());
  EXPECT_EQ(S_IFREG, file->GetType());
  EXPECT_FALSE(file->IsaDir());
  EXPECT_TRUE(file->IsaFile());
  EXPECT_FALSE(file->IsaTTY());
  EXPECT_EQ(0, file->RefCount());

  // Test IO
  char buf1[1024];
  char buf2[1024 * 2];
  for (size_t a = 0; a < sizeof(buf1); a++)
    buf1[a] = a;
  memset(buf2, 0, sizeof(buf2));

  EXPECT_EQ(0, file->GetSize(&result_size));
  EXPECT_EQ(0, result_size);
  EXPECT_EQ(0, file->Read(0, buf2, sizeof(buf2), &result_bytes));
  EXPECT_EQ(0, result_bytes);
  EXPECT_EQ(0, file->GetSize(&result_size));
  EXPECT_EQ(0, result_size);
  EXPECT_EQ(0, file->Write(0, buf1, sizeof(buf1), &result_bytes));
  EXPECT_EQ(sizeof(buf1), result_bytes);
  EXPECT_EQ(0, file->GetSize(&result_size));
  EXPECT_EQ(sizeof(buf1), result_size);
  EXPECT_EQ(0, file->Read(0, buf2, sizeof(buf2), &result_bytes));
  EXPECT_EQ(sizeof(buf1), result_bytes);
  EXPECT_EQ(0, memcmp(buf1, buf2, sizeof(buf1)));

  struct stat s;
  EXPECT_EQ(0, file->GetStat(&s));
  EXPECT_LT(0, s.st_ino);  // 0 is an invalid inode number.
  EXPECT_EQ(sizeof(buf1), s.st_size);

  // Directory operations should fail
  struct dirent d;
  EXPECT_EQ(ENOTDIR, file->GetDents(0, &d, sizeof(d), &result_bytes));
  EXPECT_EQ(ENOTDIR, file->AddChild("", result_node));
  EXPECT_EQ(ENOTDIR, file->RemoveChild(""));
  EXPECT_EQ(ENOTDIR, file->FindChild("", &result_node));
  EXPECT_EQ(NULL_NODE, result_node.get());

  delete file;
}

TEST(MountNodeTest, Directory) {
  MockDir* root = new MockDir();
  ScopedMountNode result_node;
  size_t result_size = 0;
  int result_bytes = 0;

  root->Init(S_IREAD | S_IWRITE);

  // Test properties
  EXPECT_EQ(0, root->GetLinks());
  EXPECT_EQ(S_IREAD | S_IWRITE, root->GetMode());
  EXPECT_EQ(S_IFDIR, root->GetType());
  EXPECT_TRUE(root->IsaDir());
  EXPECT_FALSE(root->IsaFile());
  EXPECT_FALSE(root->IsaTTY());
  EXPECT_EQ(0, root->RefCount());

  // IO operations should fail
  char buf1[1024];
  EXPECT_EQ(0, root->GetSize(&result_size));
  EXPECT_EQ(0, result_size);
  EXPECT_EQ(EISDIR, root->Read(0, buf1, sizeof(buf1), &result_bytes));
  EXPECT_EQ(EISDIR, root->Write(0, buf1, sizeof(buf1), &result_bytes));

  // Test directory operations
  MockMemory* raw_file = new MockMemory;
  EXPECT_EQ(0, raw_file->Init(S_IREAD | S_IWRITE));
  ScopedMountNode file(raw_file);

  EXPECT_EQ(0, root->RefCount());
  EXPECT_EQ(1, file->RefCount());
  EXPECT_EQ(0, root->AddChild("F1", file));
  EXPECT_EQ(1, file->GetLinks());
  EXPECT_EQ(2, file->RefCount());

  // Test that the directory is there
  struct dirent d;
  EXPECT_EQ(0, root->GetDents(0, &d, sizeof(d), &result_bytes));
  EXPECT_EQ(sizeof(d), result_bytes);
  EXPECT_LT(0, d.d_ino);  // 0 is an invalid inode number.
  EXPECT_EQ(sizeof(d), d.d_off);
  EXPECT_EQ(sizeof(d), d.d_reclen);
  EXPECT_EQ(0, strcmp("F1", d.d_name));
  EXPECT_EQ(0, root->GetDents(sizeof(d), &d, sizeof(d), &result_bytes));
  EXPECT_EQ(0, result_bytes);

  EXPECT_EQ(0, root->AddChild("F2", file));
  EXPECT_EQ(2, file->GetLinks());
  EXPECT_EQ(3, file->RefCount());
  EXPECT_EQ(EEXIST, root->AddChild("F1", file));
  EXPECT_EQ(2, file->GetLinks());
  EXPECT_EQ(3, file->RefCount());

  EXPECT_EQ(2, s_AllocNum);
  EXPECT_EQ(0, root->FindChild("F1", &result_node));
  EXPECT_NE(NULL_NODE, result_node.get());
  EXPECT_EQ(0, root->FindChild("F2", &result_node));
  EXPECT_NE(NULL_NODE, result_node.get());
  EXPECT_EQ(ENOENT, root->FindChild("F3", &result_node));
  EXPECT_EQ(NULL_NODE, result_node.get());

  EXPECT_EQ(2, s_AllocNum);
  EXPECT_EQ(0, root->RemoveChild("F1"));
  EXPECT_EQ(1, file->GetLinks());
  EXPECT_EQ(2, file->RefCount());
  EXPECT_EQ(0, root->RemoveChild("F2"));
  EXPECT_EQ(0, file->GetLinks());
  EXPECT_EQ(1, file->RefCount());
  EXPECT_EQ(2, s_AllocNum);

  file.reset();
  EXPECT_EQ(1, s_AllocNum);
}
