// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <sys/file.h>

#include "base/basictypes.h"
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
  write(fd, &l, sizeof(l));
  write(fd, name, strlen(name) + 1);
  write(fd, value, strlen(value) + 1);
  close(fd);
}

static scoped_ptr<std::string> received_name;
static scoped_ptr<std::string> received_value;
static int received_count = 0;

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
  struct {
    const char* name;
    const char* value;
  } pairs[] = {
    {"BootTime", "9500"},
    {"BootTime", "10000"},
    {"BootTime", "9200"},
  };
  int npairs = ARRAYSIZE_UNSAFE(pairs);
  int32 i;
  const char* path = "/tmp/.chromeos-metrics";

  scoped_ptr<chromeos::ExternalMetrics>
      external_metrics(new chromeos::ExternalMetrics());
  external_metrics->SetRecorder(&ReceiveMessage);

  EXPECT_TRUE(unlink(path) == 0 || errno == ENOENT);

  // Send a few valid messages.
  for (i = 0; i < npairs; i++) {
    SendMessage(path, pairs[i].name, pairs[i].value);
  }

  external_metrics->CollectEvents();
  CheckMessage(pairs[npairs-1].name, pairs[npairs-1].value, npairs);

  // Send a message that's too large.
  char b[MAXLENGTH + 100];
  for (i = 0; i < MAXLENGTH + 99; i++) {
    b[i] = 'x';
  }
  b[i] = '\0';
  SendMessage(path, b, "yyy");
  external_metrics->CollectEvents();
  EXPECT_EQ(received_count, npairs);

  // Send a malformed message (first string is not null-terminated).
  i = 100 + sizeof(i);
  int fd = open(path, O_CREAT | O_WRONLY);
  EXPECT_GT(fd, 0);
  EXPECT_EQ(write(fd, &i, sizeof(i)), static_cast<int>(sizeof(i)));
  EXPECT_EQ(write(fd, b, i), i);
  EXPECT_EQ(close(fd), 0);

  external_metrics->CollectEvents();
  EXPECT_EQ(received_count, npairs);

  // Send a malformed message (second string is not null-terminated).
  b[50] = '\0';
  fd = open(path, O_CREAT | O_WRONLY);
  EXPECT_GT(fd, 0);
  EXPECT_EQ(write(fd, &i, sizeof(i)), static_cast<int>(sizeof(i)));
  EXPECT_EQ(write(fd, b, i), i);
  EXPECT_EQ(close(fd), 0);

  external_metrics->CollectEvents();
  EXPECT_EQ(received_count, npairs);

  // Check that we survive when file doesn't exist.
  EXPECT_EQ(unlink(path), 0);
  external_metrics->CollectEvents();
  EXPECT_EQ(received_count, npairs);
}

}  // namespace chromeos
