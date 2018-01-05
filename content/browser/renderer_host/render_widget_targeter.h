// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_TARGETER_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_TARGETER_H_

#include <queue>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/latency/latency_info.h"

namespace blink {
class WebInputEvent;
}  // namespace blink

namespace gfx {
class PointF;
}

namespace viz {
class FrameSinkId;
}

namespace content {

class RenderWidgetHostViewBase;

struct RenderWidgetTargetResult {
  RenderWidgetTargetResult();
  RenderWidgetTargetResult(const RenderWidgetTargetResult&);
  RenderWidgetTargetResult(RenderWidgetHostViewBase* view,
                           bool should_query_view,
                           base::Optional<gfx::PointF> location);
  ~RenderWidgetTargetResult();

  RenderWidgetHostViewBase* view = nullptr;
  bool should_query_view = false;
  base::Optional<gfx::PointF> target_location = base::nullopt;
};

class RenderWidgetTargeter {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual RenderWidgetTargetResult FindTargetSynchronously(
        RenderWidgetHostViewBase* root_view,
        const blink::WebInputEvent& event) = 0;

    virtual void DispatchEventToTarget(RenderWidgetHostViewBase* root_view,
                                       RenderWidgetHostViewBase* target,
                                       const blink::WebInputEvent& event,
                                       const ui::LatencyInfo& latency) = 0;

    virtual RenderWidgetHostViewBase* FindViewFromFrameSinkId(
        const viz::FrameSinkId& frame_sink_id) const = 0;
  };

  // The delegate must outlive this targeter.
  explicit RenderWidgetTargeter(Delegate* delegate);
  ~RenderWidgetTargeter();

  // Finds the appropriate target inside |root_view| for |event|, and dispatches
  // it through the delegate. |event| is in the coord-space of |root_view|.
  void FindTargetAndDispatch(RenderWidgetHostViewBase* root_view,
                             const blink::WebInputEvent& event,
                             const ui::LatencyInfo& latency);

 private:
  // Attempts to target and dispatch all events in the queue. It stops if it has
  // to query a client, or if the queue becomes empty.
  void FlushEventQueue();

  // Queries |target| to find the correct target for |event|.
  // |event| is in the coordinate space of |root_view|.
  // |target_location|, if set, is the location in |target|'s coordinate space.
  void QueryClient(RenderWidgetHostViewBase* root_view,
                   RenderWidgetHostViewBase* target,
                   const blink::WebInputEvent& event,
                   const ui::LatencyInfo& latency,
                   const base::Optional<gfx::PointF>& target_location);

  // |event| is in the coordinate space of |root_view|. |target_location|, if
  // set, is the location in |target|'s coordinate space.
  void FoundFrameSinkId(base::WeakPtr<RenderWidgetHostViewBase> root_view,
                        base::WeakPtr<RenderWidgetHostViewBase> target,
                        const blink::WebInputEvent& event,
                        const ui::LatencyInfo& latency,
                        const base::Optional<gfx::PointF>& target_location,
                        const viz::FrameSinkId& frame_sink_id);

  // |event| is in the coordinate space of |root_view|. |target_location|, if
  // set, is the location in |target|'s coordinate space.
  void FoundTarget(RenderWidgetHostViewBase* root_view,
                   RenderWidgetHostViewBase* target,
                   const blink::WebInputEvent& event,
                   const ui::LatencyInfo& latency,
                   const base::Optional<gfx::PointF>& target_location);

  struct TargetingRequest {
    TargetingRequest();
    TargetingRequest(TargetingRequest&& request);
    TargetingRequest& operator=(TargetingRequest&& other);
    ~TargetingRequest();

    base::WeakPtr<RenderWidgetHostViewBase> root_view;
    ui::WebScopedInputEvent event;
    ui::LatencyInfo latency;
  };

  bool request_in_flight_ = false;
  std::queue<TargetingRequest> requests_;

  Delegate* const delegate_;
  base::WeakPtrFactory<RenderWidgetTargeter> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetTargeter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_WIDGET_TARGETER_H_
