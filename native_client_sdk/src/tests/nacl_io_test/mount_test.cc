// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <string>

#include "gtest/gtest.h"
#include "mount_dev_mock.h"
#include "nacl_io/ioctl.h"
#include "nacl_io/kernel_handle.h"
#include "nacl_io/mount.h"
#include "nacl_io/mount_mem.h"
#include "nacl_io/osdirent.h"
#include "nacl_io/osunistd.h"

using namespace nacl_io;

namespace {

class MountMemMock : public MountMem {
 public:
  MountMemMock() {
    StringMap_t map;
    EXPECT_EQ(0, Init(1, map, NULL));
  }

  int num_nodes() { return (int) inode_pool_.size(); }
};

}  // namespace

TEST(MountTest, Sanity) {
  MountMemMock mnt;

  ScopedMountNode file;
  ScopedMountNode root;
  ScopedMountNode result_node;

  size_t result_size = 0;
  int result_bytes = 0;
  char buf1[1024];

  // A memory mount starts with one directory node: the root.
  EXPECT_EQ(1, mnt.num_nodes());

  // Fail to open non existent file
  EXPECT_EQ(ENOENT, mnt.Access(Path("/foo"), R_OK | W_OK));
  EXPECT_EQ(ENOENT, mnt.Open(Path("/foo"), O_RDWR, &result_node));
  EXPECT_EQ(NULL, result_node.get());
  EXPECT_EQ(1, mnt.num_nodes());

  // Create a file
  EXPECT_EQ(0, mnt.Open(Path("/foo"), O_RDWR | O_CREAT, &file));
  ASSERT_NE(NULL_NODE, file.get());

  // We now have a directory and a file.  The file has a two references
  // one returned to the test, one for the name->inode map.
  EXPECT_EQ(2, mnt.num_nodes());
  EXPECT_EQ(2, file->RefCount());
  EXPECT_EQ(0, mnt.Access(Path("/foo"), R_OK | W_OK));
  EXPECT_EQ(EACCES, mnt.Access(Path("/foo"), X_OK));

  // All access should be allowed on the root directory.
  EXPECT_EQ(0, mnt.Access(Path("/"), R_OK | W_OK | X_OK));
  // Open the root directory for write should fail.
  EXPECT_EQ(EISDIR, mnt.Open(Path("/"), O_RDWR, &root));
  EXPECT_EQ(2, mnt.num_nodes());

  // Open the root directory, should not create a new file
  EXPECT_EQ(0, mnt.Open(Path("/"), O_RDONLY, &root));
  EXPECT_EQ(2, mnt.num_nodes());
  ASSERT_NE(NULL_NODE, root.get());
  struct dirent dirs[4];
  int len;
  EXPECT_EQ(0, root->GetDents(0, dirs, sizeof(dirs), &len));
  // 3 == "foo", ".", ".."
  EXPECT_EQ(3 * sizeof(struct dirent), len);

  // Fail to re-create the same file
  EXPECT_EQ(EEXIST,
            mnt.Open(Path("/foo"), O_RDWR | O_CREAT | O_EXCL, &result_node));
  EXPECT_EQ(NULL_NODE, result_node.get());
  EXPECT_EQ(2, mnt.num_nodes());

  // Fail to create a directory with the same name
  EXPECT_EQ(EEXIST, mnt.Mkdir(Path("/foo"), O_RDWR));
  EXPECT_EQ(2, mnt.num_nodes());

  HandleAttr attrs;

  // Attempt to READ/WRITE
  EXPECT_EQ(0, file->GetSize(&result_size));
  EXPECT_EQ(0, result_size);
  EXPECT_EQ(0, file->Write(attrs, buf1, sizeof(buf1), &result_bytes));
  EXPECT_EQ(sizeof(buf1), result_bytes);
  EXPECT_EQ(0, file->GetSize(&result_size));
  EXPECT_EQ(sizeof(buf1), result_size);
  EXPECT_EQ(0, file->Read(attrs, buf1, sizeof(buf1), &result_bytes));
  EXPECT_EQ(sizeof(buf1), result_bytes);
  EXPECT_EQ(2, mnt.num_nodes());
  EXPECT_EQ(2, file->RefCount());

  // Attempt to open the same file, create another ref to it, but does not
  // create a new file.
  EXPECT_EQ(0, mnt.Open(Path("/foo"), O_RDWR | O_CREAT, &result_node));
  EXPECT_EQ(3, file->RefCount());
  EXPECT_EQ(2, mnt.num_nodes());
  EXPECT_EQ(file.get(), result_node.get());
  EXPECT_EQ(0, file->GetSize(&result_size));
  EXPECT_EQ(sizeof(buf1), result_size);

  // Remove our references so that only the Mount holds it
  file.reset();
  result_node.reset();
  EXPECT_EQ(2, mnt.num_nodes());

  // This should have deleted the object
  EXPECT_EQ(0, mnt.Unlink(Path("/foo")));
  EXPECT_EQ(1, mnt.num_nodes());

  // We should fail to find it
  EXPECT_EQ(ENOENT, mnt.Unlink(Path("/foo")));
  EXPECT_EQ(1, mnt.num_nodes());

  // Recreate foo as a directory
  EXPECT_EQ(0, mnt.Mkdir(Path("/foo"), O_RDWR));
  EXPECT_EQ(2, mnt.num_nodes());

  // Create a file (exclusively)
  EXPECT_EQ(0, mnt.Open(Path("/foo/bar"), O_RDWR | O_CREAT | O_EXCL, &file));
  ASSERT_NE(NULL_NODE, file.get());
  EXPECT_EQ(2, file->RefCount());
  EXPECT_EQ(3, mnt.num_nodes());

  // Attempt to delete the directory and fail
  EXPECT_EQ(ENOTEMPTY, mnt.Rmdir(Path("/foo")));
  EXPECT_EQ(2, root->RefCount());
  EXPECT_EQ(2, file->RefCount());
  EXPECT_EQ(3, mnt.num_nodes());

  // Unlink the file, we should have the only file ref at this point.
  EXPECT_EQ(0, mnt.Unlink(Path("/foo/bar")));
  EXPECT_EQ(2, root->RefCount());
  EXPECT_EQ(1, file->RefCount());
  EXPECT_EQ(3, mnt.num_nodes());

  // Deref the file, to make it go away
  file.reset();
  EXPECT_EQ(2, mnt.num_nodes());

  // Deref the directory
  EXPECT_EQ(0, mnt.Rmdir(Path("/foo")));
  EXPECT_EQ(1, mnt.num_nodes());

  // Verify the directory is gone
  EXPECT_EQ(ENOENT, mnt.Access(Path("/foo"), F_OK));
  EXPECT_EQ(ENOENT, mnt.Open(Path("/foo"), O_RDWR, &file));
  EXPECT_EQ(NULL_NODE, file.get());
}

