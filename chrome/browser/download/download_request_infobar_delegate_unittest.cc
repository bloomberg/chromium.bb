// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/download/download_request_infobar_delegate_android.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "testing/gtest/include/gtest/gtest.h"

// MockTabDownloadState -------------------------------------------------------

class MockTabDownloadState : public DownloadRequestLimiter::TabDownloadState {
 public:
  MockTabDownloadState();
  ~MockTabDownloadState() override;

  // DownloadRequestLimiter::TabDownloadState:
  void Cancel() override;
  void Accept() override;
  void CancelOnce() override;

  ConfirmInfoBarDelegate* infobar_delegate() { return infobar_delegate_.get(); }
  void delete_infobar_delegate() { infobar_delegate_.reset(); }
  bool responded() const { return responded_; }
  bool accepted() const { return accepted_; }

 private:
  // The actual infobar delegate we're listening to.
  scoped_ptr<DownloadRequestInfoBarDelegateAndroid> infobar_delegate_;

  // True if we have gotten some sort of response.
  bool responded_;

  // True if we have gotten a Accept response. Meaningless if |responded_| is
  // not true.
  bool accepted_;

  // To produce weak pointers for infobar_ construction.
  base::WeakPtrFactory<MockTabDownloadState> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockTabDownloadState);
};

MockTabDownloadState::MockTabDownloadState()
    : responded_(false),
      accepted_(false),
      weak_ptr_factory_(this) {
  infobar_delegate_ = DownloadRequestInfoBarDelegateAndroid::Create(
      weak_ptr_factory_.GetWeakPtr());
}

MockTabDownloadState::~MockTabDownloadState() {
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
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void MockTabDownloadState::CancelOnce() {
  Cancel();
}


// Tests ----------------------------------------------------------------------

TEST(DownloadRequestInfoBarDelegateAndroid, AcceptTest) {
  MockTabDownloadState state;
  if (state.infobar_delegate()->Accept())
    state.delete_infobar_delegate();
  EXPECT_TRUE(state.accepted());
}

TEST(DownloadRequestInfoBarDelegateAndroid, CancelTest) {
  MockTabDownloadState state;
  if (state.infobar_delegate()->Cancel())
    state.delete_infobar_delegate();
  EXPECT_FALSE(state.accepted());
}

TEST(DownloadRequestInfoBarDelegateAndroid, CloseTest) {
  MockTabDownloadState state;
  state.delete_infobar_delegate();
  EXPECT_FALSE(state.accepted());
}
