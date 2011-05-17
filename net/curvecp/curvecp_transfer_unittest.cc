// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <poll.h>

#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "net/base/net_test_suite.h"
#include "net/base/test_completion_callback.h"
#include "net/curvecp/circular_buffer.h"
#include "net/curvecp/received_block_list.h"
#include "net/curvecp/rtt_and_send_rate_calculator.h"
#include "net/curvecp/test_client.h"
#include "net/curvecp/test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

class CurveCPTransferTest : public PlatformTest {
 public:
  CurveCPTransferTest() {}
};

void RunEchoTest(int bytes) {
  TestServer server;
  TestClient client;

  EXPECT_TRUE(server.Start(1234));
  HostPortPair server_address("localhost", 1234);
  TestCompletionCallback cb;
  EXPECT_TRUE(client.Start(server_address, bytes, &cb));

  int rv = cb.WaitForResult();
  EXPECT_EQ(0, rv);
  EXPECT_EQ(0, server.error_count());
  EXPECT_EQ(0, client.error_count());
}

/*
TEST_F(CurveCPTransferTest, Echo_50B_Of_Data) {
  RunEchoTest(50);
}

TEST_F(CurveCPTransferTest, Echo_1KB_Of_Data) {
  RunEchoTest(1024);
}

TEST_F(CurveCPTransferTest, Echo_4KB_Of_Data) {
  RunEchoTest(4096);
}
*/

// XXXMB
TEST_F(CurveCPTransferTest, Echo_1MB_Of_Data) {
  RunEchoTest(1024*1024);
}

// TODO(mbelshe): Do something meaningful with this test.
TEST_F(CurveCPTransferTest, RTTIntial) {
  RttAndSendRateCalculator rate;
  printf("initial %d (%d, %d/%d)\n", rate.send_rate(),
                                     rate.rtt_average(),
                                     rate.rtt_lo(),
                                     rate.rtt_hi());

  for (int i = 0; i < 10; ++i) {
    rate.OnMessage(100*1000);
    printf("%d: %d (%d, %d/%d)\n", i, rate.send_rate(),
                                     rate.rtt_average(),
                                     rate.rtt_lo(),
                                     rate.rtt_hi());
    usleep(200000);
  }

  rate.OnTimeout();
  rate.OnTimeout();
  rate.OnTimeout();
  rate.OnTimeout();

  // Get worse
  for (int i = 0; i < 10; ++i) {
    rate.OnMessage((500 + i)*1000);
    printf("%d: %d (%d, %d/%d)\n", i, rate.send_rate(),
                                     rate.rtt_average(),
                                     rate.rtt_lo(),
                                     rate.rtt_hi());
    usleep(200000);
  }

  // Get better
  for (int i = 0; i < 10; ++i) {
    rate.OnMessage((100 - i)*1000);
    printf("%d: %d (%d, %d/%d)\n", i, rate.send_rate(),
                                     rate.rtt_average(),
                                     rate.rtt_lo(),
                                     rate.rtt_hi());
    usleep(200000);
  }
}

TEST_F(CurveCPTransferTest, CircularBufferFillAndDrain) {
  CircularBuffer buffer(10);
  for (int i = 0; i < 30; ++i) {
    EXPECT_EQ(0, buffer.length());
    EXPECT_EQ(3, buffer.write("abc", 3));
    EXPECT_EQ(3, buffer.length());
    char read_data[3];
    EXPECT_EQ(3, buffer.read(read_data, 3));
    EXPECT_EQ(0, memcmp(read_data, "abc", 3));
  }
}

TEST_F(CurveCPTransferTest, CircularBufferTooLargeFill) {
  CircularBuffer buffer(10);
  EXPECT_EQ(10, buffer.write("abcdefghijklm", 13));
  EXPECT_EQ(0, buffer.write("a", 1));
  char read_data[10];
  EXPECT_EQ(10, buffer.read(read_data, 10));
  EXPECT_EQ(0, memcmp(read_data, "abcdefghij", 10));
  EXPECT_EQ(1, buffer.write("a", 1));
}

