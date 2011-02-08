// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_request_infobar_delegate.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "testing/gtest/include/gtest/gtest.h"

// MockTabDownloadState -------------------------------------------------------

class MockTabDownloadState : public DownloadRequestLimiter::TabDownloadState {
 public:
  MockTabDownloadState();
  virtual ~MockTabDownloadState();

  // DownloadRequestLimiter::TabDownloadState
  virtual void Cancel();
  virtual void Accept();

  ConfirmInfoBarDelegate* infobar() {
    return infobar_->AsConfirmInfoBarDelegate();
  }
  void close_infobar() {
    // TODO(pkasting): Right now InfoBarDelegates delete themselves via
    // InfoBarClosed(); once InfoBars own their delegates, this can become a
    // simple reset() call and ~MockTabDownloadState() will no longer need to
    // call it.
    if (infobar_ != NULL)
      infobar_.release()->InfoBarClosed();
  }
  bool responded() const { return responded_; }
  bool accepted() const { return accepted_; }

 private:
  // The actual infobar delegate we're listening to.
  scoped_ptr<InfoBarDelegate> infobar_;

  // True if we have gotten some sort of response.
  bool responded_;

  // True if we have gotten a Accept response. Meaningless if |responded_| is
  // not true.
  bool accepted_;
};

MockTabDownloadState::MockTabDownloadState()
    : responded_(false), accepted_(false) {
  infobar_.reset(new DownloadRequestInfoBarDelegate(NULL, this));
}

MockTabDownloadState::~MockTabDownloadState() {
  close_infobar();
  EXPECT_TRUE(responded_);
}

void MockTabDownloadState::Cancel() {
  EXPECT_FALSE(responded_);
  responded_ = true;
  accepted_ = false;
}

void MockTabDownloadState::Accept() {
  EXPECT_FALSE(responded_);
  responded_ = true;
  accepted_ = true;
  static_cast<DownloadRequestInfoBarDelegate*>(infobar_.get())->set_host(NULL);
}


// Tests ----------------------------------------------------------------------

TEST(DownloadRequestInfobarDelegate, AcceptTest) {
  MockTabDownloadState state;
  state.infobar()->Accept();
  EXPECT_TRUE(state.accepted());
}

TEST(DownloadRequestInfobarDelegate, CancelTest) {
  MockTabDownloadState state;
  state.infobar()->Cancel();
  EXPECT_FALSE(state.accepted());
}

TEST(DownloadRequestInfobarDelegate, CloseTest) {
  MockTabDownloadState state;
  state.close_infobar();
  EXPECT_FALSE(state.accepted());
}
