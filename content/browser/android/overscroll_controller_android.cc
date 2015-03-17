// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/overscroll_controller_android.h"

#include "base/android/build_info.h"
#include "base/command_line.h"
#include "cc/layers/layer.h"
#include "cc/output/compositor_frame_metadata.h"
#include "content/browser/android/edge_effect.h"
#include "content/browser/android/edge_effect_l.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/did_overscroll_params.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/window_android_compositor.h"
#include "ui/base/l10n/l10n_util_android.h"

namespace content {
namespace {

// Used for conditional creation of EdgeEffect types for the overscroll glow.
const int kAndroidLSDKVersion = 21;

// Default offset in dips from the top of the view beyond which the refresh
// action will be activated.
const int kDefaultRefreshDragTargetDips = 64;

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

scoped_ptr<EdgeEffectBase> CreateGlowEdgeEffect(
    ui::ResourceManager* resource_manager,
    float dpi_scale) {
  DCHECK(resource_manager);
  if (IsAndroidLOrNewer())
    return scoped_ptr<EdgeEffectBase>(new EdgeEffectL(resource_manager));

  return scoped_ptr<EdgeEffectBase>(
      new EdgeEffect(resource_manager, dpi_scale));
}

scoped_ptr<OverscrollGlow> CreateGlowEffect(OverscrollGlowClient* client,
                                            float dpi_scale) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableOverscrollEdgeEffect)) {
    return nullptr;
  }

  return make_scoped_ptr(new OverscrollGlow(client));
}

scoped_ptr<OverscrollRefresh> CreateRefreshEffect(
    ui::WindowAndroidCompositor* compositor,
    OverscrollRefreshClient* client,
    float dpi_scale) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisablePullToRefreshEffect)) {
    return nullptr;
  }

  return make_scoped_ptr(new OverscrollRefresh(
      &compositor->GetResourceManager(), client,
      kDefaultRefreshDragTargetDips * dpi_scale, l10n_util::IsLayoutRtl()));
}

}  // namespace

OverscrollControllerAndroid::OverscrollControllerAndroid(
    WebContents* web_contents,
    ui::WindowAndroidCompositor* compositor,
    float dpi_scale)
    : compositor_(compositor),
      dpi_scale_(dpi_scale),
      enabled_(true),
      glow_effect_(CreateGlowEffect(this, dpi_scale)),
      refresh_effect_(CreateRefreshEffect(compositor, this, dpi_scale)),
      triggered_refresh_active_(false),
      is_fullscreen_(static_cast<WebContentsImpl*>(web_contents)
                         ->IsFullscreenForCurrentTab()) {
  DCHECK(web_contents);
  DCHECK(compositor);
  if (refresh_effect_)
    Observe(web_contents);
}

OverscrollControllerAndroid::~OverscrollControllerAndroid() {
}

