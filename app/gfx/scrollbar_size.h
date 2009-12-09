// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GFX_SCROLLBAR_SIZE_H_
#define APP_GFX_SCROLLBAR_SIZE_H_

namespace gfx {

// This should return the thickness, in pixels, of a scrollbar in web content.
// This needs to match the values in WebCore's
// ScrollbarThemeChromiumXXX.cpp::scrollbarThickness().
int scrollbar_size();

}  // namespace gfx

#endif  // APP_GFX_SCROLLBAR_SIZE_H_
