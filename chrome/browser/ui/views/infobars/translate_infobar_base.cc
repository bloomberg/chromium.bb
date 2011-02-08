// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/translate_infobar_base.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/after_translate_infobar.h"
#include "chrome/browser/ui/views/infobars/before_translate_infobar.h"
#include "chrome/browser/ui/views/infobars/infobar_button_border.h"
#include "chrome/browser/ui/views/infobars/translate_message_infobar.h"
#include "grit/theme_resources.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/image_view.h"

TranslateInfoBarBase::TranslateInfoBarBase(
    TranslateInfoBarDelegate* delegate)
    : InfoBarView(delegate),
      normal_background_(InfoBarDelegate::PAGE_ACTION_TYPE),
      error_background_(InfoBarDelegate::WARNING_TYPE) {
  icon_ = new views::ImageView;
  SkBitmap* image = static_cast<InfoBarDelegate*>(delegate)->GetIcon();
  if (image)
    icon_->SetImage(image);
  AddChildView(icon_);

  TranslateInfoBarDelegate::BackgroundAnimationType animation =
      delegate->background_animation_type();
  if (animation != TranslateInfoBarDelegate::NONE) {
    background_color_animation_.reset(new ui::SlideAnimation(this));
    background_color_animation_->SetTweenType(ui::Tween::LINEAR);
    background_color_animation_->SetSlideDuration(500);
    if (animation == TranslateInfoBarDelegate::NORMAL_TO_ERROR) {
      background_color_animation_->Show();
    } else {
      DCHECK_EQ(TranslateInfoBarDelegate::ERROR_TO_NORMAL, animation);
      // Hide() runs the animation in reverse.
      background_color_animation_->Reset(1.0);
      background_color_animation_->Hide();
    }
  }
}

TranslateInfoBarBase::~TranslateInfoBarBase() {
}

// Overridden from views::View:
void TranslateInfoBarBase::Layout() {
  // Layout the close button.
  InfoBarView::Layout();

  // Layout the icon on left of bar.
  gfx::Size icon_ps = icon_->GetPreferredSize();
  icon_->SetBounds(InfoBarView::kHorizontalPadding,
      InfoBarView::OffsetY(this, icon_ps), icon_ps.width(), icon_ps.height());
}

views::Label* TranslateInfoBarBase::CreateLabel(const string16& text) {
  views::Label* label = new views::Label(UTF16ToWideHack(text),
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  label->SetColor(SK_ColorBLACK);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  return label;
}

void TranslateInfoBarBase::PaintBackground(gfx::Canvas* canvas) {
  // If we're not animating, simply paint the background for the current state.
  if (background_color_animation_ == NULL ||
      !background_color_animation_->is_animating()) {
    GetBackground().Paint(canvas, this);
    return;
  }

  FadeBackground(canvas, 1.0 - background_color_animation_->GetCurrentValue(),
                 normal_background_);
  FadeBackground(canvas, background_color_animation_->GetCurrentValue(),
                 error_background_);
}

void TranslateInfoBarBase::AnimationProgressed(const ui::Animation* animation) {
  if (background_color_animation_.get() == animation)
    SchedulePaint();  // That'll trigger a PaintBackgroud.
  else
    InfoBarView::AnimationProgressed(animation);
}

views::MenuButton* TranslateInfoBarBase::CreateMenuButton(
    const string16& text,
    bool normal_has_border,
    views::ViewMenuDelegate* menu_delegate) {
  // Don't use text to instantiate MenuButton because we need to set font before
  // setting text so that the button will resize to fit the entire text.
  views::MenuButton* menu_button =
      new views::MenuButton(NULL, std::wstring(), menu_delegate, true);
  menu_button->set_border(new InfoBarButtonBorder);
  menu_button->set_menu_marker(ResourceBundle::GetSharedInstance().
      GetBitmapNamed(IDR_INFOBARBUTTON_MENU_DROPARROW));
  if (normal_has_border) {
    menu_button->SetNormalHasBorder(true);  // Normal button state has border.
    // Disable animation during state change.
    menu_button->SetAnimationDuration(0);
  }
  // Set font colors for different states.
  menu_button->SetEnabledColor(SK_ColorBLACK);
  menu_button->SetHighlightColor(SK_ColorBLACK);
  menu_button->SetHoverColor(SK_ColorBLACK);

  // Set font then text, then size button to fit text.
  menu_button->SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::MediumFont));
  menu_button->SetText(UTF16ToWideHack(text));
  menu_button->ClearMaxTextSize();
  menu_button->SizeToPreferredSize();
  return menu_button;
}

gfx::Point TranslateInfoBarBase::DetermineMenuPosition(
    views::MenuButton* menu_button) {
  gfx::Rect lb = menu_button->GetContentsBounds();
  gfx::Point menu_position(lb.origin());
  menu_position.Offset(2, lb.height() - 3);
  if (base::i18n::IsRTL())
    menu_position.Offset(lb.width() - 4, 0);

  View::ConvertPointToScreen(menu_button, &menu_position);
#if defined(OS_WIN)
  int left_bound = GetSystemMetrics(SM_XVIRTUALSCREEN);
  if (menu_position.x() < left_bound)
    menu_position.set_x(left_bound);
#endif
  return menu_position;
}

TranslateInfoBarDelegate* TranslateInfoBarBase::GetDelegate() const {
  return static_cast<TranslateInfoBarDelegate*>(delegate());
}

const InfoBarBackground& TranslateInfoBarBase::GetBackground() const {
  return GetDelegate()->IsError() ? error_background_ : normal_background_;
}

void TranslateInfoBarBase::FadeBackground(gfx::Canvas* canvas,
                                          double animation_value,
                                          const InfoBarBackground& background) {
  // Draw the background into an offscreen buffer with alpha value per animation
  // value, then blend it back into the current canvas.
  canvas->SaveLayerAlpha(static_cast<int>(animation_value * 255));
  canvas->AsCanvasSkia()->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);
  background.Paint(canvas, this);
  canvas->Restore();
}

// TranslateInfoBarDelegate views specific method:
InfoBar* TranslateInfoBarDelegate::CreateInfoBar() {
  TranslateInfoBarBase* infobar = NULL;
  switch (type_) {
    case BEFORE_TRANSLATE:
      infobar = new BeforeTranslateInfoBar(this);
      break;
    case AFTER_TRANSLATE:
      infobar = new AfterTranslateInfoBar(this);
      break;
    case TRANSLATING:
    case TRANSLATION_ERROR:
      infobar = new TranslateMessageInfoBar(this);
      break;
    default:
      NOTREACHED();
  }
  // Set |infobar_view_| so that the model can notify the infobar when it
  // changes.
  infobar_view_ = infobar;
  return infobar;
}
