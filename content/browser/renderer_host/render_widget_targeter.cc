// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/render_widget_targeter.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/events/blink/blink_event_util.h"

namespace content {

namespace {

bool MergeEventIfPossible(const blink::WebInputEvent& event,
                          ui::WebScopedInputEvent* blink_event) {
  if (ui::CanCoalesce(event, **blink_event)) {
    *blink_event = ui::WebInputEventTraits::Clone(event);
    return true;
  }
  return false;
}

}  // namespace

RenderWidgetTargetResult::RenderWidgetTargetResult() = default;

RenderWidgetTargetResult::RenderWidgetTargetResult(
    const RenderWidgetTargetResult&) = default;

RenderWidgetTargetResult::RenderWidgetTargetResult(
    RenderWidgetHostViewBase* in_view,
    bool in_should_query_view,
    base::Optional<gfx::PointF> in_location)
    : view(in_view),
      should_query_view(in_should_query_view),
      target_location(in_location) {}

RenderWidgetTargetResult::~RenderWidgetTargetResult() = default;

RenderWidgetTargeter::TargetingRequest::TargetingRequest() = default;

RenderWidgetTargeter::TargetingRequest::TargetingRequest(
    TargetingRequest&& request) = default;

RenderWidgetTargeter::TargetingRequest& RenderWidgetTargeter::TargetingRequest::
operator=(TargetingRequest&&) = default;

RenderWidgetTargeter::TargetingRequest::~TargetingRequest() = default;

RenderWidgetTargeter::RenderWidgetTargeter(Delegate* delegate)
    : delegate_(delegate), weak_ptr_factory_(this) {
  DCHECK(delegate_);
}

RenderWidgetTargeter::~RenderWidgetTargeter() = default;

void RenderWidgetTargeter::FindTargetAndDispatch(
    RenderWidgetHostViewBase* root_view,
    const blink::WebInputEvent& event,
    const ui::LatencyInfo& latency) {
  if (request_in_flight_) {
    if (!requests_.empty()) {
      auto& request = requests_.back();
      if (MergeEventIfPossible(event, &request.event))
        return;
    }
    TargetingRequest request;
    request.root_view = root_view->GetWeakPtr();
    request.event = ui::WebInputEventTraits::Clone(event);
    request.latency = latency;
    requests_.push(std::move(request));
    return;
  }

  RenderWidgetTargetResult result =
      delegate_->FindTargetSynchronously(root_view, event);

  RenderWidgetHostViewBase* target = result.view;
  auto* event_ptr = &event;
  if (result.should_query_view) {
    DCHECK(target && result.target_location.has_value());
    QueryClient(root_view, target, *event_ptr, latency,
                result.target_location.value());
  } else {
    FoundTarget(root_view, target, *event_ptr, latency, result.target_location);
  }
}

void RenderWidgetTargeter::QueryClient(RenderWidgetHostViewBase* root_view,
                                       RenderWidgetHostViewBase* target,
                                       const blink::WebInputEvent& event,
                                       const ui::LatencyInfo& latency,
                                       const gfx::PointF& target_location) {
  DCHECK(!request_in_flight_);
  request_in_flight_ = true;
  auto* target_client =
      target->GetRenderWidgetHostImpl()->input_target_client();
  if (blink::WebInputEvent::IsMouseEventType(event.GetType()) ||
      event.GetType() == blink::WebInputEvent::kMouseWheel ||
      event.GetType() == blink::WebInputEvent::kTouchStart ||
      (blink::WebInputEvent::IsGestureEventType(event.GetType()) &&
       (static_cast<const blink::WebGestureEvent&>(event).source_device ==
            blink::WebGestureDevice::kWebGestureDeviceTouchscreen ||
        static_cast<const blink::WebGestureEvent&>(event).source_device ==
            blink::WebGestureDevice::kWebGestureDeviceTouchpad))) {
    target_client->FrameSinkIdAt(
        gfx::ToCeiledPoint(target_location),
        base::BindOnce(&RenderWidgetTargeter::FoundFrameSinkId,
                       weak_ptr_factory_.GetWeakPtr(), root_view->GetWeakPtr(),
                       target->GetWeakPtr(),
                       ui::WebInputEventTraits::Clone(event), latency,
                       target_location));
    return;
  }
  NOTREACHED();
}

void RenderWidgetTargeter::FlushEventQueue() {
  while (!request_in_flight_ && !requests_.empty()) {
    auto request = std::move(requests_.front());
    requests_.pop();
    // The root-view has gone away. Ignore this event, and try to process the
    // next event.
    if (!request.root_view) {
      continue;
    }
    FindTargetAndDispatch(request.root_view.get(), *request.event,
                          request.latency);
  }
}

void RenderWidgetTargeter::FoundFrameSinkId(
    base::WeakPtr<RenderWidgetHostViewBase> root_view,
    base::WeakPtr<RenderWidgetHostViewBase> target,
    ui::WebScopedInputEvent event,
    const ui::LatencyInfo& latency,
    const gfx::PointF& target_location,
    const viz::FrameSinkId& frame_sink_id) {
  request_in_flight_ = false;
  auto* view = delegate_->FindViewFromFrameSinkId(frame_sink_id);
  if (!view)
    view = target.get();
  // If a client was asked to find a target, then it is necessary to keep
  // asking the clients until a client claims an event for itself.
  if (view == target.get()) {
    FoundTarget(root_view.get(), view, *event, latency, target_location);
  } else {
    gfx::PointF location = target_location;
    target->TransformPointToCoordSpaceForView(location, view, &location);
    QueryClient(root_view.get(), view, *event, latency, location);
  }
}

void RenderWidgetTargeter::FoundTarget(
    RenderWidgetHostViewBase* root_view,
    RenderWidgetHostViewBase* target,
    const blink::WebInputEvent& event,
    const ui::LatencyInfo& latency,
    const base::Optional<gfx::PointF>& target_location) {
  if (!root_view)
    return;
  // TODO: Unify position conversion for all event types.
  if (blink::WebInputEvent::IsMouseEventType(event.GetType())) {
    blink::WebMouseEvent mouse_event =
        static_cast<const blink::WebMouseEvent&>(event);
    if (target_location.has_value()) {
      mouse_event.SetPositionInWidget(target_location->x(),
                                      target_location->y());
    }
    if (mouse_event.GetType() != blink::WebInputEvent::kUndefined)
      delegate_->DispatchEventToTarget(root_view, target, mouse_event, latency);
  } else if (event.GetType() == blink::WebInputEvent::kMouseWheel ||
             blink::WebInputEvent::IsTouchEventType(event.GetType()) ||
             blink::WebInputEvent::IsGestureEventType(event.GetType())) {
    DCHECK(!blink::WebInputEvent::IsGestureEventType(event.GetType()) ||
           (static_cast<const blink::WebGestureEvent&>(event).source_device ==
                blink::WebGestureDevice::kWebGestureDeviceTouchscreen ||
            static_cast<const blink::WebGestureEvent&>(event).source_device ==
                blink::WebGestureDevice::kWebGestureDeviceTouchpad));
    delegate_->DispatchEventToTarget(root_view, target, event, latency);
  } else {
    NOTREACHED();
    return;
  }
  FlushEventQueue();
}

}  // namespace content
