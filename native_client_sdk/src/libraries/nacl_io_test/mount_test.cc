// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <string>

#include "gtest/gtest.h"
#include "nacl_io/ioctl.h"
#include "nacl_io/mount.h"
#include "nacl_io/mount_dev.h"
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

  ScopedMountNode file;
  ScopedMountNode root;
  ScopedMountNode result_node;

  size_t result_size = 0;
  int result_bytes = 0;
  char buf1[1024];

  // A memory mount starts with one directory node: the root.
  EXPECT_EQ(1, mnt->num_nodes());

  // Fail to open non existent file
  EXPECT_EQ(ENOENT, mnt->Access(Path("/foo"), R_OK | W_OK));
  EXPECT_EQ(ENOENT, mnt->Open(Path("/foo"), O_RDWR, &result_node));
  EXPECT_EQ(NULL, result_node.get());
  EXPECT_EQ(1, mnt->num_nodes());

  // Create a file
  EXPECT_EQ(0, mnt->Open(Path("/foo"), O_RDWR | O_CREAT, &file));
  EXPECT_NE(NULL_NODE, file.get());
  if (file == NULL)
    return;

  // We now have a directory and a file.  The file has a two references
  // one returned to the test, one for the name->inode map.
  EXPECT_EQ(2, mnt->num_nodes());
  EXPECT_EQ(2, file->RefCount());
  EXPECT_EQ(0, mnt->Access(Path("/foo"), R_OK | W_OK));
  EXPECT_EQ(EACCES, mnt->Access(Path("/foo"), X_OK));

  // Write access should be allowed on the root directory.
  EXPECT_EQ(0, mnt->Access(Path("/"), R_OK | W_OK));
  EXPECT_EQ(EACCES, mnt->Access(Path("/"), X_OK));
  // Open the root directory for write should fail.
  EXPECT_EQ(EISDIR, mnt->Open(Path("/"), O_RDWR, &root));
  EXPECT_EQ(2, mnt->num_nodes());

  // Open the root directory, should not create a new file
  EXPECT_EQ(0, mnt->Open(Path("/"), O_RDONLY, &root));
  EXPECT_EQ(2, mnt->num_nodes());
  EXPECT_NE(NULL_NODE, root.get());
  if (NULL != root) {
    struct dirent dirs[2];
    int len;
    EXPECT_EQ(0, root->GetDents(0, dirs, sizeof(dirs), &len));
    EXPECT_EQ(sizeof(struct dirent), len);
  }

  // Fail to re-create the same file
  EXPECT_EQ(EEXIST,
            mnt->Open(Path("/foo"), O_RDWR | O_CREAT | O_EXCL, &result_node));
  EXPECT_EQ(NULL_NODE, result_node.get());
  EXPECT_EQ(2, mnt->num_nodes());

  // Fail to create a directory with the same name
  EXPECT_EQ(EEXIST, mnt->Mkdir(Path("/foo"), O_RDWR));
  EXPECT_EQ(2, mnt->num_nodes());

  // Attempt to READ/WRITE
  EXPECT_EQ(0, file->GetSize(&result_size));
  EXPECT_EQ(0, result_size);
  EXPECT_EQ(0, file->Write(0, buf1, sizeof(buf1), &result_bytes));
  EXPECT_EQ(sizeof(buf1), result_bytes);
  EXPECT_EQ(0, file->GetSize(&result_size));
  EXPECT_EQ(sizeof(buf1), result_size);
  EXPECT_EQ(0, file->Read(0, buf1, sizeof(buf1), &result_bytes));
  EXPECT_EQ(sizeof(buf1), result_bytes);
  EXPECT_EQ(2, mnt->num_nodes());
  EXPECT_EQ(2, file->RefCount());

  // Attempt to open the same file, create another ref to it, but does not
  // create a new file.
  EXPECT_EQ(0, mnt->Open(Path("/foo"), O_RDWR | O_CREAT, &result_node));
  EXPECT_EQ(3, file->RefCount());
  EXPECT_EQ(2, mnt->num_nodes());
  EXPECT_EQ(file.get(), result_node.get());
  EXPECT_EQ(0, file->GetSize(&result_size));
  EXPECT_EQ(sizeof(buf1), result_size);

  // Remove our references so that only the Mount holds it
  file.reset();
  result_node.reset();
  EXPECT_EQ(2, mnt->num_nodes());

  // This should have deleted the object
  EXPECT_EQ(0, mnt->Unlink(Path("/foo")));
  EXPECT_EQ(1, mnt->num_nodes());

  // We should fail to find it
  EXPECT_EQ(ENOENT, mnt->Unlink(Path("/foo")));
  EXPECT_EQ(1, mnt->num_nodes());

  // Recreate foo as a directory
  EXPECT_EQ(0, mnt->Mkdir(Path("/foo"), O_RDWR));
  EXPECT_EQ(2, mnt->num_nodes());

  // Create a file (exclusively)
  EXPECT_EQ(0, mnt->Open(Path("/foo/bar"), O_RDWR | O_CREAT | O_EXCL, &file));
  EXPECT_NE(NULL_NODE, file.get());
  if (NULL == file)
    return;
  EXPECT_EQ(2, file->RefCount());
  EXPECT_EQ(3, mnt->num_nodes());

  // Attempt to delete the directory and fail
  EXPECT_EQ(ENOTEMPTY, mnt->Rmdir(Path("/foo")));
  EXPECT_EQ(2, root->RefCount());
  EXPECT_EQ(2, file->RefCount());
  EXPECT_EQ(3, mnt->num_nodes());

  // Unlink the file, we should have the only file ref at this point.
  EXPECT_EQ(0, mnt->Unlink(Path("/foo/bar")));
  EXPECT_EQ(2, root->RefCount());
  EXPECT_EQ(1, file->RefCount());
  EXPECT_EQ(3, mnt->num_nodes());


  // Deref the file, to make it go away
  file.reset();
  EXPECT_EQ(2, mnt->num_nodes());

  // Deref the directory
  EXPECT_EQ(0, mnt->Rmdir(Path("/foo")));
  EXPECT_EQ(1, mnt->num_nodes());

  // Verify the directory is gone
  EXPECT_EQ(ENOENT, mnt->Access(Path("/foo"), F_OK));
  EXPECT_EQ(ENOENT, mnt->Open(Path("/foo"), O_RDWR, &file));
  EXPECT_EQ(NULL_NODE, file.get());
}