TEST_F(CurveCPTransferTest, CircularBufferEdgeCases) {
  CircularBuffer buffer(10);
  char read_data[10];
  EXPECT_EQ(9, buffer.write("abcdefghi", 9));
  EXPECT_EQ(9, buffer.read(read_data, 10));
  EXPECT_EQ(0, memcmp(read_data, "abcdefghi", 9));
  EXPECT_EQ(1, buffer.write("a", 1));
  EXPECT_EQ(1, buffer.write("b", 1));
  EXPECT_EQ(2, buffer.read(read_data, 10));
  EXPECT_EQ(0, memcmp(read_data, "ab", 2));
}

TEST_F(CurveCPTransferTest, CircularBufferOneWriteTwoReads) {
  CircularBuffer buffer(1000);
  char read_data[10];
  EXPECT_EQ(13, buffer.write("abcdefghijklm", 13));
  EXPECT_EQ(10, buffer.read(read_data, 10));
  EXPECT_EQ(0, memcmp(read_data, "abcdefghij", 10));
  EXPECT_EQ(3, buffer.read(read_data, 10));
  EXPECT_EQ(0, memcmp(read_data, "klm", 3));
}

TEST_F(CurveCPTransferTest, CircularBufferTwoWritesOneRead) {
  CircularBuffer buffer(1000);
  char read_data[10];
  EXPECT_EQ(6, buffer.write("abcdef", 6));
  EXPECT_EQ(4, buffer.write("ghij", 4));
  EXPECT_EQ(10, buffer.read(read_data, 10));
  EXPECT_EQ(0, memcmp(read_data, "abcdefghij", 10));
}

TEST_F(CurveCPTransferTest, CircularBufferFillOnSpill) {
  CircularBuffer buffer(10);
  char read_data[10];
  EXPECT_EQ(10, buffer.write("abcdefghij", 10));
  EXPECT_EQ(3, buffer.read(read_data, 3));
  EXPECT_EQ(0, memcmp(read_data, "abc", 3));
  // We now have a hole at the head of the circular buffer.
  EXPECT_EQ(1, buffer.write("x", 1));
  EXPECT_EQ(1, buffer.write("y", 1));
  EXPECT_EQ(1, buffer.write("z", 1));
  EXPECT_EQ(0, buffer.write("q", 1));  // Overflow
  EXPECT_EQ(10, buffer.read(read_data, 10));
  EXPECT_EQ(0, memcmp(read_data, "defghijxyz", 10));
}

TEST_F(CurveCPTransferTest, ReceivedBlockList) {
  scoped_refptr<IOBufferWithSize> buffer(new IOBufferWithSize(16));
  memcpy(buffer->data(), "abcdefghijklmnop", 16);
  scoped_refptr<IOBufferWithSize> read_buffer(new IOBufferWithSize(100));

  ReceivedBlockList list;
  EXPECT_EQ(0, list.current_position());
  list.AddBlock(0, buffer, 16);
  EXPECT_EQ(0, list.current_position());
  list.AddBlock(0, buffer, 16);
  EXPECT_EQ(16, list.ReadBytes(read_buffer.get(), read_buffer->size()));
  EXPECT_EQ(0, memcmp(buffer->data(), read_buffer->data(), 16));
  EXPECT_EQ(16, list.current_position());
}

TEST_F(CurveCPTransferTest, ReceivedBlockListCoalesce) {
  scoped_refptr<IOBufferWithSize> buffer(new IOBufferWithSize(8));
  memcpy(buffer->data(), "abcdefgh", 8);
  scoped_refptr<IOBufferWithSize> read_buffer(new IOBufferWithSize(100));

  ReceivedBlockList list;
  EXPECT_EQ(0, list.current_position());
  list.AddBlock(0, buffer, 8);
  EXPECT_EQ(0, list.current_position());
  list.AddBlock(8, buffer, 8);
  EXPECT_EQ(16, list.ReadBytes(read_buffer.get(), read_buffer->size()));
  EXPECT_EQ(0, memcmp(buffer->data(), read_buffer->data(), 8));
  EXPECT_EQ(0, memcmp(buffer->data(), read_buffer->data() + 8, 8));
  EXPECT_EQ(16, list.current_position());
}