TEST(MountTest, OpenMode_TRUNC) {
  MountMemMock mnt;
  ScopedMountNode file;
  ScopedMountNode root;
  ScopedMountNode result_node;
  HandleAttr attrs;
  int result_bytes;

  // Open a file and write something to it.
  const char* buf = "hello";
  ASSERT_EQ(0, mnt.Open(Path("/foo"), O_RDWR|O_CREAT, &file));
  ASSERT_EQ(0, file->Write(attrs, buf, strlen(buf), &result_bytes));
  ASSERT_EQ(strlen(buf), result_bytes);

  // Open it again with TRUNC and make sure it is empty
  char read_buf[10];
  ASSERT_EQ(0, mnt.Open(Path("/foo"), O_RDWR|O_TRUNC, &file));
  ASSERT_EQ(0, file->Read(attrs, read_buf, sizeof(read_buf), &result_bytes));
  ASSERT_EQ(0, result_bytes);
}

TEST(MountTest, MemMountRemove) {
  MountMemMock mnt;
  ScopedMountNode file;
  ScopedMountNode result_node;

  ASSERT_EQ(0, mnt.Mkdir(Path("/dir"), O_RDWR));
  ASSERT_EQ(0, mnt.Open(Path("/file"), O_RDWR | O_CREAT | O_EXCL, &file));
  EXPECT_NE(NULL_NODE, file.get());
  EXPECT_EQ(3, mnt.num_nodes());
  file.reset();

  EXPECT_EQ(0, mnt.Remove(Path("/dir")));
  EXPECT_EQ(2, mnt.num_nodes());
  EXPECT_EQ(0, mnt.Remove(Path("/file")));
  EXPECT_EQ(1, mnt.num_nodes());

  ASSERT_EQ(ENOENT,
            mnt.Open(Path("/dir/foo"), O_CREAT | O_RDWR, &result_node));
  ASSERT_EQ(NULL_NODE, result_node.get());
  ASSERT_EQ(ENOENT, mnt.Open(Path("/file"), O_RDONLY, &result_node));
  ASSERT_EQ(NULL_NODE, result_node.get());
}