TEST(MountTest, MemMountRemove) {
  MountMemMock* mnt = new MountMemMock();
  ScopedMountNode file;
  ScopedMountNode result_node;

  EXPECT_EQ(0, mnt->Mkdir(Path("/dir"), O_RDWR));
  EXPECT_EQ(0, mnt->Open(Path("/file"), O_RDWR | O_CREAT | O_EXCL, &file));
  EXPECT_NE(NULL_NODE, file.get());
  EXPECT_EQ(3, mnt->num_nodes());
  file.reset();

  EXPECT_EQ(0, mnt->Remove(Path("/dir")));
  EXPECT_EQ(2, mnt->num_nodes());
  EXPECT_EQ(0, mnt->Remove(Path("/file")));
  EXPECT_EQ(1, mnt->num_nodes());

  EXPECT_EQ(ENOENT,
            mnt->Open(Path("/dir/foo"), O_CREAT | O_RDWR, &result_node));
  EXPECT_EQ(NULL_NODE, result_node.get());
  EXPECT_EQ(ENOENT, mnt->Open(Path("/file"), O_RDONLY, &result_node));
  EXPECT_EQ(NULL_NODE, result_node.get());
}

TEST(MountTest, DevAccess) {
  // Should not be able to open non-existent file.
  MountDevMock* mnt = new MountDevMock();
  ASSERT_EQ(ENOENT, mnt->Access(Path("/foo"), F_OK));
}

TEST(MountTest, DevNull) {
  MountDevMock* mnt = new MountDevMock();
  ScopedMountNode dev_null;
  int result_bytes = 0;

  ASSERT_EQ(0, mnt->Access(Path("/null"), R_OK | W_OK));
  ASSERT_EQ(EACCES, mnt->Access(Path("/null"), X_OK));
  ASSERT_EQ(0, mnt->Open(Path("/null"), O_RDWR, &dev_null));
  ASSERT_NE(NULL_NODE, dev_null.get());

  // Writing to /dev/null should write everything.
  const char msg[] = "Dummy test message.";
  EXPECT_EQ(0, dev_null->Write(0, &msg[0], strlen(msg), &result_bytes));
  EXPECT_EQ(strlen(msg), result_bytes);

  // Reading from /dev/null should read nothing.
  const int kBufferLength = 100;
  char buffer[kBufferLength];
  EXPECT_EQ(0, dev_null->Read(0, &buffer[0], kBufferLength, &result_bytes));
  EXPECT_EQ(0, result_bytes);
}

