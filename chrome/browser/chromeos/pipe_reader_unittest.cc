// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/pipe_reader.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/file_path.h"
#include "base/safe_strerror_posix.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

typedef testing::Test PipeReaderTest;

TEST_F(PipeReaderTest, SuccessfulReadTest) {
  FilePath pipe_name("/tmp/MYFIFO");
  /* Create the FIFO if it does not exist */
  umask(0);
  mknod(pipe_name.value().c_str(), S_IFIFO|0666, 0);
  const char line[] = "foo";

  pid_t pID = fork();
  if (pID == 0) {
    int pipe = open(pipe_name.value().c_str(), O_WRONLY);
    EXPECT_NE(pipe, -1) << safe_strerror(errno);
    int num_bytes = write(pipe, line, strlen(line));
    EXPECT_NE(num_bytes, -1) << safe_strerror(errno);
    close(pipe);
    exit(1);
  } else {
    PipeReader reader(pipe_name);
    // asking for more should still just return the amount that was written.
    EXPECT_EQ(line, reader.Read(5 * strlen(line)));
  }
}

TEST_F(PipeReaderTest, SuccessfulMultiLineReadTest) {
  FilePath pipe_name("/tmp/TESTFIFO");
  /* Create the FIFO if it does not exist */
  umask(0);
  mknod(pipe_name.value().c_str(), S_IFIFO|0666, 0);
  const char foo[] = "foo";
  const char boo[] = "boo";
  std::string line(foo);
  line.append("\n");
  line.append(boo);
  line.append("\n");

  pid_t pID = fork();
  if (pID == 0) {
    int pipe = open(pipe_name.value().c_str(), O_WRONLY);
    EXPECT_NE(pipe, -1) << safe_strerror(errno);
    int num_bytes = write(pipe, line.c_str(), line.length());
    EXPECT_NE(num_bytes, -1) << safe_strerror(errno);
    close(pipe);
    exit(1);
  } else {
    PipeReader reader(pipe_name);
    // asking for more should still just return the amount that was written.
    std::string my_foo = reader.Read(5 * line.length());
    ASSERT_GT(my_foo.length(), 0U);
    EXPECT_EQ(my_foo[my_foo.length() - 1], '\n');
    my_foo.resize(my_foo.length() - 1);
    EXPECT_EQ(my_foo, foo);

    std::string my_boo = reader.Read(5 * line.length());
    ASSERT_GT(my_boo.length(), 0U);
    EXPECT_EQ(my_boo[my_boo.length() - 1], '\n');
    my_boo.resize(my_boo.length() - 1);
    EXPECT_EQ(my_boo, boo);
  }
}

TEST_F(PipeReaderTest, SuccessfulMultiLineReadNoEndingNewlineTest) {
  FilePath pipe_name("/tmp/TESTFIFO");
  /* Create the FIFO if it does not exist */
  umask(0);
  mknod(pipe_name.value().c_str(), S_IFIFO|0666, 0);
  const char foo[] = "foo";
  const char boo[] = "boo";
  std::string line(foo);
  line.append("\n");
  line.append(boo);

  pid_t pID = fork();
  if (pID == 0) {
    int pipe = open(pipe_name.value().c_str(), O_WRONLY);
    EXPECT_NE(pipe, -1) << safe_strerror(errno);
    int num_bytes = write(pipe, line.c_str(), line.length());
    EXPECT_NE(num_bytes, -1) << safe_strerror(errno);
    close(pipe);
    exit(1);
  } else {
    PipeReader reader(pipe_name);
    // asking for more should still just return the amount that was written.
    std::string my_foo = reader.Read(5 * line.length());
    ASSERT_GT(my_foo.length(), 0U);
    EXPECT_EQ(my_foo[my_foo.length() - 1], '\n');
    my_foo.resize(my_foo.length() - 1);
    EXPECT_EQ(my_foo, foo);

    std::string my_boo = reader.Read(5 * line.length());
    EXPECT_EQ(my_boo, boo);
  }
}

}  // namespace chromeos
