// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_started_animation.h"

#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/rect.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/widget/widget.h"

// How long to spend moving downwards and fading out after waiting.
const int kMoveTimeMs = 600;

// The animation framerate.
const int kFrameRateHz = 60;

namespace {

// DownloadStartAnimation creates an animation (which begins running
// immediately) that animates an image downward from the center of the frame
// provided on the constructor, while simultaneously fading it out.  To use,
// simply call "new DownloadStartAnimation"; the class cleans itself up when it
// finishes animating.
class DownloadStartedAnimationViews : public gfx::LinearAnimation,
                                      public views::ImageView {
 public:
  explicit DownloadStartedAnimationViews(content::WebContents* web_contents);

 private:
  // Move the animation to wherever it should currently be.
  void Reposition();

  // Shut down the animation cleanly.
  void Close();

  // Animation
  virtual void AnimateToState(double state) OVERRIDE;

  // We use a TYPE_POPUP for the popup so that it may float above any windows in
  // our UI.
  views::Widget* popup_;

  // The content area at the start of the animation. We store this so that the
  // download shelf's resizing of the content area doesn't cause the animation
  // to move around. This means that once started, the animation won't move
  // with the parent window, but it's so fast that this shouldn't cause too
  // much heartbreak.
  gfx::Rect web_contents_bounds_;

  DISALLOW_COPY_AND_ASSIGN(DownloadStartedAnimationViews);
};

DownloadStartedAnimationViews::DownloadStartedAnimationViews(
    content::WebContents* web_contents)
    : gfx::LinearAnimation(kMoveTimeMs, kFrameRateHz, NULL),
      popup_(NULL) {
  static gfx::ImageSkia* kDownloadImage = NULL;
  if (!kDownloadImage) {
    kDownloadImage = ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
        IDR_DOWNLOAD_ANIMATION_BEGIN);
  }

  // If we're too small to show the download image, then don't bother -
  // the shelf will be enough.
  web_contents_bounds_= web_contents->GetContainerBounds();
  if (web_contents_bounds_.height() < kDownloadImage->height())
    return;

  SetImage(kDownloadImage);

  popup_ = new views::Widget;

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = false;
  params.parent = web_contents->GetNativeView();
  popup_->Init(params);
  popup_->SetOpacity(0x00);
  popup_->SetContentsView(this);
  Reposition();
  popup_->Show();

  Start();
}

void DownloadStartedAnimationViews::Reposition() {
  // Align the image with the bottom left of the web contents (so that it
  // points to the newly created download).
  gfx::Size size = GetPreferredSize();
  int x = base::i18n::IsRTL() ?
      web_contents_bounds_.right() - size.width() : web_contents_bounds_.x();
  popup_->SetBounds(gfx::Rect(
      x,
      static_cast<int>(web_contents_bounds_.bottom() -
          size.height() - size.height() * (1 - GetCurrentValue())),
      size.width(),
      size.height()));
}

void DownloadStartedAnimationViews::Close() {
  popup_->Close();
}

void DownloadStartedAnimationViews::AnimateToState(double state) {
  if (state >= 1.0) {
    Close();
  } else {
    Reposition();

    // Start at zero, peak halfway and end at zero.
    double opacity = std::min(1.0 - pow(GetCurrentValue() - 0.5, 2) * 4.0,
                              static_cast<double>(1.0));

    popup_->SetOpacity(static_cast<unsigned char>(opacity * 255.0));
  }
}

}  // namespace

// static
void DownloadStartedAnimation::Show(content::WebContents* web_contents) {
  // The animation will delete itself when it's finished.
  new DownloadStartedAnimationViews(web_contents);
}
