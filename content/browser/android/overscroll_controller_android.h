// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_OVERSCROLL_CONTROLLER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_OVERSCROLL_CONTROLLER_ANDROID_H_

#include "base/time/time.h"
#include "content/browser/android/overscroll_glow.h"
#include "content/browser/android/overscroll_refresh.h"
#include "content/common/input/input_event_ack_state.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace blink {
class WebGestureEvent;
}

namespace cc {
class CompositorFrameMetadata;
class Layer;
}

namespace ui {
class WindowAndroidCompositor;
}

namespace content {

struct DidOverscrollParams;

// Glue class for handling all inputs into Android-specific overscroll effects,
// both the passive overscroll glow and the active overscroll pull-to-refresh.
// Note that all input coordinates (both for events and overscroll) are in DIPs.
class OverscrollControllerAndroid : public OverscrollGlowClient,
                                    public OverscrollRefreshClient,
                                    public WebContentsObserver {
 public:
  OverscrollControllerAndroid(WebContents* web_contents,
                              ui::WindowAndroidCompositor* compositor,
                              float dpi_scale);
  ~OverscrollControllerAndroid() override;

  // Returns true if |event| is consumed by an overscroll effect, in which
  // case it should cease propagation.
  bool WillHandleGestureEvent(const blink::WebGestureEvent& event);

  // To be called upon receipt of a gesture event ack.
  void OnGestureEventAck(const blink::WebGestureEvent& event,
                         InputEventAckState ack_result);

  // To be called upon receipt of an overscroll event.
  void OnOverscrolled(const DidOverscrollParams& overscroll_params);

  // Returns true if the effect still needs animation ticks.
  // Note: The effect will detach itself when no further animation is required.
  bool Animate(base::TimeTicks current_time, cc::Layer* parent_layer);

  // To be called whenever the content frame has been updated.
  void OnFrameMetadataUpdated(const cc::CompositorFrameMetadata& metadata);

  // Toggle activity of any overscroll effects. When disabled, events will be
  // ignored until the controller is re-enabled.
  void Enable();
  void Disable();

 private:
  // WebContentsObserver implementation.
  void DidNavigateMainFrame(const LoadCommittedDetails& details,
                            const FrameNavigateParams& params) override;
  void DidToggleFullscreenModeForTab(bool entered_fullscreen) override;

  // OverscrollRefreshClient implementation.
  void TriggerRefresh() override;
  bool IsStillRefreshing() const override;

  // OverscrollGlowClient implementation.
  scoped_ptr<EdgeEffectBase> CreateEdgeEffect() override;

  void SetNeedsAnimate();

  ui::WindowAndroidCompositor* compositor_;
  const float dpi_scale_;

  bool enabled_;

  // TODO(jdduke): Factor out a common API from the two overscroll effects.
  scoped_ptr<OverscrollGlow> glow_effect_;
  scoped_ptr<OverscrollRefresh> refresh_effect_;
  bool triggered_refresh_active_;
  bool is_fullscreen_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollControllerAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_OVERSCROLL_CONTROLLER_ANDROID_H_
