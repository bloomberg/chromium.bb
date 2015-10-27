// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_OWNER_DELEGATE_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_OWNER_DELEGATE_H_

#include "content/common/content_export.h"

namespace content {

//
// RenderWidgetHostOwnerDelegate
//
//  An interface implemented by an object owning a RenderWidgetHost. This is
//  intended to be temporary until the RenderViewHostImpl and
//  RenderWidgetHostImpl classes are disentangled; see http://crbug.com/542477
//  and http://crbug.com/478281.
class CONTENT_EXPORT RenderWidgetHostOwnerDelegate {
 protected:
  virtual ~RenderWidgetHostOwnerDelegate() {}
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_OWNER_DELEGATE_H_
