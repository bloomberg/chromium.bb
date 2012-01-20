// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/download_rate_monitor.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Mock;

namespace media {

class DownloadRateMonitorTest : public ::testing::Test {
 public:
  DownloadRateMonitorTest()
      : canplaythrough_cb_(base::Bind(&DownloadRateMonitorTest::CanPlayThrough,
                                      base::Unretained(this))) { }

  virtual ~DownloadRateMonitorTest() { }

  MOCK_METHOD0(CanPlayThrough, void());

 protected:
  static const int kMediaDuration = 120;
  static const int kMediaBitrate = 1024 * 1024 * 8;
  static const int kMediaByterate = kMediaBitrate / 8;
  static const int kMediaSizeInBytes = kMediaDuration * kMediaByterate;
  static const int kDeferThreshold = 30 * kMediaByterate;

  // Simulates downloading |bytes_to_download| bytes of the media file, at
  // |download_speed_in_bps| bytes per second.
  void SimulateNetwork(int bytes_to_download, int download_speed_in_bps) {
    int bytes_downloaded = 0;
    while (bytes_downloaded < bytes_to_download &&
           !data_source_.is_deferred()) {
      int bytes_left_to_download = bytes_to_download - bytes_downloaded;
      int packet_size = std::min(download_speed_in_bps, bytes_left_to_download);
      time_elapsed_ += base::TimeDelta::FromMilliseconds(
          1000 * packet_size / download_speed_in_bps);
      data_source_.ReceiveData(
          packet_size, time_elapsed_ + base::Time::UnixEpoch());
      bytes_downloaded += packet_size;
    }
  }

  void Initialize() {
    Initialize(false, false);
  }

  void Initialize(bool streaming, bool local_source) {
    data_source_.Initialize(
        kMediaBitrate, streaming, local_source, canplaythrough_cb_);
  }

  bool DownloadIsDeferred() {
    return data_source_.is_deferred();
  }

  void SeekTo(int offset) {
    data_source_.SeekTo(offset);
  }

  // Returns the size of |seconds| seconds of media data in bytes.
  int SecondsToBytes(int seconds) {
    return seconds * kMediaByterate;
  }

 private:
  // Helper class to simulate a buffered data source.
  class FakeDataSource {
   public:
    FakeDataSource()
        : total_bytes_(kMediaSizeInBytes),
          buffered_bytes_(0),
          offset_(0),
          defer_threshold_(kDeferThreshold),
          is_downloading_data_(true) { }

    bool is_deferred() { return !is_downloading_data_; }

    void ReceiveData(int bytes_received, base::Time packet_time) {
      CHECK(is_downloading_data_);
      buffered_bytes_ += bytes_received;
      // This simulates that the download is being deferred.
      if (buffered_bytes_ >= defer_threshold_)
        is_downloading_data_ = false;

      // Update monitor's state.
      monitor_.SetNetworkActivity(is_downloading_data_);
      monitor_.set_total_bytes(total_bytes_);
      monitor_.SetBufferedBytes(buffered_bytes_ + offset_, packet_time);
    }

    void Initialize(int bitrate, bool streaming, bool local_source,
                    const base::Closure& canplaythrough_cb) {
      monitor_.Start(canplaythrough_cb, bitrate, streaming, local_source);
    }

    void SeekTo(int byte_position) {
      offset_ = byte_position;
      // Simulate recreating the buffer after a seek.
      buffered_bytes_ = 0;
      is_downloading_data_ = true;
    }

   private:
    DownloadRateMonitor monitor_;

    // Size of the media file being downloaded, in bytes.
    int total_bytes_;

    // Number of bytes currently in buffer.
    int buffered_bytes_;

    // The byte offset into the file from which the download began.
    int offset_;

    // The size of |buffered_bytes_| at which downloading should defer.
    int defer_threshold_;

    // True if download is active (not deferred_, false otherwise.
    bool is_downloading_data_;
  };

  FakeDataSource data_source_;
  base::Closure canplaythrough_cb_;

  // Amount of time elapsed while downloading data.
  base::TimeDelta time_elapsed_;