TEST(MountTest, MemMountRename) {
  MountMemMock mnt;
  ASSERT_EQ(0, mnt.Mkdir(Path("/dir1"), O_RDWR));
  ASSERT_EQ(0, mnt.Mkdir(Path("/dir2"), O_RDWR));
  ASSERT_EQ(3, mnt.num_nodes());

  ScopedMountNode file;
  ASSERT_EQ(0, mnt.Open(Path("/dir1/file"), O_RDWR | O_CREAT | O_EXCL, &file));
  ASSERT_EQ(0, mnt.Access(Path("/dir1/file"), R_OK));
  ASSERT_EQ(4, mnt.num_nodes());

  // Move from one directory to another should ok
  ASSERT_EQ(0, mnt.Rename(Path("/dir1/file"), Path("/dir2/new_file")));
  ASSERT_NE(0, mnt.Access(Path("/dir1/file"), R_OK));
  ASSERT_EQ(0, mnt.Access(Path("/dir2/new_file"), R_OK));
  ASSERT_EQ(4, mnt.num_nodes());

  // Move within the same directory
  ASSERT_EQ(0, mnt.Rename(Path("/dir2/new_file"), Path("/dir2/new_file2")));
  ASSERT_NE(0, mnt.Access(Path("/dir2/new_file"), R_OK));
  ASSERT_EQ(0, mnt.Access(Path("/dir2/new_file2"), R_OK));
  ASSERT_EQ(4, mnt.num_nodes());

  // Move to another directory but without a filename
  ASSERT_EQ(0, mnt.Rename(Path("/dir2/new_file2"), Path("/dir1")));
  ASSERT_NE(0, mnt.Access(Path("/dir2/new_file2"), R_OK));
  ASSERT_EQ(0, mnt.Access(Path("/dir1/new_file2"), R_OK));
  ASSERT_EQ(4, mnt.num_nodes());
}

TEST(MountTest, MemMountRenameDir) {
  MountMemMock mnt;

  ASSERT_EQ(0, mnt.Mkdir(Path("/dir1"), O_RDWR));
  ASSERT_EQ(0, mnt.Mkdir(Path("/dir2"), O_RDWR));
  EXPECT_EQ(3, mnt.num_nodes());

  // Renaming one directory to another should work
  ASSERT_EQ(0, mnt.Rename(Path("/dir1"), Path("/dir2")));
  ASSERT_NE(0, mnt.Access(Path("/dir1"), R_OK));
  ASSERT_EQ(0, mnt.Access(Path("/dir2"), R_OK));
  EXPECT_EQ(2, mnt.num_nodes());

  // Reset to initial state
  ASSERT_EQ(0, mnt.Mkdir(Path("/dir1"), O_RDWR));
  EXPECT_EQ(3, mnt.num_nodes());

  // Renaming a directory to a new name within another
  ASSERT_EQ(0, mnt.Rename(Path("/dir1"), Path("/dir2/foo")));
  ASSERT_EQ(0, mnt.Access(Path("/dir2"), R_OK));
  ASSERT_EQ(0, mnt.Access(Path("/dir2/foo"), R_OK));
  EXPECT_EQ(3, mnt.num_nodes());

  // Reset to initial state
  ASSERT_EQ(0, mnt.Rmdir(Path("/dir2/foo")));
  ASSERT_EQ(0, mnt.Mkdir(Path("/dir1"), O_RDWR));
  EXPECT_EQ(3, mnt.num_nodes());

  // Renaming one directory to another should fail if the target is non-empty
  ASSERT_EQ(0, mnt.Mkdir(Path("/dir2/dir3"), O_RDWR));
  ASSERT_EQ(ENOTEMPTY, mnt.Rename(Path("/dir1"), Path("/dir2")));
}

TEST(MountTest, DevAccess) {
  // Should not be able to open non-existent file.
  MountDevMock mnt;
  ASSERT_EQ(ENOENT, mnt.Access(Path("/foo"), F_OK));
}

