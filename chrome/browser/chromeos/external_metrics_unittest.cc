// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <sys/file.h>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chromeos/external_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {  // Need this because of the FRIEND_TEST

class ExternalMetricsTest : public testing::Test {
};

// Because the metrics service is not essential, errors will not cause the
// program to terminate.  However, the errors produce logs.

#define MAXLENGTH ExternalMetrics::kMetricsMessageMaxLength

static void SendMessage(const char* path, const char* name, const char* value) {
  int fd = open(path, O_CREAT | O_APPEND | O_WRONLY, 0666);
  int32 l = strlen(name) + strlen(value) + 2 + sizeof(l);
  int num_bytes;
  num_bytes = write(fd, &l, sizeof(l));
  num_bytes = write(fd, name, strlen(name) + 1);
  num_bytes = write(fd, value, strlen(value) + 1);
  close(fd);
}

static scoped_ptr<std::string> received_name;
static scoped_ptr<std::string> received_value;
int received_count = 0;

static void ReceiveMessage(const char* name, const char* value) {
  received_name.reset(new std::string(name));
  received_value.reset(new std::string(value));
  received_count++;
}

static void CheckMessage(const char* name, const char* value, int count) {
  EXPECT_EQ(*received_name.get(), name);
  EXPECT_EQ(*received_value.get(), value);
  EXPECT_EQ(received_count, count);
}

TEST(ExternalMetricsTest, ParseExternalMetricsFile) {
  const char *histogram_data[] = {
    "BootTime 9500 0 20000 50",
    "BootTime 10000 0 20000 50",
    "BootTime 9200 0 20000 50",
    "ConnmanIdle 1000 0 2000 20",
    "ConnmanIdle 1200 0 2000 20",
    "ConnmanDisconnect 1000 0 2000 20",
    "ConnmanFailure 1000 0 2000 20",
    "ConnmanFailure 13000 2000 20",
    "ConnmanAssociation 1000 0 2000 20",
    "ConnmanConfiguration 1000 0 2000 20",
    "ConnmanOffline 1000 0 2000 20",
    "ConnmanOnline 1000 0 2000 20",
    "ConnmanOffline 2000 0 2000 20",
    "ConnmanReady 33000 0 100000 50",
    "ConnmanReady 44000 0 100000 50",
    "ConnmanReady 22000 0 100000 50",
  };
  int nhist = ARRAYSIZE_UNSAFE(histogram_data);
  int32 i;
  const char* path = "/tmp/.chromeos-metrics";
  scoped_refptr<chromeos::ExternalMetrics>
      external_metrics(new chromeos::ExternalMetrics());
  external_metrics->test_recorder_ = &ReceiveMessage;
  external_metrics->test_path_ = FilePath(path);
  EXPECT_TRUE(unlink(path) == 0 || errno == ENOENT);

  // Sends a few valid messages.  Once in a while, collects them and checks the
  // last message.  We don't want to check every single message because we also
  // want to test the ability to deal with a file containing more than one
  // message.
  for (i = 0; i < nhist; i++) {
    SendMessage(path, "histogram", histogram_data[i]);
    if (i % 3 == 2) {
      external_metrics->CollectEvents();
      CheckMessage("histogram", histogram_data[i], i + 1);
    }
  }

  // Sends a crash message.
  int expect_count = nhist;
  SendMessage(path, "crash", "user");
  external_metrics->CollectEvents();
  CheckMessage("crash", "user", ++expect_count);

  // Sends a message that's too large.
  char b[MAXLENGTH + 100];
  for (i = 0; i < MAXLENGTH + 99; i++) {
    b[i] = 'x';
  }
  b[i] = '\0';
  SendMessage(path, b, "yyy");
  // Expect logged errors about bad message size.
  external_metrics->CollectEvents();
  EXPECT_EQ(expect_count, received_count);

  // Sends a malformed message (first string is not null-terminated).
  i = 100 + sizeof(i);
  int fd = open(path, O_CREAT | O_WRONLY, 0666);
  EXPECT_GT(fd, 0);
  EXPECT_EQ(static_cast<int>(sizeof(i)), write(fd, &i, sizeof(i)));
  EXPECT_EQ(i, write(fd, b, i));
  EXPECT_EQ(0, close(fd));

  external_metrics->CollectEvents();
  EXPECT_EQ(expect_count, received_count);

  // Sends a malformed message (second string is not null-terminated).
  b[50] = '\0';
  fd = open(path, O_CREAT | O_WRONLY, 0666);
  EXPECT_GT(fd, 0);
  EXPECT_EQ(static_cast<int>(sizeof(i)), write(fd, &i, sizeof(i)));
  EXPECT_EQ(i, write(fd, b, i));
  EXPECT_EQ(0, close(fd));

  external_metrics->CollectEvents();
  EXPECT_EQ(expect_count, received_count);

  // Checks that we survive when file doesn't exist.
  EXPECT_EQ(0, unlink(path));
  external_metrics->CollectEvents();
  EXPECT_EQ(expect_count, received_count);
}

}  // namespace chromeos
