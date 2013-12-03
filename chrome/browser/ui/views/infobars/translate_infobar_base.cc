// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/translate_infobar_base.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/after_translate_infobar.h"
#include "chrome/browser/ui/views/infobars/before_translate_infobar.h"
#include "chrome/browser/ui/views/infobars/translate_message_infobar.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"


// TranslateInfoBarDelegate ---------------------------------------------------

InfoBar* TranslateInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  if (infobar_type_ == BEFORE_TRANSLATE)
    return new BeforeTranslateInfoBar(owner, this);
  if (infobar_type_ == AFTER_TRANSLATE)
    return new AfterTranslateInfoBar(owner, this);
  return new TranslateMessageInfoBar(owner, this);
}


// TranslateInfoBarBase -------------------------------------------------------

// static
const int TranslateInfoBarBase::kButtonInLabelSpacing = 5;

void TranslateInfoBarBase::UpdateLanguageButtonText(views::MenuButton* button,
                                                    const string16& text) {
  DCHECK(button);
  button->SetText(text);
  // The button may have to grow to show the new text.
  Layout();
  SchedulePaint();
}

TranslateInfoBarBase::TranslateInfoBarBase(InfoBarService* owner,
                                           TranslateInfoBarDelegate* delegate)
    : InfoBarView(owner, delegate),
      error_background_(InfoBarDelegate::WARNING_TYPE) {
}

TranslateInfoBarBase::~TranslateInfoBarBase() {
}

void TranslateInfoBarBase::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && (details.child == this) &&
      (background_color_animation_ == NULL)) {
    background_color_animation_.reset(new gfx::SlideAnimation(this));
    background_color_animation_->SetTweenType(gfx::Tween::LINEAR);
    background_color_animation_->SetSlideDuration(500);
    TranslateInfoBarDelegate::BackgroundAnimationType animation =
        GetDelegate()->background_animation_type();
    if (animation == TranslateInfoBarDelegate::NORMAL_TO_ERROR) {
      background_color_animation_->Show();
    } else if (animation == TranslateInfoBarDelegate::ERROR_TO_NORMAL) {
      // Hide() runs the animation in reverse.
      background_color_animation_->Reset(1.0);
      background_color_animation_->Hide();
    }
  }

  // This must happen after adding all other children so InfoBarView can ensure
  // the close button is the last child.
  InfoBarView::ViewHierarchyChanged(details);
}

TranslateInfoBarDelegate* TranslateInfoBarBase::GetDelegate() {
  return delegate()->AsTranslateInfoBarDelegate();
}

void TranslateInfoBarBase::OnPaintBackground(gfx::Canvas* canvas) {
  // We need to set the separator color for |error_background_| like
  // InfoBarView::Layout() does for the normal background.
  const InfoBarContainer::Delegate* delegate = container_delegate();
  if (delegate)
    error_background_.set_separator_color(delegate->GetInfoBarSeparatorColor());

  // If we're not animating, simply paint the background for the current state.
  if (!background_color_animation_->is_animating()) {
    GetBackground().Paint(canvas, this);
    return;
  }

  FadeBackground(canvas, 1.0 - background_color_animation_->GetCurrentValue(),
                 *background());
  FadeBackground(canvas, background_color_animation_->GetCurrentValue(),
                 error_background_);
}

void TranslateInfoBarBase::AnimationProgressed(
    const gfx::Animation* animation) {
  if (animation == background_color_animation_.get())
    SchedulePaint();  // That'll trigger a PaintBackgroud.
  else
    InfoBarView::AnimationProgressed(animation);
}

const views::Background& TranslateInfoBarBase::GetBackground() {
  return GetDelegate()->is_error() ? error_background_ : *background();
}

void TranslateInfoBarBase::FadeBackground(gfx::Canvas* canvas,
                                          double animation_value,
                                          const views::Background& background) {
  // Draw the background into an offscreen buffer with alpha value per animation
  // value, then blend it back into the current canvas.
  canvas->SaveLayerAlpha(static_cast<int>(animation_value * 255));
  background.Paint(canvas, this);
  canvas->Restore();
}
