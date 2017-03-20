// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SESSION_SESSION_STATE_DELEGATE_H_
#define ASH_COMMON_SESSION_SESSION_STATE_DELEGATE_H_

#include "ash/ash_export.h"

namespace gfx {
class ImageSkia;
}

namespace ash {

class WmWindow;

// Delegate for checking and modifying the session state.
// DEPRECATED in favor of SessionController/SessionControllerClient for mash.
// TODO(xiyuan): Remove this when SessionController etc are ready.
class ASH_EXPORT SessionStateDelegate {
 public:
  virtual ~SessionStateDelegate() {}

  // Whether or not the window's title should show the avatar.
  virtual bool ShouldShowAvatar(WmWindow* window) const = 0;

  // Returns the avatar image for the specified window.
  virtual gfx::ImageSkia GetAvatarImageForWindow(WmWindow* window) const = 0;
};

}  // namespace ash

#endif  // ASH_COMMON_SESSION_SESSION_STATE_DELEGATE_H_
