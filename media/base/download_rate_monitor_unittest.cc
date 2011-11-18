// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/download_rate_monitor.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;

namespace media {

class DownloadRateMonitorTest : public ::testing::Test {
 public:
  DownloadRateMonitorTest() {
    monitor_.set_total_bytes(kMediaSizeInBytes);
  }

  virtual ~DownloadRateMonitorTest() { }

  MOCK_METHOD0(CanPlayThrough, void());

 protected:
  static const int kMediaSizeInBytes = 20 * 1024 * 1024;

  // Simulates downloading of the media file. Packets are timed evenly in
  // |ms_between_packets| intervals, starting at |starting_time|, which is
  // number of seconds since unix epoch (Jan 1, 1970).
  // Returns the number of bytes buffered in the media file after the
  // network activity.
  int SimulateNetwork(double starting_time,
                      int starting_bytes,
                      int bytes_per_packet,
                      int ms_between_packets,
                      int number_of_packets) {
    int bytes_buffered = starting_bytes;
    base::Time packet_time = base::Time::FromDoubleT(starting_time);

    monitor_.SetNetworkActivity(true);
    // Loop executes (number_of_packets + 1) times because a packet needs a
    // starting and end point.
    for (int i = 0; i < number_of_packets + 1; ++i) {
      monitor_.SetBufferedBytes(bytes_buffered, packet_time);
      packet_time += base::TimeDelta::FromMilliseconds(ms_between_packets);
      bytes_buffered += bytes_per_packet;
    }
    monitor_.SetNetworkActivity(false);
    return bytes_buffered;
  }

  DownloadRateMonitor monitor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadRateMonitorTest);
};

TEST_F(DownloadRateMonitorTest, DownloadRateGreaterThanBitrate) {
  static const int media_bitrate = 1024 * 1024 * 8;

  // Simulate downloading at double the media's bitrate.
  monitor_.Start(base::Bind(&DownloadRateMonitorTest::CanPlayThrough,
                            base::Unretained(this)), media_bitrate);
  EXPECT_CALL(*this, CanPlayThrough());
  SimulateNetwork(1, 0, 2 * media_bitrate / 8, 1000, 10);
}

// If the user pauses and the pipeline stops downloading data, make sure the
// DownloadRateMonitor understands that the download is not stalling.
TEST_F(DownloadRateMonitorTest, DownloadRateGreaterThanBitrate_Pause) {
  static const int media_bitrate = 1024 * 1024 * 8;
  static const int download_byte_rate = 1.1 * media_bitrate / 8;

  // Start downloading faster than the media's bitrate.
  monitor_.Start(base::Bind(&DownloadRateMonitorTest::CanPlayThrough,
                            base::Unretained(this)), media_bitrate);
  EXPECT_CALL(*this, CanPlayThrough());
  int buffered = SimulateNetwork(1, 0, download_byte_rate, 1000, 2);

  // Then "pause" for 3 minutes and continue downloading at same rate.
  SimulateNetwork(60 * 3, buffered, download_byte_rate, 1000, 4);
}

TEST_F(DownloadRateMonitorTest, DownloadRateGreaterThanBitrate_SeekForward) {
  static const int media_bitrate = 1024 * 1024 * 8;
  static const int download_byte_rate = 1.1 * media_bitrate / 8;

  // Start downloading faster than the media's bitrate.
  EXPECT_CALL(*this, CanPlayThrough());
  monitor_.Start(base::Bind(&DownloadRateMonitorTest::CanPlayThrough,
                            base::Unretained(this)), media_bitrate);
  SimulateNetwork(1, 0, download_byte_rate, 1000, 2);

  // Then seek forward mid-file and continue downloading at same rate.
  SimulateNetwork(4, kMediaSizeInBytes / 2, download_byte_rate, 1000, 4);
}

TEST_F(DownloadRateMonitorTest, DownloadRateGreaterThanBitrate_SeekBackward) {
  static const int media_bitrate = 1024 * 1024 * 8;
  static const int download_byte_rate = 1.1 * media_bitrate / 8;

  // Start downloading faster than the media's bitrate, in middle of file.
  monitor_.Start(base::Bind(&DownloadRateMonitorTest::CanPlayThrough,
                            base::Unretained(this)), media_bitrate);
  SimulateNetwork(1, kMediaSizeInBytes / 2, download_byte_rate, 1000, 2);

  // Then seek back to beginning and continue downloading at same rate.
  EXPECT_CALL(*this, CanPlayThrough());
  SimulateNetwork(4, 0, download_byte_rate, 1000, 4);
}

TEST_F(DownloadRateMonitorTest, DownloadRateLessThanBitrate) {
  static const int media_bitrate = 1024 * 1024 * 8;

  // Simulate downloading at half the media's bitrate.
  EXPECT_CALL(*this, CanPlayThrough())
      .Times(0);
  monitor_.Start(base::Bind(&DownloadRateMonitorTest::CanPlayThrough,
                            base::Unretained(this)), media_bitrate);
  SimulateNetwork(1, 0, media_bitrate / 8 / 2, 1000, 10);
}

TEST_F(DownloadRateMonitorTest, MediaIsLoaded) {
  static const int media_bitrate = 1024 * 1024 * 8;

  monitor_.set_loaded(true);

  // Simulate no data downloaded (source is already loaded).
  EXPECT_CALL(*this, CanPlayThrough());
  monitor_.Start(base::Bind(&DownloadRateMonitorTest::CanPlayThrough,
                            base::Unretained(this)), media_bitrate);
  SimulateNetwork(1, 0, 0, 1000, 10);
}

TEST_F(DownloadRateMonitorTest, VeryFastDownloadRate) {
  static const int media_bitrate = 1024 * 1024 * 8;

  // Simulate downloading half the video very quickly in one chunk.
  monitor_.Start(base::Bind(&DownloadRateMonitorTest::CanPlayThrough,
                            base::Unretained(this)), media_bitrate);
  EXPECT_CALL(*this, CanPlayThrough());
  SimulateNetwork(1, 0, kMediaSizeInBytes / 2, 10, 1);
}

TEST_F(DownloadRateMonitorTest, DownloadEntireVideo) {
  static const int seconds_of_data = 20;
  static const int media_bitrate = kMediaSizeInBytes * 8 / seconds_of_data;

  // Simulate downloading entire video at half the bitrate of the video.
  monitor_.Start(base::Bind(&DownloadRateMonitorTest::CanPlayThrough,
                            base::Unretained(this)), media_bitrate);
  EXPECT_CALL(*this, CanPlayThrough());
  SimulateNetwork(1, 0, media_bitrate / 8 / 2, 1000, seconds_of_data * 2);
}

}  // namespace media
