/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "nacl_mounts/kernel_proxy.h"
#include "nacl_mounts/mount_node.h"
#include "nacl_mounts/mount_node_dir.h"
#include "nacl_mounts/mount_node_mem.h"

#define __STDC__ 1
#include "gtest/gtest.h"

#define NULL_NODE ((MountNode *) NULL)

static int s_AllocNum = 0;

class MockMemory : public MountNodeMem {
 public:
  MockMemory() : MountNodeMem(NULL, 0, 0) {
    s_AllocNum++;
  }

  ~MockMemory() {
    s_AllocNum--;
  }

  bool Init(int mode) {
    return MountNodeMem::Init(mode,0,0);
  }
  int AddChild(const std::string& name, MountNode *node) {
    return MountNodeMem::AddChild(name, node);
  }
  int RemoveChild(const std::string& name) {
    return MountNodeMem::RemoveChild(name);
  }
  MountNode* FindChild(const std::string& name) {
    return MountNodeMem::FindChild(name);
  }
  void Link() { MountNodeMem::Link(); }
  void Unlink() { MountNodeMem::Unlink(); }
};

class MockDir : public MountNodeDir {
 public:
  MockDir() : MountNodeDir(NULL, 0, 0) {
    s_AllocNum++;
  }

  ~MockDir() {
    s_AllocNum--;
  }

  bool Init(int mode) {
    return MountNodeDir::Init(mode,0,0);
  }
  int AddChild(const std::string& name, MountNode *node) {
    return MountNodeDir::AddChild(name, node);
  }
  int RemoveChild(const std::string& name) {
    return MountNodeDir::RemoveChild(name);
  }
  MountNode* FindChild(const std::string& name) {
    return MountNodeDir::FindChild(name);
  }
  void Link() { MountNodeDir::Link(); }
  void Unlink() { MountNodeDir::Unlink(); }
};

TEST(MountNodeTest, File) {
  MockMemory *file = new MockMemory;

  EXPECT_EQ(true, file->Init(S_IREAD | S_IWRITE));

  // Test properties
  EXPECT_EQ(0, file->GetLinks());
  EXPECT_EQ(S_IREAD | S_IWRITE, file->GetMode());
  EXPECT_EQ(_S_IFREG, file->GetType());
  EXPECT_EQ(false, file->IsaDir());
  EXPECT_EQ(true, file->IsaFile());
  EXPECT_EQ(false, file->IsaTTY());
  EXPECT_EQ(1, file->RefCount());

  // Test IO
  char buf1[1024];
  char buf2[1024 * 2];
  for (int a=0; a < sizeof(buf1); a++)
    buf1[a] = a;
  memset(buf2, 0, sizeof(buf2));

  EXPECT_EQ(0, file->GetSize());
  EXPECT_EQ(0, file->Read(0, buf2, sizeof(buf2)));
  EXPECT_EQ(0, file->GetSize());
  EXPECT_EQ(sizeof(buf1), file->Write(0, buf1, sizeof(buf1)));
  EXPECT_EQ(sizeof(buf1), file->GetSize());
  EXPECT_EQ(sizeof(buf1), file->Read(0, buf2, sizeof(buf2)));
  EXPECT_EQ(0, memcmp(buf1, buf2, sizeof(buf1)));

  struct stat s;
  EXPECT_EQ(0, file->GetStat(&s));
  EXPECT_EQ(sizeof(buf1), s.st_size);

  // Directory operations should fail
  struct dirent d;
  EXPECT_EQ(-1, file->GetDents(0, &d, sizeof(d)));
  EXPECT_EQ(errno, ENOTDIR);
  EXPECT_EQ(-1, file->AddChild("", file));
  EXPECT_EQ(errno, ENOTDIR);
  EXPECT_EQ(-1, file->RemoveChild(""));
  EXPECT_EQ(errno, ENOTDIR);
  EXPECT_EQ(NULL_NODE, file->FindChild(""));
  EXPECT_EQ(errno, ENOTDIR);

  delete file;
}

TEST(MountNodeTest, Directory) {
  MockDir *root = new MockDir();
  root->Init(S_IREAD | S_IWRITE);

  // Test properties
  EXPECT_EQ(0, root->GetLinks());
  EXPECT_EQ(S_IREAD | S_IWRITE, root->GetMode());
  EXPECT_EQ(_S_IFDIR, root->GetType());
  EXPECT_EQ(true, root->IsaDir());
  EXPECT_EQ(false, root->IsaFile());
  EXPECT_EQ(false, root->IsaTTY());
  EXPECT_EQ(1, root->RefCount());

  // IO operations should fail
  char buf1[1024];
  EXPECT_EQ(0, root->GetSize());
  EXPECT_EQ(-1, root->Read(0, buf1, sizeof(buf1)));
  EXPECT_EQ(errno, EISDIR);
  EXPECT_EQ(-1, root->Write(0, buf1, sizeof(buf1)));
  EXPECT_EQ(errno, EISDIR);

  // Test directory operations
  MockMemory* file = new MockMemory;
  EXPECT_EQ(true, file->Init(S_IREAD | S_IWRITE));

  EXPECT_EQ(1, root->RefCount());
  EXPECT_EQ(1, file->RefCount());
  EXPECT_EQ(0, root->AddChild("F1", file));
  EXPECT_EQ(1, file->GetLinks());
  EXPECT_EQ(2, file->RefCount());

  // Test that the directory is there
  struct dirent d;
  EXPECT_EQ(sizeof(d), root->GetDents(0, &d, sizeof(d)));
  EXPECT_EQ(0, strcmp("F1", d.d_name));
  EXPECT_EQ(0, root->GetDents(sizeof(d), &d, sizeof(d)));

  EXPECT_EQ(0, root->AddChild("F2", file));
  EXPECT_EQ(2, file->GetLinks());
  EXPECT_EQ(3, file->RefCount());
  EXPECT_EQ(-1, root->AddChild("F1", file));
  EXPECT_EQ(EEXIST, errno);
  EXPECT_EQ(2, file->GetLinks());

  EXPECT_EQ(2, s_AllocNum);
  EXPECT_NE(NULL_NODE, root->FindChild("F1"));
  EXPECT_NE(NULL_NODE, root->FindChild("F2"));
  EXPECT_EQ(NULL_NODE, root->FindChild("F3"));
  EXPECT_EQ(errno, ENOENT);

  EXPECT_EQ(2, s_AllocNum);
  EXPECT_EQ(0, root->RemoveChild("F1"));
  EXPECT_EQ(1, file->GetLinks());
  EXPECT_EQ(2, file->RefCount());
  EXPECT_EQ(0, root->RemoveChild("F2"));
  EXPECT_EQ(0, file->GetLinks());
  EXPECT_EQ(1, file->RefCount());
  EXPECT_EQ(2, s_AllocNum);

  file->Release();
  EXPECT_EQ(1, s_AllocNum);
  root->Release();
  EXPECT_EQ(0, s_AllocNum);
}

