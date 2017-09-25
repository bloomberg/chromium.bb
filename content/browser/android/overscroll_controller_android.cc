// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/overscroll_controller_android.h"

#include "base/android/build_info.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "cc/layers/layer.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "content/common/content_switches_internal.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/platform/WebGestureEvent.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/android/edge_effect.h"
#include "ui/android/edge_effect_l.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/window_android.h"
#include "ui/android/window_android_compositor.h"
#include "ui/base/l10n/l10n_util_android.h"
#include "ui/events/blink/did_overscroll_params.h"

using ui::DidOverscrollParams;
using ui::EdgeEffect;
using ui::EdgeEffectBase;
using ui::EdgeEffectL;
using ui::OverscrollGlow;
using ui::OverscrollGlowClient;
using ui::OverscrollRefresh;

namespace content {
namespace {

// Used for conditional creation of EdgeEffect types for the overscroll glow.
const int kAndroidLSDKVersion = 21;

// If the glow effect alpha is greater than this value, the refresh effect will
// be suppressed. This value was experimentally determined to provide a
// reasonable balance between avoiding accidental refresh activation and
// minimizing the wait required to refresh after the glow has been triggered.
const float kMinGlowAlphaToDisableRefreshOnL = 0.085f;

bool IsAndroidLOrNewer() {
  static bool android_l_or_newer =
      base::android::BuildInfo::GetInstance()->sdk_int() >= kAndroidLSDKVersion;
  return android_l_or_newer;
}

// Suppressing refresh detection when the glow is still animating prevents
// visual confusion and accidental activation after repeated scrolls.
float MinGlowAlphaToDisableRefresh() {
  // Only the L effect is guaranteed to be both sufficiently brief and prominent
  // to provide a meaningful "wait" signal. The refresh effect on previous
  // Android releases can be quite faint, depending on the OEM-supplied
  // overscroll resources, and lasts nearly twice as long.
  if (IsAndroidLOrNewer())
    return kMinGlowAlphaToDisableRefreshOnL;

  // Any value greater than 1 effectively prevents the glow effect from ever
  // suppressing the refresh effect.
  return 1.01f;
}

std::unique_ptr<EdgeEffectBase> CreateGlowEdgeEffect(
    ui::ResourceManager* resource_manager,
    float dpi_scale) {
  DCHECK(resource_manager);
  if (IsAndroidLOrNewer())
    return std::unique_ptr<EdgeEffectBase>(new EdgeEffectL(resource_manager));

  return std::unique_ptr<EdgeEffectBase>(
      new EdgeEffect(resource_manager, dpi_scale));
}

std::unique_ptr<OverscrollGlow> CreateGlowEffect(OverscrollGlowClient* client,
                                                 float dpi_scale) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableOverscrollEdgeEffect)) {
    return nullptr;
  }

  return base::MakeUnique<OverscrollGlow>(client);
}

std::unique_ptr<OverscrollRefresh> CreateRefreshEffect(
    ui::OverscrollRefreshHandler* overscroll_refresh_handler) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePullToRefreshEffect)) {
    return nullptr;
  }

  return base::MakeUnique<OverscrollRefresh>(overscroll_refresh_handler);
}

}  // namespace

// static
std::unique_ptr<OverscrollControllerAndroid>
OverscrollControllerAndroid::CreateForTests(
    ui::WindowAndroidCompositor* compositor,
    float dpi_scale,
    std::unique_ptr<ui::OverscrollGlow> glow_effect,
    std::unique_ptr<ui::OverscrollRefresh> refresh_effect) {
  return std::unique_ptr<OverscrollControllerAndroid>(
      new OverscrollControllerAndroid(compositor, dpi_scale,
                                      std::move(glow_effect),
                                      std::move(refresh_effect)));
}

OverscrollControllerAndroid::OverscrollControllerAndroid(
    ui::WindowAndroidCompositor* compositor,
    float dpi_scale,
    std::unique_ptr<ui::OverscrollGlow> glow_effect,
    std::unique_ptr<ui::OverscrollRefresh> refresh_effect)
    : compositor_(compositor),
      dpi_scale_(dpi_scale),
      enabled_(true),
      glow_effect_(std::move(glow_effect)),
      refresh_effect_(std::move(refresh_effect)) {}

