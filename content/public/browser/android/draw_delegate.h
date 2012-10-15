// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_DRAW_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_DRAW_DELEGATE_H_

#include "base/callback.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

namespace content {

class RenderWidgetHostView;

// TODO(sievers): Route sizing of views through ContentViewCore
// and remove this class.
class DrawDelegate {
 public:
  static DrawDelegate* GetInstance();
  virtual ~DrawDelegate() { }

  virtual void SetBounds(const gfx::Size& size) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_DRAW_DELEGATE_H_
