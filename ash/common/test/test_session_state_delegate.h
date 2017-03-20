// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_TEST_TEST_SESSION_STATE_DELEGATE_H_
#define ASH_COMMON_TEST_TEST_SESSION_STATE_DELEGATE_H_

#include "ash/common/session/session_state_delegate.h"
#include "base/macros.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace test {

class TestSessionStateDelegate : public SessionStateDelegate {
 public:
  TestSessionStateDelegate();
  ~TestSessionStateDelegate() override;

  // SessionStateDelegate:
  bool ShouldShowAvatar(WmWindow* window) const override;
  gfx::ImageSkia GetAvatarImageForWindow(WmWindow* window) const override;

  // Setting non NULL image enables avatar icon.
  void SetUserImage(const gfx::ImageSkia& user_image);

 private:
  gfx::ImageSkia user_image_;

  DISALLOW_COPY_AND_ASSIGN(TestSessionStateDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_COMMON_TEST_TEST_SESSION_STATE_DELEGATE_H_
