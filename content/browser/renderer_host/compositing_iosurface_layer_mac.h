// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_LAYER_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_LAYER_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"

namespace content {
class CompositingIOSurfaceContext;
class RenderWidgetHostViewMac;
}

// The CoreAnimation layer for drawing accelerated content.
@interface CompositingIOSurfaceLayer : CAOpenGLLayer {
 @private
  content::RenderWidgetHostViewMac* renderWidgetHostView_;
  scoped_refptr<content::CompositingIOSurfaceContext> context_;
}

@property(nonatomic, readonly)
    scoped_refptr<content::CompositingIOSurfaceContext> context;

- (id)initWithRenderWidgetHostViewMac:(content::RenderWidgetHostViewMac*)r;

// Update the scale factor of the layer to match the scale factor of the
// IOSurface.
- (void)updateScaleFactor;

// Remove this layer from the layer heirarchy, and mark that
// |renderWidgetHostView_| is no longer valid and may no longer be dereferenced.
- (void)disableCompositing;

@end

#endif  // CONTENT_BROWSER_RENDERER_HOST_COMPOSITING_IOSURFACE_LAYER_MAC_H_
