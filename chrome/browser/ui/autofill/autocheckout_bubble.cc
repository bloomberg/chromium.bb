// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autocheckout_bubble.h"

#include "base/logging.h"

namespace autofill {

#if !defined(TOOLKIT_VIEWS)
// TODO(ahutter): Implement the bubble on other platforms. See
// http://crbug.com/173416.
void ShowAutocheckoutBubble(const gfx::RectF& anchor,
                            const gfx::NativeView& native_view,
                            const base::Closure& callback) {
  NOTIMPLEMENTED();
}
#endif

}  // namespace autofill
