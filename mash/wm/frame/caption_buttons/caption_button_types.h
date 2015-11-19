// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_FRAME_CAPTION_BUTTONS_CAPTION_BUTTON_TYPES_H_
#define MASH_WM_FRAME_CAPTION_BUTTONS_CAPTION_BUTTON_TYPES_H_

namespace mash {
namespace wm {

// These are the icon types that a caption button can have. The size button's
// action (SnapType) can be different from its icon.
enum CaptionButtonIcon {
  CAPTION_BUTTON_ICON_MINIMIZE,
  CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
  CAPTION_BUTTON_ICON_CLOSE,
  CAPTION_BUTTON_ICON_LEFT_SNAPPED,
  CAPTION_BUTTON_ICON_RIGHT_SNAPPED,
  CAPTION_BUTTON_ICON_BACK,
  CAPTION_BUTTON_ICON_LOCATION,
  CAPTION_BUTTON_ICON_COUNT
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_FRAME_CAPTION_BUTTONS_CAPTION_BUTTON_TYPES_H_
