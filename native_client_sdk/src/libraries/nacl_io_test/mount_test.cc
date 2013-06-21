/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <string>
#include <sys/stat.h>

#include "nacl_io/mount.h"
#include "nacl_io/mount_dev.h"
#include "nacl_io/mount_mem.h"
#include "nacl_io/osdirent.h"

#include "gtest/gtest.h"

namespace {

class MountMemMock : public MountMem {
 public:
  MountMemMock() {
    StringMap_t map;
    EXPECT_EQ(0, Init(1, map, NULL));
  }

  int num_nodes() { return (int) inode_pool_.size(); }
};

class MountDevMock : public MountDev {
 public:
  MountDevMock() {
    StringMap_t map;
    Init(1, map, NULL);
  }
  int num_nodes() { return (int) inode_pool_.size(); }
};

}  // namespace

#define NULL_NODE ((MountNode*) NULL)

TEST(MountTest, Sanity) {
  MountMemMock* mnt = new MountMemMock();
  MountNode* file;
  MountNode* root;
  MountNode* result_node;
  size_t result_size = 0;
  int result_bytes = 0;

  char buf1[1024];

  // A memory mount starts with one directory node: the root.
  EXPECT_EQ(1, mnt->num_nodes());

  // Fail to open non existent file
  EXPECT_EQ(ENOENT, mnt->Access(Path("/foo"), R_OK | W_OK));
  EXPECT_EQ(ENOENT, mnt->Open(Path("/foo"), O_RDWR, &result_node));
  EXPECT_EQ(NULL, result_node);

  // Create a file
  EXPECT_EQ(0, mnt->Open(Path("/foo"), O_RDWR | O_CREAT, &file));
  EXPECT_NE(NULL_NODE, file);
  if (file == NULL)
    return;
  EXPECT_EQ(2, file->RefCount());
  EXPECT_EQ(2, mnt->num_nodes());
  EXPECT_EQ(0, mnt->Access(Path("/foo"), R_OK | W_OK));
  EXPECT_EQ(EACCES, mnt->Access(Path("/foo"), X_OK));

  // Write access should be allowed on the root directory.
  EXPECT_EQ(0, mnt->Access(Path("/"), R_OK | W_OK));
  EXPECT_EQ(EACCES, mnt->Access(Path("/"), X_OK));
  // Open the root directory for write should fail.
  EXPECT_EQ(EISDIR, mnt->Open(Path("/"), O_RDWR, &root));

  // Open the root directory
  EXPECT_EQ(0, mnt->Open(Path("/"), O_RDONLY, &root));
  EXPECT_NE(NULL_NODE, root);
  if (NULL != root) {
    struct dirent dirs[2];
    int len;
    EXPECT_EQ(0, root->GetDents(0, dirs, sizeof(dirs), &len));
    EXPECT_EQ(sizeof(struct dirent), len);
  }

  // Fail to re-create the same file
  EXPECT_EQ(EEXIST,
            mnt->Open(Path("/foo"), O_RDWR | O_CREAT | O_EXCL, &result_node));
  EXPECT_EQ(NULL_NODE, result_node);
  EXPECT_EQ(2, mnt->num_nodes());

  // Fail to create a directory with the same name
  EXPECT_EQ(EEXIST, mnt->Mkdir(Path("/foo"), O_RDWR));

  // Attempt to READ/WRITE
  EXPECT_EQ(0, file->GetSize(&result_size));
  EXPECT_EQ(0, result_size);
  EXPECT_EQ(0, file->Write(0, buf1, sizeof(buf1), &result_bytes));
  EXPECT_EQ(sizeof(buf1), result_bytes);
  EXPECT_EQ(0, file->GetSize(&result_size));
  EXPECT_EQ(sizeof(buf1), result_size);
  EXPECT_EQ(0, file->Read(0, buf1, sizeof(buf1), &result_bytes));
  EXPECT_EQ(sizeof(buf1), result_bytes);

  // Attempt to open the same file
  EXPECT_EQ(0, mnt->Open(Path("/foo"), O_RDWR | O_CREAT, &result_node));
  EXPECT_EQ(file, result_node);
  EXPECT_EQ(0, file->GetSize(&result_size));
  EXPECT_EQ(sizeof(buf1), result_size);
  EXPECT_EQ(3, file->RefCount());
  EXPECT_EQ(2, mnt->num_nodes());

  // Attempt to close and delete the file
  mnt->ReleaseNode(file);
  EXPECT_EQ(2, mnt->num_nodes());
  EXPECT_EQ(0, mnt->Unlink(Path("/foo")));
  EXPECT_EQ(2, mnt->num_nodes());
  EXPECT_EQ(ENOENT, mnt->Unlink(Path("/foo")));
  EXPECT_EQ(2, mnt->num_nodes());
  mnt->ReleaseNode(file);
  EXPECT_EQ(1, mnt->num_nodes());

  // Recreate foo as a directory
  EXPECT_EQ(0, mnt->Mkdir(Path("/foo"), O_RDWR));

  // Create a file (exclusively)
  EXPECT_EQ(0, mnt->Open(Path("/foo/bar"), O_RDWR | O_CREAT | O_EXCL, &file));
  EXPECT_NE(NULL_NODE, file);
  if (NULL == file)
    return;

  // Attempt to delete the directory
  EXPECT_EQ(ENOTEMPTY, mnt->Rmdir(Path("/foo")));

  // Unlink the file, then delete the directory
  EXPECT_EQ(0, mnt->Unlink(Path("/foo/bar")));
  EXPECT_EQ(0, mnt->Rmdir(Path("/foo")));
  EXPECT_EQ(2, mnt->num_nodes());
  mnt->ReleaseNode(file);
  EXPECT_EQ(1, mnt->num_nodes());

  // Verify the directory is gone
  EXPECT_EQ(ENOENT, mnt->Access(Path("/foo"), F_OK));
  EXPECT_EQ(ENOENT, mnt->Open(Path("/foo"), O_RDWR, &file));
  EXPECT_EQ(NULL_NODE, file);
}

