// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_FRAME_UTIL_H_
#define ASH_FRAME_FRAME_UTIL_H_

#include "ash/ash_export.h"

namespace content {
class BrowserContext;
}

namespace gfx {
class Image;
}

namespace ash {

// Returns the avatar image to be used in the title bar
// for the user associated witht the |context|.
ASH_EXPORT gfx::Image GetAvatarImageForContext(
    content::BrowserContext* context);

}  // namespace ash

#endif  // ASH_FRAME_FRAME_UTIL_H_