OverscrollControllerAndroid::OverscrollControllerAndroid(
    ui::OverscrollRefreshHandler* overscroll_refresh_handler,
    ui::WindowAndroidCompositor* compositor,
    float dpi_scale)
    : compositor_(compositor),
      dpi_scale_(dpi_scale),
      enabled_(true),
      glow_effect_(CreateGlowEffect(this, dpi_scale_)),
      refresh_effect_(CreateRefreshEffect(overscroll_refresh_handler)) {
  DCHECK(compositor_);
}

OverscrollControllerAndroid::~OverscrollControllerAndroid() {
}

bool OverscrollControllerAndroid::WillHandleGestureEvent(
    const blink::WebGestureEvent& event) {
  if (!enabled_)
    return false;

  if (!refresh_effect_)
    return false;

  // Suppress refresh detection if the glow effect is still prominent.
  if (glow_effect_ && glow_effect_->IsActive()) {
    if (glow_effect_->GetVisibleAlpha() > MinGlowAlphaToDisableRefresh())
      return false;
  }

  bool handled = false;
  switch (event.GetType()) {
    case blink::WebInputEvent::kGestureScrollBegin:
      refresh_effect_->OnScrollBegin();
      break;

    case blink::WebInputEvent::kGestureScrollUpdate: {
      gfx::Vector2dF scroll_delta(event.data.scroll_update.delta_x,
                                  event.data.scroll_update.delta_y);
      scroll_delta.Scale(dpi_scale_);
      handled = refresh_effect_->WillHandleScrollUpdate(scroll_delta);
    } break;

    case blink::WebInputEvent::kGestureScrollEnd:
      refresh_effect_->OnScrollEnd(gfx::Vector2dF());
      break;

    case blink::WebInputEvent::kGestureFlingStart: {
      if (refresh_effect_->IsActive()) {
        gfx::Vector2dF scroll_velocity(event.data.fling_start.velocity_x,
                                       event.data.fling_start.velocity_y);
        scroll_velocity.Scale(dpi_scale_);
        refresh_effect_->OnScrollEnd(scroll_velocity);
        // TODO(jdduke): Figure out a cleaner way of suppressing a fling.
        // It's important that the any downstream code sees a scroll-ending
        // event (in this case GestureFlingStart) if it has seen a scroll begin.
        // Thus, we cannot simply consume the fling. Changing the event type to
        // a GestureScrollEnd might work in practice, but could lead to
        // unexpected results. For now, simply truncate the fling velocity, but
        // not to zero as downstream code may not expect a zero-velocity fling.
        blink::WebGestureEvent& modified_event =
            const_cast<blink::WebGestureEvent&>(event);
        modified_event.data.fling_start.velocity_x = .01f;
        modified_event.data.fling_start.velocity_y = .01f;
      }
    } break;

    case blink::WebInputEvent::kGesturePinchBegin:
      refresh_effect_->ReleaseWithoutActivation();
      break;

    default:
      break;
  }

  return handled;
}

void OverscrollControllerAndroid::OnGestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  if (!enabled_)
    return;

  // The overscroll effect requires an explicit release signal that may not be
  // sent from the renderer compositor.
  if (event.GetType() == blink::WebInputEvent::kGestureScrollEnd ||
      event.GetType() == blink::WebInputEvent::kGestureFlingStart) {
    OnOverscrolled(DidOverscrollParams());
  }

  if (event.GetType() == blink::WebInputEvent::kGestureScrollUpdate &&
      refresh_effect_) {
    // The effect should only be allowed if both the causal touch events go
    // unconsumed and the generated scroll events go unconsumed.
    if (refresh_effect_->IsAwaitingScrollUpdateAck() &&
        (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED ||
         event.data.scroll_update.previous_update_in_sequence_prevented)) {
      refresh_effect_->Reset();
    }
  }
}