TEST(MountTest, MemMountRemove) {
  MountMemMock* mnt = new MountMemMock();
  MountNode* file;
  MountNode* result_node;

  EXPECT_EQ(0, mnt->Mkdir(Path("/dir"), O_RDWR));
  EXPECT_EQ(0, mnt->Open(Path("/file"), O_RDWR | O_CREAT | O_EXCL, &file));
  EXPECT_NE(NULL_NODE, file);
  mnt->ReleaseNode(file);

  EXPECT_EQ(0, mnt->Remove(Path("/dir")));
  EXPECT_EQ(0, mnt->Remove(Path("/file")));

  EXPECT_EQ(ENOENT,
            mnt->Open(Path("/dir/foo"), O_CREAT | O_RDWR, &result_node));
  EXPECT_EQ(NULL_NODE, result_node);
  EXPECT_EQ(ENOENT, mnt->Open(Path("/file"), O_RDONLY, &result_node));
  EXPECT_EQ(NULL_NODE, result_node);
}

TEST(MountTest, DevAccess) {
  // Should not be able to open non-existent file.
  MountDevMock* mnt = new MountDevMock();
  ASSERT_EQ(ENOENT, mnt->Access(Path("/foo"), F_OK));
}

TEST(MountTest, DevNull) {
  MountDevMock* mnt = new MountDevMock();
  MountNode* dev_null = NULL;
  int result_bytes = 0;

  ASSERT_EQ(0, mnt->Access(Path("/null"), R_OK | W_OK));
  ASSERT_EQ(EACCES, mnt->Access(Path("/null"), X_OK));
  ASSERT_EQ(0, mnt->Open(Path("/null"), O_RDWR, &dev_null));
  ASSERT_NE(NULL_NODE, dev_null);

  // Writing to /dev/null should write everything.
  const char msg[] = "Dummy test message.";
  EXPECT_EQ(0, dev_null->Write(0, &msg[0], strlen(msg), &result_bytes));
  EXPECT_EQ(strlen(msg), result_bytes);

  // Reading from /dev/null should read nothing.
  const int kBufferLength = 100;
  char buffer[kBufferLength];
  EXPECT_EQ(0, dev_null->Read(0, &buffer[0], kBufferLength, &result_bytes));
  EXPECT_EQ(0, result_bytes);
  mnt->ReleaseNode(dev_null);
}

TEST(MountTest, DevZero) {
  MountDevMock* mnt = new MountDevMock();
  MountNode* dev_zero = NULL;
  int result_bytes = 0;

  ASSERT_EQ(0, mnt->Access(Path("/zero"), R_OK | W_OK));
  ASSERT_EQ(EACCES, mnt->Access(Path("/zero"), X_OK));
  ASSERT_EQ(0, mnt->Open(Path("/zero"), O_RDWR, &dev_zero));
  ASSERT_NE(NULL_NODE, dev_zero);

  // Writing to /dev/zero should write everything.
  const char msg[] = "Dummy test message.";
  EXPECT_EQ(0, dev_zero->Write(0, &msg[0], strlen(msg), &result_bytes));
  EXPECT_EQ(strlen(msg), result_bytes);

  // Reading from /dev/zero should read all zeroes.
  const int kBufferLength = 100;
  char buffer[kBufferLength];
  // First fill with all 1s.
  memset(&buffer[0], 0x1, kBufferLength);
  EXPECT_EQ(0, dev_zero->Read(0, &buffer[0], kBufferLength, &result_bytes));
  EXPECT_EQ(kBufferLength, result_bytes);

  char zero_buffer[kBufferLength];
  memset(&zero_buffer[0], 0, kBufferLength);
  EXPECT_EQ(0, memcmp(&buffer[0], &zero_buffer[0], kBufferLength));
  mnt->ReleaseNode(dev_zero);
}

TEST(MountTest, DevUrandom) {
  MountDevMock* mnt = new MountDevMock();
  MountNode* dev_urandom = NULL;
  int result_bytes = 0;

  ASSERT_EQ(0, mnt->Access(Path("/urandom"), R_OK | W_OK));
  ASSERT_EQ(EACCES, mnt->Access(Path("/urandom"), X_OK));
  ASSERT_EQ(0, mnt->Open(Path("/urandom"), O_RDWR, &dev_urandom));
  ASSERT_NE(NULL_NODE, dev_urandom);

  // Writing to /dev/urandom should write everything.
  const char msg[] = "Dummy test message.";
  EXPECT_EQ(0, dev_urandom->Write(0, &msg[0], strlen(msg), &result_bytes));
  EXPECT_EQ(strlen(msg), result_bytes);

  // Reading from /dev/urandom should read random bytes.
  const int kSampleBatches = 1000;
  const int kSampleBatchSize = 1000;
  const int kTotalSamples = kSampleBatches * kSampleBatchSize;

  int byte_count[256] = {0};

  unsigned char buffer[kSampleBatchSize];
  for (int batch = 0; batch < kSampleBatches; ++batch) {
    int bytes_read = 0;
    EXPECT_EQ(0,
              dev_urandom->Read(0, &buffer[0], kSampleBatchSize, &bytes_read));
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