TEST_F(CurveCPTransferTest, ReceivedBlockListGap) {
  scoped_refptr<IOBufferWithSize> buffer(new IOBufferWithSize(8));
  memcpy(buffer->data(), "abcdefgh", 8);
  scoped_refptr<IOBufferWithSize> read_buffer(new IOBufferWithSize(100));

  ReceivedBlockList list;
  EXPECT_EQ(0, list.current_position());
  list.AddBlock(0, buffer, 8);
  EXPECT_EQ(0, list.current_position());

  // Leaves a gap
  list.AddBlock(16, buffer, 8);
  EXPECT_EQ(8, list.ReadBytes(read_buffer.get(), read_buffer->size()));
  EXPECT_EQ(0, list.ReadBytes(read_buffer.get(), read_buffer->size()));
  EXPECT_EQ(8, list.current_position());

  // Fill the gap
  list.AddBlock(8, buffer, 8);
  EXPECT_EQ(16, list.ReadBytes(read_buffer.get(), read_buffer->size()));
  EXPECT_EQ(0, memcmp(buffer->data(), read_buffer->data(), 8));
  EXPECT_EQ(0, memcmp(buffer->data(), read_buffer->data() + 8, 8));
  EXPECT_EQ(24, list.current_position());
}

TEST_F(CurveCPTransferTest, ReceivedBlockListPartialRead) {
  scoped_refptr<IOBufferWithSize> buffer(new IOBufferWithSize(8));
  memcpy(buffer->data(), "abcdefgh", 8);
  scoped_refptr<IOBufferWithSize> read_buffer(new IOBufferWithSize(100));

  ReceivedBlockList list;
  EXPECT_EQ(0, list.current_position());
  list.AddBlock(0, buffer, 8);
  EXPECT_EQ(0, list.current_position());
  list.AddBlock(8, buffer, 8);
  EXPECT_EQ(3, list.ReadBytes(read_buffer.get(), 3));
  EXPECT_EQ(0, memcmp("abc", read_buffer->data(), 3));
  EXPECT_EQ(3, list.ReadBytes(read_buffer.get(), 3));
  EXPECT_EQ(0, memcmp("def", read_buffer->data(), 3));
  EXPECT_EQ(3, list.ReadBytes(read_buffer.get(), 3));
  EXPECT_EQ(0, memcmp("gha", read_buffer->data(), 3));
  EXPECT_EQ(3, list.ReadBytes(read_buffer.get(), 3));
  EXPECT_EQ(0, memcmp("bcd", read_buffer->data(), 3));
  EXPECT_EQ(3, list.ReadBytes(read_buffer.get(), 3));
  EXPECT_EQ(0, memcmp("efg", read_buffer->data(), 3));
  EXPECT_EQ(1, list.ReadBytes(read_buffer.get(), 3));
  EXPECT_EQ(0, memcmp("h", read_buffer->data(), 1));
  EXPECT_EQ(16, list.current_position());
}

/*

class PollTimerTester {
 public:
  PollTimerTester() {
    Test();
  }

  void Test() {
    struct pollfd fds[3];
    memset(fds, 0, sizeof(struct pollfd) * 3);

    while(true) {
      (void)poll(fds, 1, 2);
      base::TimeTicks now(base::TimeTicks::Now());
      if (!last_.is_null()) {
        LOG(ERROR) << "Time was " << (now - last_).InMicroseconds();
        LOG(ERROR) << "Time was " << (now - last_).InMilliseconds();
      }
      last_ = now;
    }
  }

  base::TimeTicks last_;
};

class TimerTester {
 public:
  TimerTester() {
    timer_.Start(base::TimeDelta(), this, &TimerTester::OnTimer);
  }
  void OnTimer() {
    base::TimeTicks now(base::TimeTicks::Now());
    if (!last_.is_null()) {
      LOG(ERROR) << "Time was " << (now - last_).InMicroseconds();
      LOG(ERROR) << "Time was " << (now - last_).InMilliseconds();
    }
    last_ = now;
    //timer_.Start(base::TimeDelta(), this, &TimerTester::OnTimer);
    timer_.Start(base::TimeDelta::FromMicroseconds(150),
                 this,
                 &TimerTester::OnTimer);
  }

  base::TimeTicks last_;
  base::OneShotTimer<TimerTester> timer_;
};

TEST_F(CurveCPTransferTest, MinTimer) {
  PollTimerTester tester;
  MessageLoop::current()->Run();
}

*/

}  // namespace net

int main(int argc, char**argv) {
  base::StatisticsRecorder recorder;
  NetTestSuite test_suite(argc, argv);
  return test_suite.Run();
}