void OverscrollControllerAndroid::OnOverscrolled(
    const DidOverscrollParams& params) {
  if (!enabled_)
    return;

  if (refresh_effect_) {
    if (params.scroll_boundary_behavior.y !=
        cc::ScrollBoundaryBehavior::ScrollBoundaryBehaviorType::
            kScrollBoundaryBehaviorTypeAuto)
      refresh_effect_->Reset();
    else
      refresh_effect_->OnOverscrolled();

    if (refresh_effect_->IsActive() ||
        refresh_effect_->IsAwaitingScrollUpdateAck()) {
      // An active (or potentially active) refresh effect should always pre-empt
      // the passive glow effect.
      return;
    }
  }

  // When use-zoom-for-dsf is enabled, each value of params was already scaled
  // by the device scale factor.
  float scale_factor = IsUseZoomForDSFEnabled() ? 1.f : dpi_scale_;
  gfx::Vector2dF accumulated_overscroll =
      gfx::ScaleVector2d(params.accumulated_overscroll, scale_factor);
  gfx::Vector2dF latest_overscroll_delta =
      gfx::ScaleVector2d(params.latest_overscroll_delta, scale_factor);
  gfx::Vector2dF current_fling_velocity =
      gfx::ScaleVector2d(params.current_fling_velocity, scale_factor);
  gfx::Vector2dF overscroll_location = gfx::ScaleVector2d(
      params.causal_event_viewport_point.OffsetFromOrigin(), scale_factor);

  if (params.scroll_boundary_behavior.x ==
      cc::ScrollBoundaryBehavior::ScrollBoundaryBehaviorType::
          kScrollBoundaryBehaviorTypeNone) {
    accumulated_overscroll.set_x(0);
    latest_overscroll_delta.set_x(0);
    current_fling_velocity.set_x(0);
  }

  if (params.scroll_boundary_behavior.y ==
      cc::ScrollBoundaryBehavior::ScrollBoundaryBehaviorType::
          kScrollBoundaryBehaviorTypeNone) {
    accumulated_overscroll.set_y(0);
    latest_overscroll_delta.set_y(0);
    current_fling_velocity.set_y(0);
  }

  if (glow_effect_ && glow_effect_->OnOverscrolled(
                          base::TimeTicks::Now(), accumulated_overscroll,
                          latest_overscroll_delta, current_fling_velocity,
                          overscroll_location)) {
    SetNeedsAnimate();
  }
}

bool OverscrollControllerAndroid::Animate(base::TimeTicks current_time,
                                          cc::Layer* parent_layer) {
  DCHECK(parent_layer);
  if (!enabled_ || !glow_effect_)
    return false;

  return glow_effect_->Animate(current_time, parent_layer);
}

void OverscrollControllerAndroid::OnFrameMetadataUpdated(
    const viz::CompositorFrameMetadata& frame_metadata) {
  if (!refresh_effect_ && !glow_effect_)
    return;

  // When use-zoom-for-dsf is enabled, frame_metadata.page_scale_factor was
  // already scaled by the device scale factor.
  float scale_factor = frame_metadata.page_scale_factor;
  if (!IsUseZoomForDSFEnabled()) {
    scale_factor *= frame_metadata.device_scale_factor;
  }
  gfx::SizeF viewport_size =
      gfx::ScaleSize(frame_metadata.scrollable_viewport_size, scale_factor);
  gfx::SizeF content_size =
      gfx::ScaleSize(frame_metadata.root_layer_size, scale_factor);
  gfx::Vector2dF content_scroll_offset =
      gfx::ScaleVector2d(frame_metadata.root_scroll_offset, scale_factor);

  if (refresh_effect_) {
    refresh_effect_->OnFrameUpdated(content_scroll_offset,
                                    frame_metadata.root_overflow_y_hidden);
  }

  if (glow_effect_) {
    glow_effect_->OnFrameUpdated(viewport_size, content_size,
                                 content_scroll_offset);
  }
}

void OverscrollControllerAndroid::Enable() {
  enabled_ = true;
}

void OverscrollControllerAndroid::Disable() {
  if (!enabled_)
    return;
  enabled_ = false;
  if (!enabled_) {
    if (refresh_effect_)
      refresh_effect_->Reset();
    if (glow_effect_)
      glow_effect_->Reset();
  }
}

std::unique_ptr<EdgeEffectBase>
OverscrollControllerAndroid::CreateEdgeEffect() {
  return CreateGlowEdgeEffect(&compositor_->GetResourceManager(), dpi_scale_);
}

void OverscrollControllerAndroid::SetNeedsAnimate() {
  compositor_->SetNeedsAnimate();
}

}  // namespace content
