// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/media_indicator_button.h"

#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_controller.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "content/public/browser/user_metrics.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"

using base::UserMetricsAction;

const char MediaIndicatorButton::kViewClassName[] = "MediaIndicatorButton";

class MediaIndicatorButton::FadeAnimationDelegate
    : public gfx::AnimationDelegate {
 public:
  explicit FadeAnimationDelegate(MediaIndicatorButton* button)
      : button_(button) {}
  virtual ~FadeAnimationDelegate() {}

 private:
  // gfx::AnimationDelegate
  virtual void AnimationProgressed(const gfx::Animation* animation) OVERRIDE {
    button_->SchedulePaint();
  }

  virtual void AnimationCanceled(const gfx::Animation* animation) OVERRIDE {
    button_->showing_media_state_ = button_->media_state_;
    button_->SchedulePaint();
  }

  virtual void AnimationEnded(const gfx::Animation* animation) OVERRIDE {
    button_->showing_media_state_ = button_->media_state_;
    button_->SchedulePaint();
  }

  MediaIndicatorButton* const button_;

  DISALLOW_COPY_AND_ASSIGN(FadeAnimationDelegate);
};

MediaIndicatorButton::MediaIndicatorButton()
    : views::ImageButton(NULL),
      media_state_(TAB_MEDIA_STATE_NONE),
      showing_media_state_(TAB_MEDIA_STATE_NONE) {
  SetEventTargeter(
      scoped_ptr<views::ViewTargeter>(new views::ViewTargeter(this)));
}

MediaIndicatorButton::~MediaIndicatorButton() {}

void MediaIndicatorButton::TransitionToMediaState(TabMediaState next_state) {
  if (next_state == media_state_)
    return;

  if (next_state != TAB_MEDIA_STATE_NONE) {
    const gfx::ImageSkia* const indicator_image =
        chrome::GetTabMediaIndicatorImage(next_state).ToImageSkia();
    SetImage(views::CustomButton::STATE_NORMAL, indicator_image);
    SetImage(views::CustomButton::STATE_DISABLED, indicator_image);
    const gfx::ImageSkia* const affordance_image =
        chrome::GetTabMediaIndicatorAffordanceImage(next_state).ToImageSkia();
    SetImage(views::CustomButton::STATE_HOVERED, affordance_image);
    SetImage(views::CustomButton::STATE_PRESSED, affordance_image);
  }

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

  SetEnabled(chrome::IsTabAudioMutingFeatureEnabled() &&
             (next_state == TAB_MEDIA_STATE_AUDIO_PLAYING ||
              next_state == TAB_MEDIA_STATE_AUDIO_MUTING));

  // An indicator state change should be made visible immediately, instead of
  // the user being surprised when their mouse leaves the button.
  if (state() == views::CustomButton::STATE_HOVERED) {
    SetState(enabled() ? views::CustomButton::STATE_NORMAL :
             views::CustomButton::STATE_DISABLED);
  }

  media_state_ = next_state;

  // Note: The calls to SetImage(), SetEnabled(), and SetState() above will call
  // SchedulePaint() if necessary.
}

const char* MediaIndicatorButton::GetClassName() const {
  return kViewClassName;
}

views::View* MediaIndicatorButton::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return NULL;  // Tab (the parent View) provides the tooltip.
}

bool MediaIndicatorButton::OnMouseDragged(const ui::MouseEvent& event) {
  const ButtonState previous_state = state();
  const bool ret = ImageButton::OnMouseDragged(event);
  if (previous_state != views::CustomButton::STATE_NORMAL &&
      state() == views::CustomButton::STATE_NORMAL)
    content::RecordAction(UserMetricsAction("MediaIndicatorButton_Dragged"));
  return ret;
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

  DCHECK(parent() && !strcmp(parent()->GetClassName(), Tab::kViewClassName));
  Tab* const tab = static_cast<Tab*>(parent());
  tab->controller()->ToggleTabAudioMute(tab);
}
