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
#include "nacl_io/kernel_object.h"
#include "nacl_io/mount.h"
#include "nacl_io/path.h"

#include "gtest/gtest.h"

namespace {

class MountRefMock : public Mount {
 public:
  MountRefMock(int* mount_count, int* handle_count)
      : mount_count(mount_count),
        handle_count(handle_count) {
    (*mount_count)++;
  }

  ~MountRefMock() {
    (*mount_count)--;
  }

 public:
  MountNode* Open(const Path& path, int mode) { return NULL; }
  int Close(MountNode* node) { return 0; }
  int Unlink(const Path& path) { return 0; }
  int Mkdir(const Path& path, int permissions) { return 0; }
  int Rmdir(const Path& path) { return 0; }
  int Remove(const Path& path) { return 0; }

 public:
  int* mount_count;
  int* handle_count;
};

class KernelHandleRefMock : public KernelHandle {
 public:
  KernelHandleRefMock(Mount* mnt, MountNode* node, int flags)
      : KernelHandle(mnt, node, flags) {
    MountRefMock* mock_mount = static_cast<MountRefMock*>(mnt);
    (*mock_mount->handle_count)++;
  }

  ~KernelHandleRefMock() {
    MountRefMock* mock_mount = static_cast<MountRefMock*>(mount_);
    (*mock_mount->handle_count)--;
  }
};

class KernelObjectTest : public ::testing::Test {
 public:
  KernelObjectTest()
      : mount_count(0),
        handle_count(0) {
    proxy = new KernelObject;
    mnt = new MountRefMock(&mount_count, &handle_count);
  }

  ~KernelObjectTest() {
    // mnt is ref-counted, it doesn't need to be explicitly deleted.
    delete proxy;
  }

  KernelObject* proxy;
  MountRefMock* mnt;
  int mount_count;
  int handle_count;
};

}  // namespace


TEST_F(KernelObjectTest, Referencing) {
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

  // Handles are expected to come out in order
  EXPECT_EQ(0, fd1);
  EXPECT_EQ(1, fd2);
  EXPECT_EQ(2, fd3);

  // We should find the handle by either fd
  EXPECT_EQ(handle, proxy->AcquireHandle(fd1));
  EXPECT_EQ(handle, proxy->AcquireHandle(fd2));

  // A non existent fd should fail
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

  EXPECT_EQ(2, handle_count);
  EXPECT_EQ(1, mount_count);

  proxy->FreeFD(fd1);
  EXPECT_EQ(2, handle_count);
  EXPECT_EQ(1, mount_count);

  proxy->FreeFD(fd3);
  EXPECT_EQ(1, handle_count);
  EXPECT_EQ(1, mount_count);

  proxy->FreeFD(fd2);
  EXPECT_EQ(0, handle_count);
  EXPECT_EQ(0, mount_count);
}

TEST_F(KernelObjectTest, FreeAndReassignFD) {
  EXPECT_EQ(0, handle_count);

  KernelHandle* handle = new KernelHandleRefMock(mnt, NULL, 0);

  EXPECT_EQ(1, handle_count);
  EXPECT_EQ(1, handle->RefCount());

  // Assign to a non-existent FD
  proxy->FreeAndReassignFD(2, handle);
  EXPECT_EQ((KernelHandle*)NULL, proxy->AcquireHandle(0));
  EXPECT_EQ((KernelHandle*)NULL, proxy->AcquireHandle(1));
  EXPECT_EQ(handle, proxy->AcquireHandle(2));
  proxy->ReleaseHandle(handle);

  EXPECT_EQ(1, handle_count);
  EXPECT_EQ(2, handle->RefCount());

  proxy->FreeAndReassignFD(0, handle);
  EXPECT_EQ(handle, proxy->AcquireHandle(0));
  EXPECT_EQ((KernelHandle*)NULL, proxy->AcquireHandle(1));
  EXPECT_EQ(handle, proxy->AcquireHandle(2));
  proxy->ReleaseHandle(handle);
  proxy->ReleaseHandle(handle);

  EXPECT_EQ(1, handle_count);
  EXPECT_EQ(3, handle->RefCount());

  proxy->FreeFD(0);
  proxy->FreeFD(2);
  proxy->ReleaseHandle(handle);  // handle is constructed with a refcount of 1.

  EXPECT_EQ(0, handle_count);
}
