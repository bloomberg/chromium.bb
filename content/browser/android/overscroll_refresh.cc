// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/overscroll_refresh.h"

#include "cc/animation/timing_function.h"
#include "cc/layers/ui_resource_layer.h"
#include "cc/trees/layer_tree_host.h"
#include "content/browser/android/animation_utils.h"
#include "ui/android/resources/resource_manager.h"
#include "ui/android/resources/system_ui_resource_type.h"
#include "ui/gfx/geometry/size_conversions.h"

using std::abs;
using std::max;
using std::min;

namespace content {
namespace {

const ui::SystemUIResourceType kIdleResourceId = ui::OVERSCROLL_REFRESH_IDLE;
const ui::SystemUIResourceType kActiveResourceId =
    ui::OVERSCROLL_REFRESH_ACTIVE;

// Drag movement multiplier between user input and effect translation.
const float kDragRate = .5f;

// Animation duration after the effect is released without triggering a refresh.
const int kRecedeTimeMs = 300;

// Animation duration immediately after the effect is released and activated.
const int kActivationStartTimeMs = 150;

// Animation duration after the effect is released and triggers a refresh.
const int kActivationTimeMs = 850;

// Max animation duration after the effect is released and triggers a refresh.
const int kMaxActivationTimeMs = kActivationTimeMs * 4;

// Animation duration after the refresh activated phase has completed.
const int kActivationRecedeTimeMs = 250;

// Input threshold required to start glowing.
const float kGlowActivationThreshold = 0.85f;

// Minimum alpha for the effect layer.
const float kMinAlpha = 0.3f;

// Experimentally determined constant used to allow activation even if touch
// release results in a small upward fling (quite common during a slow scroll).
const float kMinFlingVelocityForActivation = -500.f;

const float kEpsilon = 0.005f;

void UpdateLayer(cc::UIResourceLayer* layer,
                 cc::Layer* parent,
                 cc::UIResourceId res_id,
                 const gfx::Size& size,
                 const gfx::Vector2dF& offset,
                 float opacity,
                 float rotation,
                 bool mirror) {
  DCHECK(layer);
  DCHECK(parent);
  DCHECK(parent->layer_tree_host());
  if (layer->parent() != parent)
    parent->AddChild(layer);

  if (size.IsEmpty()) {
    layer->SetIsDrawable(false);
    return;
  }

  if (!res_id) {
    layer->SetIsDrawable(false);
    return;
  }

  if (opacity == 0) {
    layer->SetIsDrawable(false);
    layer->SetOpacity(0);
    return;
  }

  layer->SetUIResourceId(res_id);
  layer->SetIsDrawable(true);
  layer->SetTransformOrigin(
      gfx::Point3F(size.width() * 0.5f, size.height() * 0.5f, 0));
  layer->SetBounds(size);
  layer->SetContentsOpaque(false);
  layer->SetOpacity(Clamp(opacity, 0.f, 1.f));

  float offset_x = offset.x() - size.width() * 0.5f;
  float offset_y = offset.y() - size.height() * 0.5f;
  gfx::Transform transform;
  transform.Translate(offset_x, offset_y);
  if (mirror)
    transform.Scale(-1.f, 1.f);
  transform.Rotate(rotation);
  layer->SetTransform(transform);
}

}  // namespace

class OverscrollRefresh::Effect {
 public:
  Effect(ui::ResourceManager* resource_manager, float target_drag, bool mirror)
      : resource_manager_(resource_manager),
        idle_layer_(cc::UIResourceLayer::Create()),
        active_layer_(cc::UIResourceLayer::Create()),
        target_drag_(target_drag),
        mirror_(mirror),
        drag_(0),
        idle_alpha_(0),
        active_alpha_(0),
        offset_(0),
        rotation_(0),
        size_scale_(1),
        idle_alpha_start_(0),
        idle_alpha_finish_(0),
        active_alpha_start_(0),
        active_alpha_finish_(0),
        offset_start_(0),
        offset_finish_(0),
        rotation_start_(0),
        rotation_finish_(0),
        size_scale_start_(1),
        size_scale_finish_(1),
        state_(STATE_IDLE),
        ease_out_(cc::EaseOutTimingFunction::Create()),
        ease_in_out_(cc::EaseInOutTimingFunction::Create()) {
    DCHECK(target_drag_);
    idle_layer_->SetIsDrawable(false);
    active_layer_->SetIsDrawable(false);
  }

  ~Effect() { Detach(); }

