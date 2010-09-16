// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/app_launched_animation.h"

#include "app/animation.h"
#include "app/slide_animation.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "gfx/rect.h"
#include "views/controls/image_view.h"
#include "views/widget/widget_win.h"

namespace {

// Start at 1, end at 0
//    0ms You click the icon
//    0ms The launcher and contents begins to fade out, the icon
//        you clicked does not (stays opaque)
//    0ms The app begins loading behind the launcher
// +100ms The app icon begins to fade out.
// +200ms The launcher finishes fading out
// +350ms The app icon finishes fading out

// How long the fade should take.
static const int kDurationMS = 250;

// The delay before the fade out should start.
static const int kDelayMS = 100;

// AppLaunchedAnimation creates an animation. It loads the icon for the
// extension and once the image is loaded the animation starts. The icon fades
// out after a short delay.
class AppLaunchedAnimationWin : public AnimationDelegate,
                                public ImageLoadingTracker::Observer,
                                public views::ImageView {
 public:
  AppLaunchedAnimationWin(Extension* extension, const gfx::Rect& rect);

 private:
  // AnimationDelegate
  virtual void AnimationProgressed(const Animation* animation);
  virtual void AnimationCanceled(const Animation* animation);
  virtual void AnimationEnded(const Animation* animation);

  // We use a HWND for the popup so that it may float above any HWNDs in our UI.
  views::WidgetWin* popup_;

  // The rect to use for the popup.
  gfx::Rect rect_;

  // Used for loading extension application icon.
  ImageLoadingTracker app_icon_loader_;

  // ImageLoadingTracker::Observer.
  virtual void OnImageLoaded(SkBitmap* image, ExtensionResource resource,
                             int index);

  // Hover animation.
  scoped_ptr<SlideAnimation> animation_;

  DISALLOW_COPY_AND_ASSIGN(AppLaunchedAnimationWin);
};

AppLaunchedAnimationWin::AppLaunchedAnimationWin(Extension* extension,
                                                 const gfx::Rect& rect)
    : popup_(NULL),
      rect_(rect),
      ALLOW_THIS_IN_INITIALIZER_LIST(app_icon_loader_(this)) {
  DCHECK(extension);
  app_icon_loader_.LoadImage(
      extension,
      extension->GetIconResource(Extension::EXTENSION_ICON_LARGE,
                                 ExtensionIconSet::MATCH_EXACTLY),
      rect_.size(),
      ImageLoadingTracker::DONT_CACHE);
}

void AppLaunchedAnimationWin::AnimationCanceled(const Animation* animation) {
  AnimationEnded(animation);
}

void AppLaunchedAnimationWin::AnimationEnded(const Animation* animation) {
  popup_->Close();
}

void AppLaunchedAnimationWin::AnimationProgressed(const Animation* animation) {
  // GetCurrentValue goes from 1 to 0 since we are hiding.
  const double current_value = 1.0 - animation->GetCurrentValue();
  const double current_time = current_value * (kDelayMS + kDurationMS);

  double opacity = 1.0 -
      std::max(0.0, static_cast<double>(current_time - kDelayMS)) /
          static_cast<double>(kDurationMS);

  popup_->SetOpacity(static_cast<SkColor>(opacity * 255.0));
  SchedulePaint();
}

void AppLaunchedAnimationWin::OnImageLoaded(SkBitmap* image,
                                            ExtensionResource resource,
                                            int index) {
  if (image) {
    SetImage(image);

    popup_ = new views::WidgetWin;
    popup_->set_window_style(WS_POPUP);
    popup_->set_window_ex_style(WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                                WS_EX_TRANSPARENT);
    popup_->SetOpacity(static_cast<SkColor>(255));

    popup_->Init(NULL, rect_);
    popup_->SetContentsView(this);
    popup_->Show();

    // Start animation.
    animation_.reset(new SlideAnimation(this));
    animation_->SetSlideDuration(kDelayMS + kDurationMS);
    animation_->SetTweenType(Tween::LINEAR);
    animation_->Reset(1.0);
    animation_->Hide();
  }
}

}  // namespace

// static
void AppLaunchedAnimation::Show(Extension* extension, const gfx::Rect& rect) {
  // The animation will delete itself when it's finished.
  new AppLaunchedAnimationWin(extension, rect);
}
