// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOCHECKOUT_BUBBLE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOCHECKOUT_BUBBLE_H_

#include "base/callback_forward.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class RectF;
}

namespace autofill {

// Creates and shows the the Autocheckout bubble. |anchor| is the bubble's
// anchor on the screen. |native_view| is the parent view of the bubble.
// |callback| is called if the bubble is accepted by the user.
void ShowAutocheckoutBubble(const gfx::RectF& anchor,
                            const gfx::NativeView& native_view,
                            const base::Closure& callback);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOCHECKOUT_BUBBLE_H_