  void Pull(float delta) {
    if (state_ != STATE_PULL)
      drag_ = 0;

    state_ = STATE_PULL;

    delta *= kDragRate;
    float max_delta = target_drag_ / OverscrollRefresh::kMinPullsToActivate;
    delta = Clamp(delta, -max_delta, max_delta);

    drag_ += delta;
    drag_ = Clamp(drag_, 0.f, target_drag_ * 3.f);

    // The following logic and constants were taken from Android's refresh
    // effect (see SwipeRefreshLayout.java from v4 of the AppCompat library).
    float original_drag_percent = drag_ / target_drag_;
    float drag_percent = min(1.f, abs(original_drag_percent));
    float adjusted_percent = max(drag_percent - .4f, 0.f) * 5.f / 3.f;
    float extra_os = abs(drag_) - target_drag_;
    float slingshot_dist = target_drag_;
    float tension_slingshot_percent =
        max(0.f, min(extra_os, slingshot_dist * 2) / slingshot_dist);
    float tension_percent = ((tension_slingshot_percent / 4) -
                             std::pow((tension_slingshot_percent / 4), 2.f)) *
                            2.f;
    float extra_move = slingshot_dist * tension_percent * 2;

    offset_ = slingshot_dist * drag_percent + extra_move;

    rotation_ =
        360.f * ((-0.25f + .4f * adjusted_percent + tension_percent * 2) * .5f);

    idle_alpha_ =
        kMinAlpha + (1.f - kMinAlpha) * drag_percent / kGlowActivationThreshold;
    active_alpha_ = (drag_percent - kGlowActivationThreshold) /
                    (1.f - kGlowActivationThreshold);
    idle_alpha_ = Clamp(idle_alpha_, 0.f, 1.f);
    active_alpha_ = Clamp(active_alpha_, 0.f, 1.f);

    size_scale_ = 1;
  }

  bool Animate(base::TimeTicks current_time, bool still_refreshing) {
    if (IsFinished())
      return false;

    if (state_ == STATE_PULL)
      return true;

    const double dt = (current_time - start_time_).InMilliseconds();
    const double t = dt / duration_.InMilliseconds();
    const float interp = ease_out_->GetValue(min(t, 1.));

    idle_alpha_ = Lerp(idle_alpha_start_, idle_alpha_finish_, interp);
    active_alpha_ = Lerp(active_alpha_start_, active_alpha_finish_, interp);
    offset_ = Lerp(offset_start_, offset_finish_, interp);
    size_scale_ = Lerp(size_scale_start_, size_scale_finish_, interp);

    if (state_ == STATE_ACTIVATED || state_ == STATE_ACTIVATED_RECEDE) {
      float adjusted_interp = ease_in_out_->GetValue(min(t, 1.));
      rotation_ = Lerp(rotation_start_, rotation_finish_, adjusted_interp);
      // Add a small constant rotational velocity during activation.
      rotation_ += dt * 90.f / kActivationTimeMs;
    } else {
      rotation_ = Lerp(rotation_start_, rotation_finish_, interp);
    }

    if (t < 1.f - kEpsilon)
      return true;

    switch (state_) {
      case STATE_IDLE:
      case STATE_PULL:
        NOTREACHED() << "Invalidate state for animation.";
        break;
      case STATE_ACTIVATED_START:
        // Briefly pause the animation after the rapid initial translation.
        if (t < 1.5f)
          break;
        state_ = STATE_ACTIVATED;
        start_time_ = current_time;
        duration_ = base::TimeDelta::FromMilliseconds(kActivationTimeMs);
        activated_start_time_ = current_time;
        offset_start_ = offset_finish_ = offset_;
        rotation_start_ = rotation_;
        rotation_finish_ = rotation_start_ + 270.f;
        size_scale_start_ = size_scale_finish_ = size_scale_;
        break;
      case STATE_ACTIVATED:
        start_time_ = current_time;
        if (still_refreshing &&
            (current_time - activated_start_time_ <
             base::TimeDelta::FromMilliseconds(kMaxActivationTimeMs))) {
          offset_start_ = offset_finish_ = offset_;
          rotation_start_ = rotation_;
          rotation_finish_ = rotation_start_ + 270.f;
          break;
        }
        state_ = STATE_ACTIVATED_RECEDE;
        duration_ = base::TimeDelta::FromMilliseconds(kActivationRecedeTimeMs);
        rotation_start_ = rotation_finish_ = rotation_;
        offset_start_ = offset_finish_ = offset_;
        size_scale_start_ = size_scale_;
        size_scale_finish_ = 0;
        break;
      case STATE_ACTIVATED_RECEDE:
        Finish();
        break;
      case STATE_RECEDE:
        Finish();
        break;
    };

    return !IsFinished();
  }

