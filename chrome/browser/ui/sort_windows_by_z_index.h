// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SORT_WINDOWS_BY_Z_INDEX_H_
#define CHROME_BROWSER_UI_SORT_WINDOWS_BY_Z_INDEX_H_

#include <vector>

#include "base/callback_forward.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

using SortWindowsByZIndexCallback =
    base::OnceCallback<void(std::vector<gfx::NativeWindow>)>;

// Returns a list with the windows in |windows| sorted by z-index, from topmost
// to bottommost, via an asynchronous call to |callback| on the current
// sequence. Windows from |windows| that have been deleted by the time
// |callback| runs won't be part of the sorted list.
//
// TODO(fdoray): Implement this on all platforms. https://crbug.com/731145
void SortWindowsByZIndex(const std::vector<gfx::NativeWindow>& windows,
                         SortWindowsByZIndexCallback callback);

}  // namespace ui

#endif  // CHROME_BROWSER_UI_SORT_WINDOWS_BY_Z_INDEX_H_