TEST(MountTest, DevZero) {
  MountDevMock* mnt = new MountDevMock();
  ScopedMountNode dev_zero;
  int result_bytes = 0;

  ASSERT_EQ(0, mnt->Access(Path("/zero"), R_OK | W_OK));
  ASSERT_EQ(EACCES, mnt->Access(Path("/zero"), X_OK));
  ASSERT_EQ(0, mnt->Open(Path("/zero"), O_RDWR, &dev_zero));
  ASSERT_NE(NULL_NODE, dev_zero.get());

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
}

// Disabled due to intermittent failures on linux: http://crbug.com/257257
TEST(MountTest, DISABLED_DevUrandom) {
  MountDevMock* mnt = new MountDevMock();
  ScopedMountNode dev_urandom;
  int result_bytes = 0;

  ASSERT_EQ(0, mnt->Access(Path("/urandom"), R_OK | W_OK));
  ASSERT_EQ(EACCES, mnt->Access(Path("/urandom"), X_OK));
  ASSERT_EQ(0, mnt->Open(Path("/urandom"), O_RDWR, &dev_urandom));
  ASSERT_NE(NULL_NODE, dev_urandom.get());

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


TEST(MountTest, DevTty) {
  MountDevMock* mnt = new MountDevMock();
  ScopedMountNode dev_tty;

  ASSERT_EQ(0, mnt->Access(Path("/tty"), R_OK | W_OK));
  ASSERT_EQ(EACCES, mnt->Access(Path("/tty"), X_OK));
  ASSERT_EQ(0, mnt->Open(Path("/tty"), O_RDWR, &dev_tty));
  ASSERT_NE(NULL_NODE, dev_tty.get());

  // 123 is not a valid ioctl request.
  EXPECT_EQ(EINVAL, dev_tty->Ioctl(123, NULL));

  // TIOCNACLPREFIX is, it should set the prefix.
  std::string prefix("__my_awesome_prefix__");
  EXPECT_EQ(0, dev_tty->Ioctl(TIOCNACLPREFIX,
                              const_cast<char*>(prefix.c_str())));

  // Now let's try sending some data over.
  // First we create the message.
  std::string content("hello, how are you?\n");
  std::string message = prefix.append(content);
  struct tioc_nacl_input_string packaged_message;
  packaged_message.length = message.size();
  packaged_message.buffer = message.data();

  // Now we make buffer we'll read into.
  // We fill the buffer and a backup buffer with arbitrary data
  // and compare them after reading to make sure read doesn't
  // clobber parts of the buffer it shouldn't.
  int bytes_read;
  char buffer[100];
  char backup_buffer[100];
  memset(buffer, 'a', 100);
  memset(backup_buffer, 'a', 100);

  // Now we actually send the data
  EXPECT_EQ(0, dev_tty->Ioctl(TIOCNACLINPUT,
                              reinterpret_cast<char*>(&packaged_message)));

  // We read a small chunk first to ensure it doesn't give us
  // more than we ask for.
  EXPECT_EQ(0, dev_tty->Read(0, buffer, 5, &bytes_read));
  EXPECT_EQ(bytes_read, 5);
  EXPECT_EQ(0, memcmp(content.data(), buffer, 5));
  EXPECT_EQ(0, memcmp(buffer + 5, backup_buffer + 5, 95));

  // Now we ask for more data than is left in the tty, to ensure
  // it doesn't give us more than is there.
  EXPECT_EQ(0, dev_tty->Read(0, buffer + 5, 95, &bytes_read));
  EXPECT_EQ(bytes_read, content.size() - 5);
  EXPECT_EQ(0, memcmp(content.data(), buffer, content.size()));
  EXPECT_EQ(0, memcmp(buffer + content.size(),
                      backup_buffer + content.size(),
                       100 - content.size()));

  // Now we try to send something with an invalid prefix
  std::string bogus_message("Woah there, this message has no valid prefix");
  struct tioc_nacl_input_string bogus_pack;
  bogus_pack.length = bogus_message.size();
  bogus_pack.buffer = bogus_message.data();
  EXPECT_EQ(ENOTTY, dev_tty->Ioctl(TIOCNACLINPUT,
                                   reinterpret_cast<char*>(&bogus_pack)));
}
