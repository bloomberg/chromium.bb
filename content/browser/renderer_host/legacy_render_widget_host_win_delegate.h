// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_LEGACY_RENDER_WIDGET_HOST_WIN_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_LEGACY_RENDER_WIDGET_HOST_WIN_DELEGATE_H_

#include "content/common/content_export.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

// A delegate interface for LegacyRenderWidgetHostHWND to query the
// native view accessible from the owner class.
class CONTENT_EXPORT LegacyRenderWidgetHostHWNDDelegate {
 public:
  // Get the native view accessible for the web contents.
  // Once initialized, this should always return a valid object, but the
  // object may change as the user navigates.
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() = 0;
};


}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_LEGACY_RENDER_WIDGET_HOST_WIN_DELEGATE_H_
