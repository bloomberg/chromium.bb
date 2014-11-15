// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/overscroll_controller_android.h"

#include "base/android/build_info.h"
#include "base/bind.h"
#include "cc/layers/layer.h"
#include "cc/output/compositor_frame_metadata.h"
#include "content/browser/android/edge_effect.h"
#include "content/browser/android/edge_effect_l.h"
#include "content/common/input/did_overscroll_params.h"
#include "content/public/browser/web_contents.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/android/window_android_compositor.h"

namespace content {
namespace {

// Used for conditional creation of EdgeEffect types for the overscroll glow.
const int kAndroidLSDKVersion = 21;

scoped_ptr<EdgeEffectBase> CreateGlowEdgeEffect(
    ui::SystemUIResourceManager* resource_manager,
    float dpi_scale) {
  DCHECK(resource_manager);
  static bool use_l_flavoured_effect =
      base::android::BuildInfo::GetInstance()->sdk_int() >= kAndroidLSDKVersion;
  if (use_l_flavoured_effect)
    return scoped_ptr<EdgeEffectBase>(new EdgeEffectL(resource_manager));

  return scoped_ptr<EdgeEffectBase>(
      new EdgeEffect(resource_manager, dpi_scale));
}

}  // namespace

OverscrollControllerAndroid::OverscrollControllerAndroid(
    WebContents* web_contents,
    ui::WindowAndroidCompositor* compositor,
    float dpi_scale)
    : WebContentsObserver(web_contents),
      compositor_(compositor),
      dpi_scale_(dpi_scale),
      enabled_(true),
      glow_effect_(base::Bind(&CreateGlowEdgeEffect,
                              &compositor->GetSystemUIResourceManager(),
                              dpi_scale_)),
      refresh_effect_(&compositor->GetSystemUIResourceManager(), this),
      triggered_refresh_active_(false) {
  DCHECK(web_contents);
}

OverscrollControllerAndroid::~OverscrollControllerAndroid() {
}

bool OverscrollControllerAndroid::WillHandleGestureEvent(
    const blink::WebGestureEvent& event) {
  if (!enabled_)
    return false;

  bool handled = false;
  bool maybe_needs_animate = false;
  switch (event.type) {
    case blink::WebInputEvent::GestureScrollBegin:
      refresh_effect_.OnScrollBegin();
      break;

    case blink::WebInputEvent::GestureScrollUpdate: {
      gfx::Vector2dF scroll_delta(event.data.scrollUpdate.deltaX,
                                  event.data.scrollUpdate.deltaY);
      scroll_delta.Scale(dpi_scale_);
      maybe_needs_animate = true;
      handled = refresh_effect_.WillHandleScrollUpdate(scroll_delta);
    } break;

    case blink::WebInputEvent::GestureScrollEnd:
      refresh_effect_.OnScrollEnd(gfx::Vector2dF());
      maybe_needs_animate = true;
      break;

    case blink::WebInputEvent::GestureFlingStart: {
      gfx::Vector2dF scroll_velocity(event.data.flingStart.velocityX,
                                     event.data.flingStart.velocityY);
      scroll_velocity.Scale(dpi_scale_);
      refresh_effect_.OnScrollEnd(scroll_velocity);
      if (refresh_effect_.IsActive()) {
        // TODO(jdduke): Figure out a cleaner way of suppressing a fling.
        // It's important that the any downstream code sees a scroll-ending
        // event (in this case GestureFlingStart) if it has seen a scroll begin.
        // Thus, we cannot simply consume the fling. Changing the event type to
        // a GestureScrollEnd might work in practice, but could lead to
        // unexpected results. For now, simply truncate the fling velocity, but
        // not to zero as downstream code may not expect a zero-velocity fling.
        blink::WebGestureEvent& modified_event =
            const_cast<blink::WebGestureEvent&>(event);
        modified_event.data.flingStart.velocityX = .01f;
        modified_event.data.flingStart.velocityY = .01f;
      }
      maybe_needs_animate = true;
    } break;

    default:
      break;
  }

  if (maybe_needs_animate && refresh_effect_.IsActive())
    SetNeedsAnimate();

  return handled;
}

void OverscrollControllerAndroid::OnGestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  if (!enabled_)
    return;

