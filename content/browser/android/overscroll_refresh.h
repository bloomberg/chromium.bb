// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_OVERSCROLL_REFRESH_H_
#define CONTENT_BROWSER_ANDROID_OVERSCROLL_REFRESH_H_

#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {
class Layer;
}

namespace ui {
class ResourceManager;
}

namespace content {

// Allows both page reload activation and page reloading state queries.
class CONTENT_EXPORT OverscrollRefreshClient {
 public:
  virtual ~OverscrollRefreshClient() {}

  // Called when the effect is released beyond the activation threshold. This
  // should cause a refresh of some kind, e.g., page reload.
  virtual void TriggerRefresh() = 0;

  // Whether the triggered refresh has yet to complete. The effect will continue
  // animating until the refresh completes (or it reaches a reasonable timeout).
  virtual bool IsStillRefreshing() const = 0;
};

// Simple pull-to-refresh styled effect. Listens to scroll events, conditionally
// activating when:
//   1) The scroll begins when the page's root layer 1) has no vertical scroll
//      offset and 2) lacks the overflow-y:hidden property.
//   2) The page doesn't consume the initial scroll events.
//   3) The initial scroll direction is upward.
// The actual page reload action is triggered only when the effect is active
// and beyond a particular threshold when released.
class CONTENT_EXPORT OverscrollRefresh {
 public:
  // Minmum number of overscrolling pull events required to activate the effect.
  // Useful for avoiding accidental triggering when a scroll janks (is delayed),
  // capping the impulse per event.
  enum { kMinPullsToActivate = 3 };

  // Both |resource_manager| and |client| must not be null.
  // |target_drag_offset_pixels| is the threshold beyond which the effect
  // will trigger a refresh action when released. When |mirror| is true,
  // the effect and its rotation will be mirrored about the y axis.
  OverscrollRefresh(ui::ResourceManager* resource_manager,
                    OverscrollRefreshClient* client,
                    float target_drag_offset_pixels,
                    bool mirror);
  ~OverscrollRefresh();

  // Scroll event stream listening methods.
  void OnScrollBegin();
  void OnScrollEnd(const gfx::Vector2dF& velocity);

  // Scroll ack listener. The effect will only be activated if the initial
  // updates go unconsumed.
  void OnScrollUpdateAck(bool was_consumed);

  // Returns true if the effect has consumed the |scroll_delta|.
  bool WillHandleScrollUpdate(const gfx::Vector2dF& scroll_delta);

  // Release the effect (if active), preventing any associated refresh action.
  void ReleaseWithoutActivation();

  // Returns true if the effect still needs animation ticks, with effect layers
  // attached to |parent| if necessary.
  // Note: The effect will detach itself when no further animation is required.
  bool Animate(base::TimeTicks current_time, cc::Layer* parent_layer);

  // Update the effect according to the most recent display parameters,
  // Note: All dimensions are in device pixels.
  void UpdateDisplay(const gfx::SizeF& viewport_size,
                     const gfx::Vector2dF& content_scroll_offset,
                     bool root_overflow_y_hidden);

  // Reset the effect to its inactive state, immediately detaching and
  // disabling any active effects.
  void Reset();

  // Returns true if the refresh effect is either being manipulated or animated.
  bool IsActive() const;

  // Returns true if the effect is waiting for an unconsumed scroll to start.
  bool IsAwaitingScrollUpdateAck() const;

 private:
  void Release(bool allow_activation);

  OverscrollRefreshClient* const client_;

  gfx::SizeF viewport_size_;
  bool scrolled_to_top_;
  bool overflow_y_hidden_;

  enum ScrollConsumptionState {
    DISABLED,
    AWAITING_SCROLL_UPDATE_ACK,
    ENABLED,
  } scroll_consumption_state_;

  class Effect;
  scoped_ptr<Effect> effect_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollRefresh);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_OVERSCROLL_REFRESH_H_
