/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#include <map>
#include <string>

#include "nacl_mounts/kernel_handle.h"
#include "nacl_mounts/kernel_object.h"
#include "nacl_mounts/mount.h"
#include "nacl_mounts/path.h"

#define __STDC__ 1
#include "gtest/gtest.h"

int g_MountCnt = 0;
int g_HandleCnt = 0;

class MountRefMock : public Mount {
 public:
  MountRefMock() : Mount() { g_MountCnt++; }
  ~MountRefMock() { g_MountCnt--; }

 public:
  MountNode* Open(const Path& path, int mode) { return NULL; }
  int Close(MountNode* node) { return 0; }
  int Unlink(const Path& path) { return 0; }
  int Mkdir(const Path& path, int permissions) { return 0; }
  int Rmdir(const Path& path) { return 0; }
};

class KernelHandleRefMock : public KernelHandle {
 public:
  KernelHandleRefMock(Mount* mnt, MountNode* node, int flags) :
      KernelHandle(mnt, node, flags) {
    g_HandleCnt++;
  }
  ~KernelHandleRefMock() {
    g_HandleCnt--;
  }
};

TEST(KernelObject, Referencing) {
  KernelObject* proxy = new KernelObject();
  Mount* mnt = new MountRefMock();
  KernelHandle* handle = new KernelHandleRefMock(mnt, NULL, 0);
  KernelHandle* handle2 = new KernelHandleRefMock(mnt, NULL, 0);

  // Objects should have one ref when we start
  EXPECT_EQ(1, mnt->RefCount());
  EXPECT_EQ(1, handle->RefCount());

  // Objects should have two refs when we get here
  int fd1 = proxy->AllocateFD(handle);
  EXPECT_EQ(2, mnt->RefCount());
  EXPECT_EQ(2, handle->RefCount());

  // If we "dup" the handle, we should bump the refs
  int fd2 = proxy->AllocateFD(handle);
  EXPECT_EQ(3, mnt->RefCount());
  EXPECT_EQ(3, handle->RefCount());

  // If use a new handle with the same values... bump the refs
  int fd3 = proxy->AllocateFD(handle2);
  EXPECT_EQ(4, mnt->RefCount());
  EXPECT_EQ(2, handle2->RefCount());

  // Handles are expectd to come out in order
  EXPECT_EQ(0, fd1);
  EXPECT_EQ(1, fd2);
  EXPECT_EQ(2, fd3);

  // We should find the handle by either fd
  EXPECT_EQ(handle, proxy->AcquireHandle(fd1));
  EXPECT_EQ(handle, proxy->AcquireHandle(fd2));

  // A non existant fd should fail
  EXPECT_EQ(NULL, proxy->AcquireHandle(-1));
  EXPECT_EQ(EBADF, errno);
  EXPECT_EQ(NULL, proxy->AcquireHandle(100));
  EXPECT_EQ(EBADF, errno);

  // Acquiring the handle, should have ref'd it
  EXPECT_EQ(4, mnt->RefCount());
  EXPECT_EQ(5, handle->RefCount());

  // Release the handle for each call to acquire
  proxy->ReleaseHandle(handle);
  proxy->ReleaseHandle(handle);

  // Release the handle for each time we constructed something
  proxy->ReleaseHandle(handle);
  proxy->ReleaseHandle(handle2);
  proxy->ReleaseMount(mnt);

  // We should now only have references used by the KernelProxy
  EXPECT_EQ(2, handle->RefCount());
  EXPECT_EQ(1, handle2->RefCount());
  EXPECT_EQ(3, mnt->RefCount());

  EXPECT_EQ(2, g_HandleCnt);
  EXPECT_EQ(1, g_MountCnt);

  proxy->FreeFD(fd1);
  EXPECT_EQ(2, g_HandleCnt);
  EXPECT_EQ(1, g_MountCnt);

  proxy->FreeFD(fd3);
  EXPECT_EQ(1, g_HandleCnt);
  EXPECT_EQ(1, g_MountCnt);

  proxy->FreeFD(fd2);
  EXPECT_EQ(0, g_HandleCnt);
  EXPECT_EQ(0, g_MountCnt);
}

