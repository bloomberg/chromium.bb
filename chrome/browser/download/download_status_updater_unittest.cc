// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/download_status_updater.h"
#include "chrome/browser/download/download_status_updater_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockDelegate : public DownloadStatusUpdaterDelegate {
 public:
  explicit MockDelegate(DownloadStatusUpdater* updater)
      : updater_(updater->AsWeakPtr()),
        is_download_progress_known_(true),
        in_progress_download_count_(0),
        received_bytes_(0),
        total_bytes_(0) {
    EXPECT_TRUE(updater);
    if (updater_)
      updater_->AddDelegate(this);
  }

  ~MockDelegate() {
    EXPECT_TRUE(updater_);
    if (updater_)
      updater_->RemoveDelegate(this);
  }

  // Overriden from DownloadStatusUpdaterDelegate:
  virtual bool IsDownloadProgressKnown() {
    return is_download_progress_known_;
  }

  virtual int64 GetInProgressDownloadCount() {
    return in_progress_download_count_;
  }

  virtual int64 GetReceivedDownloadBytes() {
    return received_bytes_;
  }

  virtual int64 GetTotalDownloadBytes() {
    return total_bytes_;
  }

  void set_is_download_progress_known(bool progress_known) {
    is_download_progress_known_ = progress_known;
  }

  void set_in_progress_download_count(int64 download_count) {
    in_progress_download_count_ = download_count;
  }

  void set_received_bytes(int64 received_bytes) {
    received_bytes_ = received_bytes;
  }

  void set_total_bytes(int64 total_bytes) {
    total_bytes_ = total_bytes;
  }

 private:
  base::WeakPtr<DownloadStatusUpdater> updater_;

  bool is_download_progress_known_;
  int64 in_progress_download_count_;
  int64 received_bytes_;
  int64 total_bytes_;

  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

}  // namespace

class DownloadStatusUpdaterTest : public testing::Test {
 protected:
  DownloadStatusUpdater updater_;
};

TEST_F(DownloadStatusUpdaterTest, Basic) {
  float progress = -1;
  EXPECT_TRUE(updater_.GetProgress(&progress));
  EXPECT_FLOAT_EQ(0, progress);
  EXPECT_EQ(0, updater_.GetInProgressDownloadCount());
}

TEST_F(DownloadStatusUpdaterTest, OneDelegate) {
  MockDelegate delegate(&updater_);

  {
    float progress = -1;
    EXPECT_TRUE(updater_.GetProgress(&progress));
    EXPECT_FLOAT_EQ(0, progress);
    EXPECT_EQ(0, updater_.GetInProgressDownloadCount());
  }

  delegate.set_in_progress_download_count(1);
  delegate.set_received_bytes(21);
  delegate.set_total_bytes(42);

  {
    float progress = -1;
    EXPECT_TRUE(updater_.GetProgress(&progress));
    EXPECT_FLOAT_EQ(static_cast<float>(1) / 2, progress);
    EXPECT_EQ(1, updater_.GetInProgressDownloadCount());
  }

  delegate.set_is_download_progress_known(false);

  {
    float progress = -1;
    EXPECT_FALSE(updater_.GetProgress(&progress));
    EXPECT_FLOAT_EQ(0, progress);
    EXPECT_EQ(1, updater_.GetInProgressDownloadCount());
  }
}

TEST_F(DownloadStatusUpdaterTest, MultipleDelegates) {
  scoped_ptr<MockDelegate> delegate1(new MockDelegate(&updater_));
  scoped_ptr<MockDelegate> delegate2(new MockDelegate(&updater_));

  {
    float progress = -1;
    EXPECT_TRUE(updater_.GetProgress(&progress));
    EXPECT_FLOAT_EQ(0, progress);
    EXPECT_EQ(0, updater_.GetInProgressDownloadCount());
  }

  delegate1->set_in_progress_download_count(1);
  delegate1->set_received_bytes(14);
  delegate1->set_total_bytes(21);

  delegate2->set_in_progress_download_count(2);
  delegate2->set_received_bytes(7);
  delegate2->set_total_bytes(21);

  {
    float progress = -1;
    EXPECT_TRUE(updater_.GetProgress(&progress));
    EXPECT_FLOAT_EQ(static_cast<float>(1) / 2, progress);
    EXPECT_EQ(3, updater_.GetInProgressDownloadCount());
  }

  delegate1->set_is_download_progress_known(false);

  {
    float progress = -1;
    EXPECT_FALSE(updater_.GetProgress(&progress));
    EXPECT_FLOAT_EQ(0, progress);
    EXPECT_EQ(3, updater_.GetInProgressDownloadCount());
  }

  delegate1.reset();

  {
    float progress = -1;
    EXPECT_TRUE(updater_.GetProgress(&progress));
    EXPECT_FLOAT_EQ(static_cast<float>(1) / 3, progress);
    EXPECT_EQ(2, updater_.GetInProgressDownloadCount());
  }
}