  DISALLOW_COPY_AND_ASSIGN(DownloadRateMonitorTest);
};

TEST_F(DownloadRateMonitorTest, DownloadRateGreaterThanBitrate) {
  // Simulate downloading at double the media's bitrate.
  Initialize();
  EXPECT_CALL(*this, CanPlayThrough());
  SimulateNetwork(SecondsToBytes(20), 2 * kMediaByterate);
}

TEST_F(DownloadRateMonitorTest, DownloadRateLessThanBitrate_Defer) {
  Initialize();

  // Download slower than the media's bitrate, but buffer enough data such that
  // the data source defers.
  EXPECT_CALL(*this, CanPlayThrough());
  SimulateNetwork(kDeferThreshold, 0.3 * kMediaByterate);
  CHECK(DownloadIsDeferred());
}

TEST_F(DownloadRateMonitorTest, DownloadRateGreaterThanBitrate_SeekForward) {
  // Start downloading faster than the media's bitrate.
  Initialize();
  SimulateNetwork(SecondsToBytes(3), 1.1 * kMediaByterate);

  // Then seek forward mid-file and continue downloading at same rate.
  SeekTo(kMediaSizeInBytes / 2);
  EXPECT_CALL(*this, CanPlayThrough());
  SimulateNetwork(7 * kMediaByterate, 1.1 * kMediaByterate);

  // Verify deferring is not what caused CanPlayThrough to fire.
  CHECK(!DownloadIsDeferred());
}

TEST_F(DownloadRateMonitorTest, DownloadRateGreaterThanBitrate_SeekBackward) {
  // Start downloading faster than the media's bitrate, in middle of file.
  Initialize();
  SeekTo(kMediaSizeInBytes / 2);
  SimulateNetwork(SecondsToBytes(3), 1.1 * kMediaByterate);

  // Then seek back to beginning and continue downloading at same rate.
  SeekTo(0);
  EXPECT_CALL(*this, CanPlayThrough());
  SimulateNetwork(SecondsToBytes(7), 1.1 * kMediaByterate);

  // Verify deferring is not what caused CanPlayThrough to fire.
  CHECK(!DownloadIsDeferred());
}

TEST_F(DownloadRateMonitorTest, DownloadRateLessThanByterate) {
  // Simulate downloading at half the media's bitrate.
  EXPECT_CALL(*this, CanPlayThrough())
      .Times(0);
  Initialize();
  SimulateNetwork(SecondsToBytes(10), kMediaByterate / 2);
}

TEST_F(DownloadRateMonitorTest, MediaSourceIsLocal) {
  // Simulate no data downloaded.
  EXPECT_CALL(*this, CanPlayThrough());
  Initialize(false, true);
}

TEST_F(DownloadRateMonitorTest, MediaSourceIsStreaming) {
  // Simulate downloading at the media's bitrate while streaming.
  EXPECT_CALL(*this, CanPlayThrough());
  Initialize(true, false);
  SimulateNetwork(SecondsToBytes(10), kMediaByterate);
}

TEST_F(DownloadRateMonitorTest, VeryFastDownloadRate) {
  // Simulate downloading half the video very quickly in one chunk.
  Initialize();
  EXPECT_CALL(*this, CanPlayThrough());
  SimulateNetwork(kMediaSizeInBytes / 2, kMediaSizeInBytes * 10);
}

TEST_F(DownloadRateMonitorTest, DownloadEntireVideo) {
  // Simulate downloading entire video at half the bitrate of the video.
  Initialize();
  EXPECT_CALL(*this, CanPlayThrough());
  SimulateNetwork(kMediaSizeInBytes, kMediaByterate / 2);
}

TEST_F(DownloadRateMonitorTest, DownloadEndOfVideo) {
  Initialize();
  // Seek to 10s before the end of the file, then download the remainder of the
  // file at half the bitrate.
  SeekTo((kMediaDuration - 10) * kMediaByterate);
  EXPECT_CALL(*this, CanPlayThrough());
  SimulateNetwork(SecondsToBytes(10), kMediaByterate/ 2);

  // Verify deferring is not what caused CanPlayThrough to fire.
  CHECK(!DownloadIsDeferred());
}

}  // namespace media
