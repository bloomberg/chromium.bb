// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/translate_infobar_base.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/after_translate_infobar.h"
#include "chrome/browser/ui/views/infobars/before_translate_infobar.h"
#include "chrome/browser/ui/views/infobars/translate_message_infobar.h"
#include "grit/theme_resources.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"

// TranslateInfoBarDelegate ---------------------------------------------------

InfoBar* TranslateInfoBarDelegate::CreateInfoBar(InfoBarTabHelper* owner) {
  TranslateInfoBarBase* infobar = NULL;
  switch (type_) {
    case BEFORE_TRANSLATE:
      infobar = new BeforeTranslateInfoBar(owner, this);
      break;
    case AFTER_TRANSLATE:
      infobar = new AfterTranslateInfoBar(owner, this);
      break;
    case TRANSLATING:
    case TRANSLATION_ERROR:
      infobar = new TranslateMessageInfoBar(owner, this);
      break;
    default:
      NOTREACHED();
  }
  infobar_view_ = infobar;
  return infobar;
}

// TranslateInfoBarBase -------------------------------------------------------

// static
const int TranslateInfoBarBase::kButtonInLabelSpacing = 5;

TranslateInfoBarBase::TranslateInfoBarBase(InfoBarTabHelper* owner,
                                           TranslateInfoBarDelegate* delegate)
    : InfoBarView(owner, delegate),
      error_background_(InfoBarDelegate::WARNING_TYPE) {
}

TranslateInfoBarBase::~TranslateInfoBarBase() {
}

void TranslateInfoBarBase::ViewHierarchyChanged(bool is_add,
                                                View* parent,
                                                View* child) {
  if (is_add && (child == this) && (background_color_animation_ == NULL)) {
    background_color_animation_.reset(new ui::SlideAnimation(this));
    background_color_animation_->SetTweenType(ui::Tween::LINEAR);
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
  InfoBarView::ViewHierarchyChanged(is_add, parent, child);
}

void TranslateInfoBarBase::UpdateLanguageButtonText(
    views::MenuButton* button,
    LanguagesMenuModel::LanguageType language_type) {
  DCHECK(button);
  TranslateInfoBarDelegate* delegate = GetDelegate();
  bool is_original = language_type == LanguagesMenuModel::ORIGINAL;
  int index = is_original ? delegate->original_language_index()
                          : delegate->target_language_index();
  button->SetText(delegate->GetLanguageDisplayableNameAt(index));
  // The button may have to grow to show the new text.
  Layout();
  SchedulePaint();
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

void TranslateInfoBarBase::AnimationProgressed(const ui::Animation* animation) {
  if (animation == background_color_animation_.get())
    SchedulePaint();  // That'll trigger a PaintBackgroud.
  else
    InfoBarView::AnimationProgressed(animation);
}

const views::Background& TranslateInfoBarBase::GetBackground() {
  return GetDelegate()->IsError() ? error_background_ : *background();
}

void TranslateInfoBarBase::FadeBackground(gfx::Canvas* canvas,
                                          double animation_value,
                                          const views::Background& background) {
  // Draw the background into an offscreen buffer with alpha value per animation
  // value, then blend it back into the current canvas.
  canvas->SaveLayerAlpha(static_cast<int>(animation_value * 255));
  canvas->GetSkCanvas()->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);
  background.Paint(canvas, this);
  canvas->Restore();
}