  // The overscroll effect requires an explicit release signal that may not be
  // sent from the renderer compositor.
  if (event.type == blink::WebInputEvent::GestureScrollEnd ||
      event.type == blink::WebInputEvent::GestureFlingStart) {
    OnOverscrolled(DidOverscrollParams());
  }

  if (event.type == blink::WebInputEvent::GestureScrollUpdate) {
    // The effect should only be allowed if both the causal touch events go
    // unconsumed and the generated scroll events go unconsumed.
    // TODO(jdduke): Prevent activation if the first touchmove was consumed,
    // i.e., the first GSU was prevented.
    bool consumed = ack_result == INPUT_EVENT_ACK_STATE_CONSUMED;
    refresh_effect_.OnScrollUpdateAck(consumed);
  }
}

void OverscrollControllerAndroid::OnOverscrolled(
    const DidOverscrollParams& params) {
  if (!enabled_)
    return;

  if (refresh_effect_.IsActive() ||
      refresh_effect_.IsAwaitingScrollUpdateAck()) {
    // An active (or potentially active) refresh effect should always pre-empt
    // the passive glow effect.
    return;
  }

  if (glow_effect_.OnOverscrolled(
      base::TimeTicks::Now(),
      gfx::ScaleVector2d(params.accumulated_overscroll, dpi_scale_),
      gfx::ScaleVector2d(params.latest_overscroll_delta, dpi_scale_),
      gfx::ScaleVector2d(params.current_fling_velocity, dpi_scale_),
      gfx::ScaleVector2d(params.causal_event_viewport_point.OffsetFromOrigin(),
                         dpi_scale_))) {
    SetNeedsAnimate();
  }
}

bool OverscrollControllerAndroid::Animate(base::TimeTicks current_time,
                                          cc::Layer* parent_layer) {
  DCHECK(parent_layer);
  if (!enabled_)
    return false;

  bool needs_animate = refresh_effect_.Animate(current_time, parent_layer);
  needs_animate |= glow_effect_.Animate(current_time, parent_layer);
  return needs_animate;
}

void OverscrollControllerAndroid::OnFrameMetadataUpdated(
    const cc::CompositorFrameMetadata& frame_metadata) {
  const float scale_factor =
      frame_metadata.page_scale_factor * frame_metadata.device_scale_factor;
  gfx::SizeF viewport_size =
      gfx::ScaleSize(frame_metadata.scrollable_viewport_size, scale_factor);
  gfx::SizeF content_size =
      gfx::ScaleSize(frame_metadata.root_layer_size, scale_factor);
  gfx::Vector2dF content_scroll_offset =
      gfx::ScaleVector2d(frame_metadata.root_scroll_offset, scale_factor);

  refresh_effect_.UpdateDisplay(viewport_size, content_scroll_offset);
  glow_effect_.UpdateDisplay(viewport_size, content_size,
                             content_scroll_offset);
}

void OverscrollControllerAndroid::Enable() {
  enabled_ = true;
}

void OverscrollControllerAndroid::Disable() {
  if (!enabled_)
    return;
  enabled_ = false;
  if (!enabled_) {
    refresh_effect_.Reset();
    glow_effect_.Reset();
  }
}

void OverscrollControllerAndroid::DidNavigateMainFrame(
    const LoadCommittedDetails& details,
    const FrameNavigateParams& params) {
  // Once the main frame has navigated, there's little need to further animate
  // the reload effect. Note that the effect will naturally time out should the
  // reload be interruped for any reason.
  triggered_refresh_active_ = false;
}

void OverscrollControllerAndroid::TriggerRefresh() {
  triggered_refresh_active_ = false;
  if (!web_contents())
    return;

  triggered_refresh_active_ = true;
  web_contents()->ReloadFocusedFrame(false);
}

bool OverscrollControllerAndroid::IsStillRefreshing() const {
  return triggered_refresh_active_;
}

void OverscrollControllerAndroid::SetNeedsAnimate() {
  compositor_->SetNeedsAnimate();
}

}  // namespace content