TEST(MountTest, DevNull) {
  MountDevMock mnt;
  ScopedMountNode dev_null;
  int result_bytes = 0;

  ASSERT_EQ(0, mnt.Access(Path("/null"), R_OK | W_OK));
  ASSERT_EQ(EACCES, mnt.Access(Path("/null"), X_OK));
  ASSERT_EQ(0, mnt.Open(Path("/null"), O_RDWR, &dev_null));
  ASSERT_NE(NULL_NODE, dev_null.get());

  // Writing to /dev/null should write everything.
  const char msg[] = "Dummy test message.";
  HandleAttr attrs;
  EXPECT_EQ(0, dev_null->Write(attrs, &msg[0], strlen(msg), &result_bytes));
  EXPECT_EQ(strlen(msg), result_bytes);

  // Reading from /dev/null should read nothing.
  const int kBufferLength = 100;
  char buffer[kBufferLength];
  EXPECT_EQ(0, dev_null->Read(attrs, &buffer[0], kBufferLength, &result_bytes));
  EXPECT_EQ(0, result_bytes);
}

TEST(MountTest, DevZero) {
  MountDevMock mnt;
  ScopedMountNode dev_zero;
  int result_bytes = 0;

  ASSERT_EQ(0, mnt.Access(Path("/zero"), R_OK | W_OK));
  ASSERT_EQ(EACCES, mnt.Access(Path("/zero"), X_OK));
  ASSERT_EQ(0, mnt.Open(Path("/zero"), O_RDWR, &dev_zero));
  ASSERT_NE(NULL_NODE, dev_zero.get());

  // Writing to /dev/zero should write everything.
  HandleAttr attrs;
  const char msg[] = "Dummy test message.";
  EXPECT_EQ(0, dev_zero->Write(attrs, &msg[0], strlen(msg), &result_bytes));
  EXPECT_EQ(strlen(msg), result_bytes);

  // Reading from /dev/zero should read all zeroes.
  const int kBufferLength = 100;
  char buffer[kBufferLength];
  // First fill with all 1s.
  memset(&buffer[0], 0x1, kBufferLength);
  EXPECT_EQ(0, dev_zero->Read(attrs, &buffer[0], kBufferLength, &result_bytes));
  EXPECT_EQ(kBufferLength, result_bytes);

  char zero_buffer[kBufferLength];
  memset(&zero_buffer[0], 0, kBufferLength);
  EXPECT_EQ(0, memcmp(&buffer[0], &zero_buffer[0], kBufferLength));
}

// Disabled due to intermittent failures on linux: http://crbug.com/257257
TEST(MountTest, DISABLED_DevUrandom) {
  MountDevMock mnt;
  ScopedMountNode dev_urandom;
  int result_bytes = 0;

  ASSERT_EQ(0, mnt.Access(Path("/urandom"), R_OK | W_OK));
  ASSERT_EQ(EACCES, mnt.Access(Path("/urandom"), X_OK));
  ASSERT_EQ(0, mnt.Open(Path("/urandom"), O_RDWR, &dev_urandom));
  ASSERT_NE(NULL_NODE, dev_urandom.get());

  // Writing to /dev/urandom should write everything.
  const char msg[] = "Dummy test message.";
  HandleAttr attrs;
  EXPECT_EQ(0, dev_urandom->Write(attrs, &msg[0], strlen(msg), &result_bytes));
  EXPECT_EQ(strlen(msg), result_bytes);

  // Reading from /dev/urandom should read random bytes.
  const int kSampleBatches = 1000;
  const int kSampleBatchSize = 1000;
  const int kTotalSamples = kSampleBatches * kSampleBatchSize;

  int byte_count[256] = {0};

  unsigned char buffer[kSampleBatchSize];
  for (int batch = 0; batch < kSampleBatches; ++batch) {
    int bytes_read = 0;
    EXPECT_EQ(0, dev_urandom->Read(attrs, &buffer[0], kSampleBatchSize,
                                   &bytes_read));
    EXPECT_EQ(kSampleBatchSize, bytes_read);

    for (int i = 0; i < bytes_read; ++i) {
      byte_count[buffer[i]]++;
    }
  }

  double expected_count = kTotalSamples / 256.;
  double chi_squared = 0;
  for (int i = 0; i < 256; ++i) {
    double difference = byte_count[i] - expected_count;
    chi_squared += difference * difference / expected_count;
  }

  // Approximate chi-squared value for p-value 0.05, 255 degrees-of-freedom.
  EXPECT_LE(chi_squared, 293.24);
}
