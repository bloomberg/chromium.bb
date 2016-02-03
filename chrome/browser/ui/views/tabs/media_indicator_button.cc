// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/media_indicator_button.h"

#include "base/macros.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "content/public/browser/user_metrics.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"

using base::UserMetricsAction;

namespace {

// The minimum required click-to-select area of an inactive Tab before allowing
// the click-to-mute functionality to be enabled.  These values are in terms of
// some percentage of the MediaIndicatorButton's width.  See comments in
// UpdateEnabledForMuteToggle().
const int kMinMouseSelectableAreaPercent = 250;
const int kMinGestureSelectableAreaPercent = 400;

// Returns true if either Shift or Control are being held down.  In this case,
// mouse events are delegated to the Tab, to perform tab selection in the tab
// strip instead.
bool IsShiftOrControlDown(const ui::Event& event) {
  return (event.flags() & (ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN)) != 0;
}

}  // namespace

const char MediaIndicatorButton::kViewClassName[] = "MediaIndicatorButton";

class MediaIndicatorButton::FadeAnimationDelegate
    : public gfx::AnimationDelegate {
 public:
  explicit FadeAnimationDelegate(MediaIndicatorButton* button)
      : button_(button) {}
  ~FadeAnimationDelegate() override {}

 private:
  // gfx::AnimationDelegate
  void AnimationProgressed(const gfx::Animation* animation) override {
    button_->SchedulePaint();
  }

  void AnimationCanceled(const gfx::Animation* animation) override {
    AnimationEnded(animation);
  }

  void AnimationEnded(const gfx::Animation* animation) override {
    button_->showing_media_state_ = button_->media_state_;
    button_->parent_tab_->MediaStateChanged();
  }

  MediaIndicatorButton* const button_;

  DISALLOW_COPY_AND_ASSIGN(FadeAnimationDelegate);
};

MediaIndicatorButton::MediaIndicatorButton(Tab* parent_tab)
    : views::ImageButton(NULL),
      parent_tab_(parent_tab),
      media_state_(TAB_MEDIA_STATE_NONE),
      showing_media_state_(TAB_MEDIA_STATE_NONE) {
  DCHECK(parent_tab_);
  SetEventTargeter(
      scoped_ptr<views::ViewTargeter>(new views::ViewTargeter(this)));
}

MediaIndicatorButton::~MediaIndicatorButton() {}

void MediaIndicatorButton::TransitionToMediaState(TabMediaState next_state) {
  if (next_state == media_state_)
    return;

  TabMediaState previous_media_showing_state = showing_media_state_;

  if (next_state != TAB_MEDIA_STATE_NONE)
    ResetImages(next_state);

  if ((media_state_ == TAB_MEDIA_STATE_AUDIO_PLAYING &&
       next_state == TAB_MEDIA_STATE_AUDIO_MUTING) ||
      (media_state_ == TAB_MEDIA_STATE_AUDIO_MUTING &&
       next_state == TAB_MEDIA_STATE_AUDIO_PLAYING) ||
      (media_state_ == TAB_MEDIA_STATE_AUDIO_MUTING &&
       next_state == TAB_MEDIA_STATE_NONE)) {
    // Instant user feedback: No fade animation.
    showing_media_state_ = next_state;
    fade_animation_.reset();
  } else {
    if (next_state == TAB_MEDIA_STATE_NONE)
      showing_media_state_ = media_state_;  // Fading-out indicator.
    else
      showing_media_state_ = next_state;  // Fading-in to next indicator.
    fade_animation_ = chrome::CreateTabMediaIndicatorFadeAnimation(next_state);
    if (!fade_animation_delegate_)
      fade_animation_delegate_.reset(new FadeAnimationDelegate(this));
    fade_animation_->set_delegate(fade_animation_delegate_.get());
    fade_animation_->Start();
  }

  media_state_ = next_state;

  if (previous_media_showing_state != showing_media_state_)
    parent_tab_->MediaStateChanged();

  UpdateEnabledForMuteToggle();

  // An indicator state change should be made visible immediately, instead of
  // the user being surprised when their mouse leaves the button.
  if (state() == views::CustomButton::STATE_HOVERED) {
    SetState(enabled() ? views::CustomButton::STATE_NORMAL :
             views::CustomButton::STATE_DISABLED);
  }

  // Note: The calls to SetImage(), SetEnabled(), and SetState() above will call
  // SchedulePaint() if necessary.
}

void MediaIndicatorButton::UpdateEnabledForMuteToggle() {
  bool enable = chrome::AreExperimentalMuteControlsEnabled() &&
      (media_state_ == TAB_MEDIA_STATE_AUDIO_PLAYING ||
       media_state_ == TAB_MEDIA_STATE_AUDIO_MUTING);

  // If the tab is not the currently-active tab, make sure it is wide enough
  // before enabling click-to-mute.  This ensures that there is enough click
  // area for the user to activate a tab rather than unintentionally muting it.
  // Note that IsTriggerableEvent() is also overridden to provide an even wider
  // requirement for tap gestures.
  if (enable && !GetTab()->IsActive()) {
    const int required_width = width() * kMinMouseSelectableAreaPercent / 100;
    enable = (GetTab()->GetWidthOfLargestSelectableRegion() >= required_width);
  }

  SetEnabled(enable);
}

void MediaIndicatorButton::OnParentTabButtonColorChanged() {
  if (media_state_ == TAB_MEDIA_STATE_AUDIO_PLAYING ||
      media_state_ == TAB_MEDIA_STATE_AUDIO_MUTING)
    ResetImages(media_state_);
}

