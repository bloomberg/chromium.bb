// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/test/test_session_state_delegate.h"

namespace ash {
namespace test {

TestSessionStateDelegate::TestSessionStateDelegate() = default;
TestSessionStateDelegate::~TestSessionStateDelegate() = default;

void TestSessionStateDelegate::SetUserImage(const gfx::ImageSkia& user_image) {
  user_image_ = user_image;
}

bool TestSessionStateDelegate::ShouldShowAvatar(WmWindow* window) const {
  return !user_image_.isNull();
}

gfx::ImageSkia TestSessionStateDelegate::GetAvatarImageForWindow(
    WmWindow* window) const {
  return gfx::ImageSkia();
}

}  // namespace test
}  // namespace ash