  bool Release(base::TimeTicks current_time, bool allow_activation) {
    switch (state_) {
      case STATE_PULL:
        break;

      case STATE_ACTIVATED:
      case STATE_ACTIVATED_START:
        // Avoid redundant activations.
        if (allow_activation)
          return false;
        break;

      case STATE_IDLE:
      case STATE_ACTIVATED_RECEDE:
      case STATE_RECEDE:
        // These states have already been "released" in some fashion.
        return false;
    }

    start_time_ = current_time;
    idle_alpha_start_ = idle_alpha_;
    active_alpha_start_ = active_alpha_;
    offset_start_ = offset_;
    rotation_start_ = rotation_;
    size_scale_start_ = size_scale_finish_ = size_scale_;

    if (drag_ < target_drag_ || !allow_activation) {
      state_ = STATE_RECEDE;
      duration_ = base::TimeDelta::FromMilliseconds(kRecedeTimeMs);
      idle_alpha_finish_ = 0;
      active_alpha_finish_ = 0;
      offset_finish_ = 0;
      rotation_finish_ = rotation_start_ - 180.f;
      return false;
    }

    state_ = STATE_ACTIVATED_START;
    duration_ = base::TimeDelta::FromMilliseconds(kActivationStartTimeMs);
    activated_start_time_ = current_time;
    idle_alpha_finish_ = idle_alpha_start_;
    active_alpha_finish_ = active_alpha_start_;
    offset_finish_ = target_drag_;
    rotation_finish_ = rotation_start_;
    return true;
  }

  void Finish() {
    Detach();
    idle_layer_->SetIsDrawable(false);
    active_layer_->SetIsDrawable(false);
    offset_ = 0;
    idle_alpha_ = 0;
    active_alpha_ = 0;
    rotation_ = 0;
    size_scale_ = 1;
    state_ = STATE_IDLE;
  }

  void ApplyToLayers(const gfx::SizeF& viewport_size, cc::Layer* parent) {
    if (IsFinished())
      return;

    if (!parent->layer_tree_host())
      return;

    // An empty window size, while meaningless, is also relatively harmless, and
    // will simply prevent any drawing of the layers.
    if (viewport_size.IsEmpty()) {
      idle_layer_->SetIsDrawable(false);
      active_layer_->SetIsDrawable(false);
      return;
    }

    cc::UIResourceId idle_resource = resource_manager_->GetUIResourceId(
        ui::ANDROID_RESOURCE_TYPE_SYSTEM, kIdleResourceId);
    cc::UIResourceId active_resource = resource_manager_->GetUIResourceId(
        ui::ANDROID_RESOURCE_TYPE_SYSTEM, kActiveResourceId);

    gfx::Size idle_size =
        parent->layer_tree_host()->GetUIResourceSize(idle_resource);
    gfx::Size active_size =
        parent->layer_tree_host()->GetUIResourceSize(active_resource);
    gfx::Size scaled_idle_size =
        gfx::ToRoundedSize(gfx::ScaleSize(idle_size, size_scale_));
    gfx::Size scaled_active_size =
        gfx::ToRoundedSize(gfx::ScaleSize(active_size, size_scale_));

    gfx::Vector2dF idle_offset(viewport_size.width() * 0.5f,
                               offset_ - idle_size.height() * 0.5f);
    gfx::Vector2dF active_offset(viewport_size.width() * 0.5f,
                                 offset_ - active_size.height() * 0.5f);

    UpdateLayer(idle_layer_.get(), parent, idle_resource, scaled_idle_size,
                idle_offset, idle_alpha_, rotation_, mirror_);
    UpdateLayer(active_layer_.get(), parent, active_resource,
                scaled_active_size, active_offset, active_alpha_, rotation_,
                mirror_);
  }

  bool IsFinished() const { return state_ == STATE_IDLE; }

 private:
  enum State {
    STATE_IDLE = 0,
    STATE_PULL,
    STATE_ACTIVATED_START,
    STATE_ACTIVATED,
    STATE_ACTIVATED_RECEDE,
    STATE_RECEDE
  };

  void Detach() {
    idle_layer_->RemoveFromParent();
    active_layer_->RemoveFromParent();
  }

  ui::ResourceManager* const resource_manager_;

  scoped_refptr<cc::UIResourceLayer> idle_layer_;
  scoped_refptr<cc::UIResourceLayer> active_layer_;