bool OverscrollControllerAndroid::WillHandleGestureEvent(
    const blink::WebGestureEvent& event) {
  if (!enabled_)
    return false;

  if (!refresh_effect_)
    return false;

  // Suppress refresh detection for fullscreen HTML5 scenarios, e.g., video.
  if (is_fullscreen_)
    return false;

  // Suppress refresh detection if the glow effect is still prominent.
  if (glow_effect_ && glow_effect_->IsActive()) {
    if (glow_effect_->GetVisibleAlpha() > MinGlowAlphaToDisableRefresh())
      return false;
  }

  bool handled = false;
  bool maybe_needs_animate = false;
  switch (event.type) {
    case blink::WebInputEvent::GestureScrollBegin:
      refresh_effect_->OnScrollBegin();
      break;

    case blink::WebInputEvent::GestureScrollUpdate: {
      gfx::Vector2dF scroll_delta(event.data.scrollUpdate.deltaX,
                                  event.data.scrollUpdate.deltaY);
      scroll_delta.Scale(dpi_scale_);
      maybe_needs_animate = true;
      handled = refresh_effect_->WillHandleScrollUpdate(scroll_delta);
    } break;

    case blink::WebInputEvent::GestureScrollEnd:
      refresh_effect_->OnScrollEnd(gfx::Vector2dF());
      maybe_needs_animate = true;
      break;

    case blink::WebInputEvent::GestureFlingStart: {
      gfx::Vector2dF scroll_velocity(event.data.flingStart.velocityX,
                                     event.data.flingStart.velocityY);
      scroll_velocity.Scale(dpi_scale_);
      refresh_effect_->OnScrollEnd(scroll_velocity);
      if (refresh_effect_->IsActive()) {
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

    case blink::WebInputEvent::GesturePinchBegin:
      refresh_effect_->ReleaseWithoutActivation();
      break;

    default:
      break;
  }

  if (maybe_needs_animate && refresh_effect_->IsActive())
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

  if (event.type == blink::WebInputEvent::GestureScrollUpdate &&
      refresh_effect_) {
    // The effect should only be allowed if both the causal touch events go
    // unconsumed and the generated scroll events go unconsumed.
    bool consumed = ack_result == INPUT_EVENT_ACK_STATE_CONSUMED ||
                    event.data.scrollUpdate.previousUpdateInSequencePrevented;
    refresh_effect_->OnScrollUpdateAck(consumed);
  }
}

void OverscrollControllerAndroid::OnOverscrolled(
    const DidOverscrollParams& params) {
  if (!enabled_)
    return;

  if (refresh_effect_ && (refresh_effect_->IsActive() ||
                          refresh_effect_->IsAwaitingScrollUpdateAck())) {
    // An active (or potentially active) refresh effect should always pre-empt
    // the passive glow effect.
    return;
  }

  if (glow_effect_ &&
      glow_effect_->OnOverscrolled(
          base::TimeTicks::Now(),
          gfx::ScaleVector2d(params.accumulated_overscroll, dpi_scale_),
          gfx::ScaleVector2d(params.latest_overscroll_delta, dpi_scale_),
          gfx::ScaleVector2d(params.current_fling_velocity, dpi_scale_),
          gfx::ScaleVector2d(
              params.causal_event_viewport_point.OffsetFromOrigin(),
              dpi_scale_))) {
    SetNeedsAnimate();
  }
}

bool OverscrollControllerAndroid::Animate(base::TimeTicks current_time,
                                          cc::Layer* parent_layer) {
  DCHECK(parent_layer);
  if (!enabled_)
    return false;

  bool needs_animate = false;
  if (refresh_effect_)
    needs_animate |= refresh_effect_->Animate(current_time, parent_layer);
  if (glow_effect_)
    needs_animate |= glow_effect_->Animate(current_time, parent_layer);
  return needs_animate;
}

void OverscrollControllerAndroid::OnFrameMetadataUpdated(
    const cc::CompositorFrameMetadata& frame_metadata) {
  if (!refresh_effect_ && !glow_effect_)
    return;

  const float scale_factor =
      frame_metadata.page_scale_factor * frame_metadata.device_scale_factor;
  gfx::SizeF viewport_size =
      gfx::ScaleSize(frame_metadata.scrollable_viewport_size, scale_factor);
  gfx::SizeF content_size =
      gfx::ScaleSize(frame_metadata.root_layer_size, scale_factor);
  gfx::Vector2dF content_scroll_offset =
      gfx::ScaleVector2d(frame_metadata.root_scroll_offset, scale_factor);

  if (refresh_effect_) {
    refresh_effect_->UpdateDisplay(viewport_size, content_scroll_offset,
                                   frame_metadata.root_overflow_y_hidden);
  }

  if (glow_effect_) {
    glow_effect_->UpdateDisplay(viewport_size, content_size,
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

void OverscrollControllerAndroid::DidNavigateMainFrame(
    const LoadCommittedDetails& details,
    const FrameNavigateParams& params) {
  // Once the main frame has navigated, there's little need to further animate
  // the reload effect. Note that the effect will naturally time out should the
  // reload be interruped for any reason.
  triggered_refresh_active_ = false;
}

void OverscrollControllerAndroid::DidToggleFullscreenModeForTab(
    bool entered_fullscreen) {
  if (is_fullscreen_ == entered_fullscreen)
    return;
  is_fullscreen_ = entered_fullscreen;
  if (is_fullscreen_)
    refresh_effect_->ReleaseWithoutActivation();
}

void OverscrollControllerAndroid::TriggerRefresh() {
  triggered_refresh_active_ = false;
  if (!web_contents())
    return;

  triggered_refresh_active_ = true;
  RecordAction(base::UserMetricsAction("MobilePullGestureReload"));
  web_contents()->GetController().Reload(true);
}

bool OverscrollControllerAndroid::IsStillRefreshing() const {
  return triggered_refresh_active_;
}

scoped_ptr<EdgeEffectBase> OverscrollControllerAndroid::CreateEdgeEffect() {
  return CreateGlowEdgeEffect(&compositor_->GetResourceManager(), dpi_scale_);
}

void OverscrollControllerAndroid::SetNeedsAnimate() {
  compositor_->SetNeedsAnimate();
}

}  // namespace content
