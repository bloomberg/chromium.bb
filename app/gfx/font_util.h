// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GFX_FONT_UTIL_H_
#define APP_GFX_FONT_UTIL_H_

#include "base/gfx/size.h"

namespace gfx {

class Font;

// Returns the preferred size of the contents view of a window based on
// its localized size data and the given font. The width in cols is held in a
// localized string resource identified by |col_resource_id|, the height in the
// same fashion.
int GetLocalizedContentsWidthForFont(int col_resource_id,
                                     const gfx::Font& font);
int GetLocalizedContentsHeightForFont(int row_resource_id,
                                      const gfx::Font& font);
gfx::Size GetLocalizedContentsSizeForFont(int col_resource_id,
                                          int row_resource_id,
                                          const gfx::Font& font);

}  // namespace gfx

#endif  // APP_GFX_FONT_UTIL_H_

