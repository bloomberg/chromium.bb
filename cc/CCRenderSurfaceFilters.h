// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CCRenderSurfaceFilters_h
#define CCRenderSurfaceFilters_h

#if USE(ACCELERATED_COMPOSITING)

class GrContext;
class SkBitmap;

namespace WebKit {
class WebFilterOperations;
class WebGraphicsContext3D;
}

namespace cc {
class FloatSize;

class CCRenderSurfaceFilters {
public:
    static SkBitmap apply(const WebKit::WebFilterOperations& filters, unsigned textureId, const FloatSize&, WebKit::WebGraphicsContext3D*, GrContext*);
    static WebKit::WebFilterOperations optimize(const WebKit::WebFilterOperations& filters);

private:
    CCRenderSurfaceFilters();
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
