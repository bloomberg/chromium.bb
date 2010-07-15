// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_infobar_delegate.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockTabDownloadState : public DownloadRequestLimiter::TabDownloadState {
 public:
  MockTabDownloadState() : responded_(false), accepted_(false) {
  }

  virtual ~MockTabDownloadState() {
    EXPECT_TRUE(responded_);
  }

  virtual void Accept() {
    EXPECT_FALSE(responded_);
    responded_ = true;
    accepted_ = true;
  }

  virtual void Cancel() {
    EXPECT_FALSE(responded_);
    responded_ = true;
    accepted_ = false;
  }

  bool responded() {
    return responded_;
  }

  bool accepted() {
    return responded_ && accepted_;
  }

 private:
  // True if we have gotten some sort of response.
  bool responded_;

  // True if we have gotten a Accept response. Meaningless if |responded_| is
  // not true.
  bool accepted_;
};

TEST(DownloadRequestInfobar, AcceptTest) {
  MockTabDownloadState state;
  DownloadRequestInfoBarDelegate infobar(NULL, &state);
  infobar.Accept();
  EXPECT_TRUE(state.accepted());
}

TEST(DownloadRequestInfobar, CancelTest) {
  MockTabDownloadState state;
  DownloadRequestInfoBarDelegate infobar(NULL, &state);
  infobar.Cancel();
  EXPECT_FALSE(state.accepted());
}

TEST(DownloadRequestInfobar, CloseTest) {
  MockTabDownloadState state;
  DownloadRequestInfoBarDelegate infobar(NULL, &state);
  infobar.InfoBarClosed();
  EXPECT_FALSE(state.accepted());
}