const char* MediaIndicatorButton::GetClassName() const {
  return kViewClassName;
}

views::View* MediaIndicatorButton::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return NULL;  // Tab (the parent View) provides the tooltip.
}

bool MediaIndicatorButton::OnMousePressed(const ui::MouseEvent& event) {
  // Do not handle this mouse event when anything but the left mouse button is
  // pressed or when any modifier keys are being held down.  Instead, the Tab
  // should react (e.g., middle-click for close, right-click for context menu).
  if (!event.IsOnlyLeftMouseButton() || IsShiftOrControlDown(event)) {
    if (state() != views::CustomButton::STATE_DISABLED)
      SetState(views::CustomButton::STATE_NORMAL);  // Turn off hover.
    return false;  // Event to be handled by Tab.
  }
  return ImageButton::OnMousePressed(event);
}

bool MediaIndicatorButton::OnMouseDragged(const ui::MouseEvent& event) {
  const ButtonState previous_state = state();
  const bool ret = ImageButton::OnMouseDragged(event);
  if (previous_state != views::CustomButton::STATE_NORMAL &&
      state() == views::CustomButton::STATE_NORMAL)
    content::RecordAction(UserMetricsAction("MediaIndicatorButton_Dragged"));
  return ret;
}

void MediaIndicatorButton::OnMouseEntered(const ui::MouseEvent& event) {
  // If any modifier keys are being held down, do not turn on hover.
  if (state() != views::CustomButton::STATE_DISABLED &&
      IsShiftOrControlDown(event)) {
    SetState(views::CustomButton::STATE_NORMAL);
    return;
  }
  ImageButton::OnMouseEntered(event);
}

void MediaIndicatorButton::OnMouseMoved(const ui::MouseEvent& event) {
  // If any modifier keys are being held down, turn off hover.
  if (state() != views::CustomButton::STATE_DISABLED &&
      IsShiftOrControlDown(event)) {
    SetState(views::CustomButton::STATE_NORMAL);
    return;
  }
  ImageButton::OnMouseMoved(event);
}

void MediaIndicatorButton::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  UpdateEnabledForMuteToggle();
}

void MediaIndicatorButton::OnPaint(gfx::Canvas* canvas) {
  double opaqueness =
      fade_animation_ ? fade_animation_->GetCurrentValue() : 1.0;
  if (media_state_ == TAB_MEDIA_STATE_NONE)
    opaqueness = 1.0 - opaqueness;  // Fading out, not in.
  if (opaqueness < 1.0)
    canvas->SaveLayerAlpha(opaqueness * SK_AlphaOPAQUE);
  ImageButton::OnPaint(canvas);
  if (opaqueness < 1.0)
    canvas->Restore();
}

bool MediaIndicatorButton::DoesIntersectRect(const views::View* target,
                                             const gfx::Rect& rect) const {
  // If this button is not enabled, Tab (the parent View) handles all mouse
  // events.
  return enabled() &&
      views::ViewTargeterDelegate::DoesIntersectRect(target, rect);
}

void MediaIndicatorButton::NotifyClick(const ui::Event& event) {
  if (media_state_ == TAB_MEDIA_STATE_AUDIO_PLAYING)
    content::RecordAction(UserMetricsAction("MediaIndicatorButton_Mute"));
  else if (media_state_ == TAB_MEDIA_STATE_AUDIO_MUTING)
    content::RecordAction(UserMetricsAction("MediaIndicatorButton_Unmute"));
  else
    NOTREACHED();

  GetTab()->controller()->ToggleTabAudioMute(GetTab());
}

bool MediaIndicatorButton::IsTriggerableEvent(const ui::Event& event) {
  // For mouse events, only trigger on the left mouse button and when no
  // modifier keys are being held down.
  if (event.IsMouseEvent() &&
      (!static_cast<const ui::MouseEvent*>(&event)->IsOnlyLeftMouseButton() ||
       IsShiftOrControlDown(event)))
    return false;

  // For gesture events on an inactive tab, require an even wider tab before
  // click-to-mute can be triggered.  See comments in
  // UpdateEnabledForMuteToggle().
  if (event.IsGestureEvent() && !GetTab()->IsActive()) {
    const int required_width = width() * kMinGestureSelectableAreaPercent / 100;
    if (GetTab()->GetWidthOfLargestSelectableRegion() < required_width)
      return false;
  }

  return views::ImageButton::IsTriggerableEvent(event);
}

Tab* MediaIndicatorButton::GetTab() const {
  DCHECK_EQ(static_cast<views::View*>(parent_tab_), parent());
  return parent_tab_;
}

void MediaIndicatorButton::ResetImages(TabMediaState state) {
  SkColor color = parent_tab_->button_color();
  gfx::ImageSkia indicator_image =
      chrome::GetTabMediaIndicatorImage(state, color).AsImageSkia();
  SetImage(views::CustomButton::STATE_NORMAL, &indicator_image);
  SetImage(views::CustomButton::STATE_DISABLED, &indicator_image);
  gfx::ImageSkia affordance_image =
      chrome::GetTabMediaIndicatorAffordanceImage(state, color).AsImageSkia();
  SetImage(views::CustomButton::STATE_HOVERED, &affordance_image);
  SetImage(views::CustomButton::STATE_PRESSED, &affordance_image);
}