  const float target_drag_;
  const bool mirror_;
  float drag_;
  float idle_alpha_;
  float active_alpha_;
  float offset_;
  float rotation_;
  float size_scale_;

  float idle_alpha_start_;
  float idle_alpha_finish_;
  float active_alpha_start_;
  float active_alpha_finish_;
  float offset_start_;
  float offset_finish_;
  float rotation_start_;
  float rotation_finish_;
  float size_scale_start_;
  float size_scale_finish_;

  base::TimeTicks start_time_;
  base::TimeTicks activated_start_time_;
  base::TimeDelta duration_;

  State state_;

  scoped_ptr<cc::TimingFunction> ease_out_;
  scoped_ptr<cc::TimingFunction> ease_in_out_;

  DISALLOW_COPY_AND_ASSIGN(Effect);
};

OverscrollRefresh::OverscrollRefresh(ui::ResourceManager* resource_manager,
                                     OverscrollRefreshClient* client,
                                     float target_drag_offset_pixels,
                                     bool mirror)
    : client_(client),
      scrolled_to_top_(true),
      overflow_y_hidden_(false),
      scroll_consumption_state_(DISABLED),
      effect_(new Effect(resource_manager, target_drag_offset_pixels, mirror)) {
  DCHECK(client);
}

OverscrollRefresh::~OverscrollRefresh() {
}

void OverscrollRefresh::Reset() {
  scroll_consumption_state_ = DISABLED;
  effect_->Finish();
}

void OverscrollRefresh::OnScrollBegin() {
  ReleaseWithoutActivation();
  if (scrolled_to_top_ && !overflow_y_hidden_)
    scroll_consumption_state_ = AWAITING_SCROLL_UPDATE_ACK;
}

void OverscrollRefresh::OnScrollEnd(const gfx::Vector2dF& scroll_velocity) {
  bool allow_activation = scroll_velocity.y() > kMinFlingVelocityForActivation;
  Release(allow_activation);
}

void OverscrollRefresh::OnScrollUpdateAck(bool was_consumed) {
  if (scroll_consumption_state_ != AWAITING_SCROLL_UPDATE_ACK)
    return;

  scroll_consumption_state_ = was_consumed ? DISABLED : ENABLED;
}

bool OverscrollRefresh::WillHandleScrollUpdate(
    const gfx::Vector2dF& scroll_delta) {
  if (viewport_size_.IsEmpty())
    return false;

  switch (scroll_consumption_state_) {
    case DISABLED:
      return false;

    case AWAITING_SCROLL_UPDATE_ACK:
      // If the initial scroll motion is downward, never allow activation.
      if (scroll_delta.y() <= 0)
        scroll_consumption_state_ = DISABLED;
      return false;

    case ENABLED: {
      effect_->Pull(scroll_delta.y());
      return true;
    }
  }

  NOTREACHED() << "Invalid overscroll state: " << scroll_consumption_state_;
  return false;
}

void OverscrollRefresh::ReleaseWithoutActivation() {
  bool allow_activation = false;
  Release(allow_activation);
}

bool OverscrollRefresh::Animate(base::TimeTicks current_time,
                                cc::Layer* parent_layer) {
  DCHECK(parent_layer);
  if (effect_->IsFinished())
    return false;

  if (effect_->Animate(current_time, client_->IsStillRefreshing()))
    effect_->ApplyToLayers(viewport_size_, parent_layer);

  return !effect_->IsFinished();
}

bool OverscrollRefresh::IsActive() const {
  return scroll_consumption_state_ == ENABLED || !effect_->IsFinished();
}

bool OverscrollRefresh::IsAwaitingScrollUpdateAck() const {
  return scroll_consumption_state_ == AWAITING_SCROLL_UPDATE_ACK;
}

void OverscrollRefresh::UpdateDisplay(
    const gfx::SizeF& viewport_size,
    const gfx::Vector2dF& content_scroll_offset,
    bool root_overflow_y_hidden) {
  viewport_size_ = viewport_size;
  scrolled_to_top_ = content_scroll_offset.y() == 0;
  overflow_y_hidden_ = root_overflow_y_hidden;
}

void OverscrollRefresh::Release(bool allow_activation) {
  if (scroll_consumption_state_ == ENABLED) {
    if (effect_->Release(base::TimeTicks::Now(), allow_activation))
      client_->TriggerRefresh();
  }
  scroll_consumption_state_ = DISABLED;
}

}  // namespace content
