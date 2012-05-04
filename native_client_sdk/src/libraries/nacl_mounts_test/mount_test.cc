/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include <string>

#include "nacl_mounts/mount.h"
#include "nacl_mounts/mount_mem.h"

#define __STDC__ 1
#include "gtest/gtest.h"

class MountMock : public MountMem {
 public:
  MountMock()
    : MountMem(),
      nodes_(0) {
    StringMap_t map;
    Init(1, map);
  };

  MountNode *AllocateData(int mode) {
    nodes_++;
    return MountMem::AllocateData(mode);
  }

  void ReleaseNode(MountNode* node) {
    if (!node->Release()) {
      nodes_--;
    }
  }

  int nodes_;
};

#define NULL_NODE ((MountNode *) NULL)

TEST(MountTest, Sanity) {
  MountMock* mnt = new MountMock();
  MountNode* file;
  MountNode* root;

  char buf1[1024];

  EXPECT_EQ(0, mnt->nodes_);

  // Fail to open non existant file
  EXPECT_EQ(NULL_NODE, mnt->Open(Path("/foo"), O_RDWR));
  EXPECT_EQ(errno, ENOENT);

  // Create a file
  file =  mnt->Open(Path("/foo"), O_RDWR | O_CREAT);
  EXPECT_NE(NULL_NODE, file);
  if (file == NULL) return;
  EXPECT_EQ(2, file->RefCount());
  EXPECT_EQ(1, mnt->nodes_);

  // Open the root directory
  root = mnt->Open(Path("/"), O_RDWR);
  EXPECT_NE(NULL_NODE, root);
  if (NULL != root) {
    struct dirent dirs[2];
    int len = root->GetDents(0, dirs, sizeof(dirs));
    EXPECT_EQ(sizeof(struct dirent), len);
  }

  // Fail to re-create the same file
  EXPECT_EQ(NULL_NODE, mnt->Open(Path("/foo"), O_RDWR | O_CREAT | O_EXCL));
  EXPECT_EQ(errno, EEXIST);
  EXPECT_EQ(1, mnt->nodes_);

  // Fail to create a directory with the same name
  EXPECT_EQ(-1, mnt->Mkdir(Path("/foo"), O_RDWR));
  EXPECT_EQ(errno, EEXIST);

  // Attempt to READ/WRITE
  EXPECT_EQ(0, file->GetSize());
  EXPECT_EQ(sizeof(buf1), file->Write(0, buf1, sizeof(buf1)));
  EXPECT_EQ(sizeof(buf1), file->GetSize());
  EXPECT_EQ(sizeof(buf1), file->Read(0, buf1, sizeof(buf1)));

  // Attempt to open the same file
  EXPECT_EQ(file, mnt->Open(Path("/foo"), O_RDWR | O_CREAT));
  EXPECT_EQ(sizeof(buf1), file->GetSize());
  EXPECT_EQ(3, file->RefCount());
  EXPECT_EQ(1, mnt->nodes_);

  // Attempt to close and delete the file
  EXPECT_EQ(0, mnt->Close(file));
  EXPECT_EQ(1, mnt->nodes_);
  EXPECT_EQ(0, mnt->Unlink(Path("/foo")));
  EXPECT_EQ(1, mnt->nodes_);
  EXPECT_EQ(-1, mnt->Unlink(Path("/foo")));
  EXPECT_EQ(1, mnt->nodes_);
  EXPECT_EQ(errno, ENOENT);
  EXPECT_EQ(0, mnt->Close(file));
  EXPECT_EQ(0, mnt->nodes_);

  // Recreate foo as a directory
  EXPECT_EQ(0, mnt->Mkdir(Path("/foo"), O_RDWR));

  // Create a file (exclusively)
  file =  mnt->Open(Path("/foo/bar"), O_RDWR | O_CREAT | O_EXCL);
  EXPECT_NE(NULL_NODE, file);
  if (NULL == file) return;

  // Attempt to delete the directory
  EXPECT_EQ(-1, mnt->Rmdir(Path("/foo")));
  EXPECT_EQ(errno, ENOTEMPTY);

  // Unlink the file, then delete the directory
  EXPECT_EQ(0, mnt->Unlink(Path("/foo/bar")));
  EXPECT_EQ(0, mnt->Rmdir(Path("/foo")));
  EXPECT_EQ(1, mnt->nodes_);
  EXPECT_EQ(0, mnt->Close(file));
  EXPECT_EQ(0, mnt->nodes_);

  // Verify the directory is gone
  file =  mnt->Open(Path("/foo"), O_RDWR);
  EXPECT_EQ(NULL_NODE, file);
  EXPECT_EQ(errno, ENOENT);
}
