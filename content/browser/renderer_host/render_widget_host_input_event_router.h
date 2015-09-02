// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_INPUT_EVENT_ROUTER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_INPUT_EVENT_ROUTER_H_

#include <stdint.h>

#include "base/containers/hash_tables.h"
#include "content/common/content_export.h"

namespace blink {
class WebMouseEvent;
class WebMouseWheelEvent;
}

namespace gfx {
class Point;
}

namespace content {

class RenderWidgetHostViewBase;

// Class owned by WebContentsImpl for the purpose of directing input events
// to the correct RenderWidgetHost on pages with multiple RenderWidgetHosts.
// It maintains a mapping of RenderWidgetHostViews to Surface IDs that they
// own. When an input event requires routing based on window coordinates,
// this class requests a Surface hit test from the provided |root_view| and
// forwards the event to the owning RWHV of the returned Surface ID.
class CONTENT_EXPORT RenderWidgetHostInputEventRouter {
 public:
  RenderWidgetHostInputEventRouter();
  ~RenderWidgetHostInputEventRouter();

  void RouteMouseEvent(RenderWidgetHostViewBase* root_view,
                       blink::WebMouseEvent* event);
  void RouteMouseWheelEvent(RenderWidgetHostViewBase* root_view,
                            blink::WebMouseWheelEvent* event);

  void AddSurfaceIdNamespaceOwner(uint32_t id, RenderWidgetHostViewBase* owner);
  void RemoveSurfaceIdNamespaceOwner(uint32_t id);

 private:
  RenderWidgetHostViewBase* FindEventTarget(RenderWidgetHostViewBase* root_view,
                                            const gfx::Point& point,
                                            gfx::Point* transformed_point);

  typedef base::hash_map<uint32_t, RenderWidgetHostViewBase*>
      SurfaceIdNamespaceOwnerMap;
  SurfaceIdNamespaceOwnerMap owner_map_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostInputEventRouter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_HOST_INPUT_EVENT_ROUTER_H_
