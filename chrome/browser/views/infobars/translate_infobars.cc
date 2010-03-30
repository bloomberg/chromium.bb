// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/infobars/translate_infobars.h"

#include <algorithm>
#include <vector>

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/slide_animation.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/notification_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/translate/languages_menu_model.h"
#include "chrome/browser/translate/options_menu_model.h"
#include "chrome/browser/translate/page_translated_details.h"
#include "gfx/canvas.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/button/text_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"

// IDs for various menus.
static const int kMenuIDOptions          = 1000;
static const int kMenuIDOriginalLanguage = 1001;
static const int kMenuIDTargetLanguage   = 1002;

////////////////////////////////////////////////////////////////////////////////
//
// TranslateButtonBorder
//
//  A TextButtonBorder subclass that adds to also paint button frame in normal
//  state, and changes the images used.
//
////////////////////////////////////////////////////////////////////////////////

class TranslateButtonBorder : public views::TextButtonBorder {
 public:
  TranslateButtonBorder();
  virtual ~TranslateButtonBorder();

  // Overriden from Border
  virtual void Paint(const views::View& view, gfx::Canvas* canvas) const;

 private:
  MBBImageSet normal_set_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TranslateButtonBorder);
};

// TranslateButtonBorder, public: ----------------------------------------------

TranslateButtonBorder::TranslateButtonBorder() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  normal_set_.top_left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_LEFT_N);
  normal_set_.top = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_N);
  normal_set_.top_right = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_RIGHT_N);
  normal_set_.left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_LEFT_N);
  normal_set_.center = rb.GetBitmapNamed(IDR_INFOBARBUTTON_CENTER_N);
  normal_set_.right = rb.GetBitmapNamed(IDR_INFOBARBUTTON_RIGHT_N);
  normal_set_.bottom_left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_LEFT_N);
  normal_set_.bottom = rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_N);
  normal_set_.bottom_right = rb.GetBitmapNamed(
      IDR_INFOBARBUTTON_BOTTOM_RIGHT_N);

  hot_set_.top_left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_LEFT_H);
  hot_set_.top = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_H);
  hot_set_.top_right = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_RIGHT_H);
  hot_set_.left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_LEFT_H);
  hot_set_.center = rb.GetBitmapNamed(IDR_INFOBARBUTTON_CENTER_H);
  hot_set_.right = rb.GetBitmapNamed(IDR_INFOBARBUTTON_RIGHT_H);
  hot_set_.bottom_left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_LEFT_H);
  hot_set_.bottom = rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_H);
  hot_set_.bottom_right = rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_RIGHT_H);

  pushed_set_.top_left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_LEFT_P);
  pushed_set_.top = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_P);
  pushed_set_.top_right = rb.GetBitmapNamed(IDR_INFOBARBUTTON_TOP_RIGHT_P);
  pushed_set_.left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_LEFT_P);
  pushed_set_.center = rb.GetBitmapNamed(IDR_INFOBARBUTTON_CENTER_P);
  pushed_set_.right = rb.GetBitmapNamed(IDR_INFOBARBUTTON_RIGHT_P);
  pushed_set_.bottom_left = rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_LEFT_P);
  pushed_set_.bottom = rb.GetBitmapNamed(IDR_INFOBARBUTTON_BOTTOM_P);
  pushed_set_.bottom_right = rb.GetBitmapNamed(
      IDR_INFOBARBUTTON_BOTTOM_RIGHT_P);
}

TranslateButtonBorder::~TranslateButtonBorder() {
}

// TranslateButtonBorder, Border overrides: ------------------------------------

void TranslateButtonBorder::Paint(const views::View& view, gfx::Canvas* canvas)
    const {
  const views::TextButton* mb = static_cast<const views::TextButton*>(&view);
  int state = mb->state();

  // TextButton takes care of deciding when to call Paint.
  const MBBImageSet* set = &normal_set_;
  if (state == views::TextButton::BS_HOT)
    set = &hot_set_;
  else if (state == views::TextButton::BS_PUSHED)
    set = &pushed_set_;

  gfx::Rect bounds = view.bounds();

  // Draw top left image.
  canvas->DrawBitmapInt(*set->top_left, 0, 0);

  // Stretch top image.
  canvas->DrawBitmapInt(
      *set->top,
      0, 0, set->top->width(), set->top->height(),
      set->top_left->width(),
      0,
      bounds.width() - set->top_right->width() - set->top_left->width(),
      set->top->height(), false);

  // Draw top right image.
  canvas->DrawBitmapInt(*set->top_right,
                        bounds.width() - set->top_right->width(), 0);

  // Stretch left image.
  canvas->DrawBitmapInt(
      *set->left,
      0, 0, set->left->width(), set->left->height(),
      0,
      set->top_left->height(),
      set->top_left->width(),
      bounds.height() - set->top->height() - set->bottom_left->height(), false);

  // Stretch center image.
  canvas->DrawBitmapInt(
      *set->center,
      0, 0, set->center->width(), set->center->height(),
      set->left->width(),
      set->top->height(),
      bounds.width() - set->right->width() - set->left->width(),
      bounds.height() - set->bottom->height() - set->top->height(), false);

  // Stretch right image.
  canvas->DrawBitmapInt(
      *set->right,
      0, 0, set->right->width(), set->right->height(),
      bounds.width() - set->right->width(),
      set->top_right->height(),
      set->right->width(),
      bounds.height() - set->bottom_right->height() -
          set->top_right->height(), false);

  // Draw bottom left image.
  canvas->DrawBitmapInt(*set->bottom_left,
                        0,
                        bounds.height() - set->bottom_left->height());

  // Stretch bottom image.
  canvas->DrawBitmapInt(
      *set->bottom,
      0, 0, set->bottom->width(), set->bottom->height(),
      set->bottom_left->width(),
      bounds.height() - set->bottom->height(),
      bounds.width() - set->bottom_right->width() -
          set->bottom_left->width(),
      set->bottom->height(), false);

  // Draw bottom right image.
  canvas->DrawBitmapInt(*set->bottom_right,
                        bounds.width() - set->bottom_right->width(),
                        bounds.height() -  set->bottom_right->height());
}

////////////////////////////////////////////////////////////////////////////////
//
// TranslateTextButton
//
//  A TextButton subclass that overrides OnMousePressed to default to
//  CustomButton so as to create pressed state effect.
//
////////////////////////////////////////////////////////////////////////////////

class TranslateTextButton : public views::TextButton {
 public:
  TranslateTextButton(views::ButtonListener* listener, int label_id);
  virtual ~TranslateTextButton();

 protected:
  // Overriden from TextButton:
  virtual bool OnMousePressed(const views::MouseEvent& e);

 private:
  DISALLOW_COPY_AND_ASSIGN(TranslateTextButton);
};

// TranslateButtonBorder, public: ----------------------------------------------

TranslateTextButton::TranslateTextButton(views::ButtonListener* listener,
    int label_id)
    // Don't use text to construct TextButton because we need to set font
    // before setting text so that the button will resize to fit entire text.
    : TextButton(listener, std::wstring()) {
  set_border(new TranslateButtonBorder);
  SetNormalHasBorder(true);  // Normal button state has border.
  SetAnimationDuration(0);  // Disable animation during state change.
  // Set font colors for different states.
  SetEnabledColor(SK_ColorBLACK);
  SetHighlightColor(SK_ColorBLACK);
  SetHoverColor(SK_ColorBLACK);
  // Set font then text, then size button to fit text.
  SetFont(ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::MediumFont));
  SetText(l10n_util::GetString(label_id));
  ClearMaxTextSize();
  SizeToPreferredSize();
}

TranslateTextButton::~TranslateTextButton() {
}

// TranslateTextButton, protected: ---------------------------------------------

bool TranslateTextButton::OnMousePressed(const views::MouseEvent& e) {
  return views::CustomButton::OnMousePressed(e);
}

// TranslateInfoBar, public: ---------------------------------------------------

TranslateInfoBar::TranslateInfoBar(TranslateInfoBarDelegate* delegate)
    : InfoBar(delegate),
      state_(TranslateInfoBarDelegate::kTranslateNone),
      translation_pending_(false),
      label_1_(NULL),
      label_2_(NULL),
      label_3_(NULL),
      translating_label_(NULL),
      error_label_(NULL),
      accept_button_(NULL),
      deny_button_(NULL),
      target_language_menu_button_(NULL),
      revert_button_(NULL),
      retry_button_(NULL),
      swapped_language_placeholders_(false) {
  // Clear background set in base class InfoBarBackground, so that we can
  // handle special background requirements for translate infobar.
  set_background(NULL);

  // Initialize backgrounds.
  normal_background_.reset(
      new InfoBarBackground(InfoBarDelegate::PAGE_ACTION_TYPE));
  error_background_.reset(
      new InfoBarBackground(InfoBarDelegate::ERROR_TYPE));

  // Initialize slide animation for transitioning to and from error state.
  error_animation_.reset(new SlideAnimation(this));
  error_animation_->SetTweenType(SlideAnimation::NONE);
  error_animation_->SetSlideDuration(500);

  // Initialize icon.
  icon_ = new views::ImageView;
  SkBitmap* image = delegate->GetIcon();
  if (image)
    icon_->SetImage(image);
  AddChildView(icon_);

  // Create original language menu button.
  string16 language_name = delegate->GetDisplayNameForLocale(
      GetDelegate()->original_lang_code());
  original_language_menu_button_ = CreateMenuButton(kMenuIDOriginalLanguage,
      UTF16ToWideHack(language_name), true);
  AddChildView(original_language_menu_button_);

  // Create options menu button.
  options_menu_button_ = CreateMenuButton(kMenuIDOptions,
      l10n_util::GetString(IDS_TRANSLATE_INFOBAR_OPTIONS), false);
  AddChildView(options_menu_button_);

  // Create state-dependent controls.
  UpdateState(GetDelegate()->state(), GetDelegate()->translation_pending(),
              GetDelegate()->error_type());

  // Register for PAGE_TRANSLATED notification.
  notification_registrar_.Add(this, NotificationType::PAGE_TRANSLATED,
      Source<TabContents>(GetDelegate()->tab_contents()));

  if (GetDelegate()->state() == TranslateInfoBarDelegate::kBeforeTranslate)
    UMA_HISTOGRAM_COUNTS("Translate.ShowBeforeTranslateInfobar", 1);
}

TranslateInfoBar::~TranslateInfoBar() {
}

void TranslateInfoBar::UpdateState(
    TranslateInfoBarDelegate::TranslateState new_state,
    bool new_translation_pending, TranslateErrors::Type error_type) {
  if (state_ == new_state && translation_pending_ == new_translation_pending)
    return;

  TranslateInfoBarDelegate::TranslateState old_state = state_;
  state_ = new_state;
  translation_pending_ = new_translation_pending;

  // Create and initialize state-dependent controls if necessary.
  switch (state_) {
    case TranslateInfoBarDelegate::kAfterTranslate:
      if (!target_language_menu_button_) {
        CreateLabels();
        string16 language_name =
            GetDelegate()->GetDisplayNameForLocale(
                GetDelegate()->target_lang_code());
        target_language_menu_button_ = CreateMenuButton(kMenuIDTargetLanguage,
            UTF16ToWideHack(language_name), true);
        AddChildView(target_language_menu_button_);
      }
      if (!revert_button_) {
        revert_button_ = new TranslateTextButton(this,
            IDS_TRANSLATE_INFOBAR_REVERT);
        AddChildView(revert_button_);
      }
      break;

    case TranslateInfoBarDelegate::kBeforeTranslate:
      if (!label_1_)
        CreateLabels();
      if (!accept_button_) {
        accept_button_ = new TranslateTextButton(this,
            IDS_TRANSLATE_INFOBAR_ACCEPT);
        AddChildView(accept_button_);
      }
      if (!deny_button_) {
        deny_button_ = new TranslateTextButton(this,
            IDS_TRANSLATE_INFOBAR_DENY);
        AddChildView(deny_button_);
      }
      break;

    case TranslateInfoBarDelegate::kTranslateError: {
      string16 error_message_utf16 = GetDelegate()->GetErrorMessage(error_type);
      std::wstring error_message = UTF16ToWideHack(error_message_utf16);
      if (error_label_) {
        error_label_->SetText(error_message);
      } else {
        error_label_ = CreateLabel(error_message);
        AddChildView(error_label_);
      }
      if (!retry_button_) {
        retry_button_ = new TranslateTextButton(this,
            IDS_TRANSLATE_INFOBAR_RETRY);
        AddChildView(retry_button_);
      }
      break;
    }

    default:
      NOTREACHED() << "Invalid translate state change";
      break;
  }

  // If translation is pending, create "Translating..." label.
  if (translation_pending_ && !translating_label_) {
    translating_label_ = CreateLabel(
        l10n_util::GetString(IDS_TRANSLATE_INFOBAR_TRANSLATING));
    AddChildView(translating_label_);
  }

  // Determine visibility of controls.
  if (label_1_)
    label_1_->SetVisible(
        state_ != TranslateInfoBarDelegate::kTranslateError);
  if (label_2_)
    label_2_->SetVisible(
        state_ != TranslateInfoBarDelegate::kTranslateError);
  if (label_3_ && !label_3_->GetText().empty())
    label_3_->SetVisible(
        state_ != TranslateInfoBarDelegate::kTranslateError);
  if (accept_button_)
    accept_button_->SetVisible(!translation_pending_ &&
        state_ == TranslateInfoBarDelegate::kBeforeTranslate);
  if (deny_button_)
    deny_button_->SetVisible(!translation_pending_ &&
        state_ == TranslateInfoBarDelegate::kBeforeTranslate);
  if (target_language_menu_button_)
    target_language_menu_button_->SetVisible(
        state_ == TranslateInfoBarDelegate::kAfterTranslate);
  if (revert_button_)
    revert_button_->SetVisible(!translation_pending_ &&
        state_ == TranslateInfoBarDelegate::kAfterTranslate);
  if (translating_label_)
    translating_label_->SetVisible(translation_pending_);
  if (error_label_)
    error_label_->SetVisible(!translation_pending_ &&
        state_ == TranslateInfoBarDelegate::kTranslateError);
  if (retry_button_)
    retry_button_->SetVisible(!translation_pending_ &&
        state_ == TranslateInfoBarDelegate::kTranslateError);
  if (options_menu_button_)
    options_menu_button_->SetVisible(
        state_ != TranslateInfoBarDelegate::kTranslateError);
  if (original_language_menu_button_)
    original_language_menu_button_->SetVisible(
        state_ != TranslateInfoBarDelegate::kTranslateError);
  if (target_language_menu_button_)
    target_language_menu_button_->SetVisible(
        state_ != TranslateInfoBarDelegate::kTranslateError);

  // If background should change per state, trigger animation of transition
  // accordingly.
  if (old_state != TranslateInfoBarDelegate::kTranslateError &&
      state_ == TranslateInfoBarDelegate::kTranslateError)
    error_animation_->Show();  // Transition to error state.
  else if (old_state == TranslateInfoBarDelegate::kTranslateError &&
           state_ != TranslateInfoBarDelegate::kTranslateError)
    error_animation_->Hide();  // Transition from error state.
  else
    error_animation_->Stop();  // No transition.

  // Trigger layout and repaint.
  Layout();
  SchedulePaint();
}

// TranslateInfoBar, views::View overrides: ------------------------------------

void TranslateInfoBar::Layout() {
  // Layout the close button.
  InfoBar::Layout();

  // Layout the icon on left of bar.
  gfx::Size icon_ps = icon_->GetPreferredSize();
  icon_->SetBounds(InfoBar::kHorizontalPadding, InfoBar::OffsetY(this, icon_ps),
      icon_ps.width(), icon_ps.height());

  // Check if translation is pending.
  gfx::Size translating_ps;
  if (translation_pending_)
    translating_ps = translating_label_->GetPreferredSize();
  int translating_width = translating_ps.width();

  // Handle error state.
  if (state_ == TranslateInfoBarDelegate::kTranslateError) {
    int available_text_width = std::max(GetAvailableWidth(), 0);
    if (translation_pending_) {  // Layout "Translating..." label.
      translating_label_->SetBounds(icon_->bounds().right() +
          InfoBar::kIconLabelSpacing, InfoBar::OffsetY(this, translating_ps),
          std::min(translating_width, available_text_width),
          translating_ps.height());
    } else  {  // Layout error label and retry button.
      gfx::Size error_ps = error_label_->GetPreferredSize();
      error_label_->SetBounds(icon_->bounds().right() +
          InfoBar::kIconLabelSpacing, InfoBar::OffsetY(this, error_ps),
          std::min(error_ps.width(), available_text_width), error_ps.height());
      gfx::Size retry_ps = retry_button_->GetPreferredSize();
      retry_button_->SetBounds(error_label_->bounds().right() +
          InfoBar::kEndOfLabelSpacing, InfoBar::OffsetY(this, retry_ps),
          retry_ps.width(), retry_ps.height());
    }
    return;
  }

  // Handle normal states.
  // Layout the options menu button on right of bar.
  int available_width = InfoBar::GetAvailableWidth();
  gfx::Size options_ps = options_menu_button_->GetPreferredSize();
  options_menu_button_->SetBounds(available_width - options_ps.width(),
      OffsetY(this, options_ps), options_ps.width(), options_ps.height());

  // Layout the controls between icon and options i.e. labels, original language
  // menu button, and if available, target language menu button.
  views::MenuButton* button1 = original_language_menu_button_;
  views::MenuButton* button2 =
      (state_ == TranslateInfoBarDelegate::kAfterTranslate ?
          target_language_menu_button_ : NULL);
  if (button2 && swapped_language_placeholders_) {
    button1 = button2;
    button2 = original_language_menu_button_;
  }
  gfx::Size button1_ps = button1->GetPreferredSize();
  int available_text_width = std::max(GetAvailableWidth(), 0);
  gfx::Size label1_ps = label_1_->GetPreferredSize();
  gfx::Size label2_ps = label_2_->GetPreferredSize();
  gfx::Size label3_ps;
  if (label_3_)
    label3_ps = label_3_->GetPreferredSize();
  int text1_width = label1_ps.width();
  int text2_width = label2_ps.width();
  int text3_width = label3_ps.width();
  int total_text_width = text1_width + text2_width + text3_width +
      translating_width;
  if (total_text_width > available_text_width) {
    double ratio = static_cast<double>(available_text_width) /
        static_cast<double>(total_text_width);
    text1_width = static_cast<int>(text1_width * ratio);
    text2_width = static_cast<int>(text2_width * ratio);
    text3_width = static_cast<int>(text3_width * ratio);
    translating_width = static_cast<int>(translating_width * ratio);
  }
  label_1_->SetBounds(icon_->bounds().right() + InfoBar::kIconLabelSpacing,
      InfoBar::OffsetY(this, label1_ps), text1_width, label1_ps.height());

  // Place first language menu button after label_1.
  button1->SetBounds(label_1_->bounds().right() +
      InfoBar::kButtonInLabelSpacing, OffsetY(this, button1_ps),
      button1_ps.width(), button1_ps.height());

  // Place label_2 after first language menu button.
  label_2_->SetBounds(button1->bounds().right() +
      GetSpacingAfterFirstLanguageButton(), InfoBar::OffsetY(this, label2_ps),
      text2_width, label2_ps.height());

  int prev_right = label_2_->bounds().right();

  // If second language menu button is available, place it after label_2.
  if (button2) {
    gfx::Size button2_ps = button2->GetPreferredSize();
    button2->SetBounds(prev_right + InfoBar::kButtonInLabelSpacing,
        OffsetY(this, button2_ps), button2_ps.width(), button2_ps.height());
    prev_right = button2->bounds().right();

    // If label_3 is available, place it after second language menu button.
    if (label_3_) {
      gfx::Size label3_ps = label_3_->GetPreferredSize();
      label_3_->SetBounds(prev_right + InfoBar::kButtonInLabelSpacing,
          InfoBar::OffsetY(this, label3_ps), text3_width, label3_ps.height());
      prev_right = label_3_->bounds().right();
    }

    // If no translation is pending, layout revert button.
    if (!translation_pending_ && revert_button_) {
      gfx::Size revert_ps = revert_button_->GetPreferredSize();
      revert_button_->SetBounds(prev_right + InfoBar::kEndOfLabelSpacing,
          OffsetY(this, revert_ps), revert_ps.width(), revert_ps.height());
    }
  }

  // If translate state is kBeforeTranslate with no pending translation,
  // layout accept and deny butons.
  if (state_ == TranslateInfoBarDelegate::kBeforeTranslate &&
      !translation_pending_) {
    gfx::Size accept_ps = accept_button_->GetPreferredSize();
    accept_button_->SetBounds(prev_right + InfoBar::kEndOfLabelSpacing,
        OffsetY(this, accept_ps), accept_ps.width(), accept_ps.height());
    gfx::Size deny_ps = deny_button_->GetPreferredSize();
    deny_button_->SetBounds(
        accept_button_->bounds().right() + InfoBar::kButtonButtonSpacing,
        OffsetY(this, deny_ps), deny_ps.width(), deny_ps.height());
  }

  // If translation is pending, layout "Translating..." label.
  if (translation_pending_) {
    translating_label_->SetBounds(
        prev_right + InfoBar::kEndOfLabelSpacing,
        InfoBar::OffsetY(this, translating_ps),
        translating_width, translating_ps.height());
  }
}

void TranslateInfoBar::PaintBackground(gfx::Canvas* canvas) {
  // If we're not animating, simply paint background for current state.
  if (!error_animation_->IsAnimating()) {
    GetBackground(state_)->Paint(canvas, this);
    return;
  }

  // Animate cross-fading between error and normal states;
  // since all normal states use the same background, just use kAfterTranslate.
  if (error_animation_->IsShowing()) {  // Transitioning to error state.
    // Fade out normal state.
    FadeBackground(canvas, 1.0 - error_animation_->GetCurrentValue(),
        TranslateInfoBarDelegate::kAfterTranslate);
    // Fade in error state.
    FadeBackground(canvas, error_animation_->GetCurrentValue(),
        TranslateInfoBarDelegate::kTranslateError);
  } else {  // Transitioning from error state.
    // Fade out error state.
    FadeBackground(canvas, error_animation_->GetCurrentValue(),
        TranslateInfoBarDelegate::kTranslateError);
    // Fade in normal state.
    FadeBackground(canvas, 1.0 - error_animation_->GetCurrentValue(),
        TranslateInfoBarDelegate::kAfterTranslate);
  }
}

// TranslateInfoBar, InfoBar overrides: ----------------------------------------

int TranslateInfoBar::GetAvailableWidth() const {
  int available_width = InfoBar::GetAvailableWidth() -
      icon_->bounds().right() - InfoBar::kIconLabelSpacing;

  // Handle the simplest state - error state, with the least no. of controls.
  if (state_ == TranslateInfoBarDelegate::kTranslateError) {
    if (!translation_pending_)
      available_width -= InfoBar::kEndOfLabelSpacing +
        retry_button_->bounds().width();
    return available_width;
  }

  // Handle the normal states, which have more controls.
  // For language button, reserve spacing before and after it.
  gfx::Size language_ps = original_language_menu_button_->GetPreferredSize();
  int language_spacing = InfoBar::kButtonInLabelSpacing +
      GetSpacingAfterFirstLanguageButton();
  // Options button could come after different types of controls, so we reserve
  // different spacings for each:
  // - after label_3 (i.e. this label follows the second language button):
  //   spacing after second language button and for end of sentence
  // - after second language button (i.e. there's no label_3): spacing for end
  //   of sentence
  // - all other cases, regular button spacing before options button
  gfx::Size options_ps = options_menu_button_->GetPreferredSize();
  int options_spacing =
      (label_3_ ? InfoBar::kButtonInLabelSpacing + InfoBar::kEndOfLabelSpacing :
          (target_language_menu_button_ ? InfoBar::kEndOfLabelSpacing :
              InfoBar::kButtonButtonSpacing));
  available_width -= options_ps.width() + options_spacing +
      language_ps.width() + language_spacing;
  if (state_ == TranslateInfoBarDelegate::kBeforeTranslate) {
    gfx::Size accept_ps = accept_button_->GetPreferredSize();
    gfx::Size deny_ps = deny_button_->GetPreferredSize();
    available_width -= accept_ps.width() + InfoBar::kEndOfLabelSpacing +
        deny_ps.width() + InfoBar::kButtonButtonSpacing;
  } else if (state_ == TranslateInfoBarDelegate::kAfterTranslate) {
    gfx::Size target_ps = target_language_menu_button_->GetPreferredSize();
    available_width -= target_ps.width() + InfoBar::kButtonInLabelSpacing;
  }
  return available_width;
}

// TranslateInfoBar, views::ViewMenuDelegate implementation: -------------------

void TranslateInfoBar::RunMenu(views::View* source, const gfx::Point& pt) {
  switch (source->GetID()) {
    case kMenuIDOptions: {
      if (!options_menu_model_.get()) {
        options_menu_model_.reset(new OptionsMenuModel(this, GetDelegate()));
        options_menu_menu_.reset(new views::Menu2(options_menu_model_.get()));
      }
      options_menu_menu_->RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
      break;
    }

    case kMenuIDOriginalLanguage: {
      if (!translation_pending_) {
        if (!original_language_menu_model_.get()) {
          original_language_menu_model_.reset(
              new LanguagesMenuModel(this, GetDelegate(), true));
          original_language_menu_menu_.reset(
              new views::Menu2(original_language_menu_model_.get()));
        }
        views::Menu2::Alignment alignment;
        gfx::Point menu_position = DetermineMenuPositionAndAlignment(
            original_language_menu_button_, &alignment);
        original_language_menu_menu_->RunMenuAt(menu_position, alignment);
      }
      break;
    }

    case kMenuIDTargetLanguage: {
      if (!translation_pending_) {
        if (!target_language_menu_model_.get()) {
          target_language_menu_model_.reset(
              new LanguagesMenuModel(this, GetDelegate(), false));
          target_language_menu_menu_.reset(
              new views::Menu2(target_language_menu_model_.get()));
        }
        views::Menu2::Alignment alignment;
        gfx::Point menu_position = DetermineMenuPositionAndAlignment(
            target_language_menu_button_, &alignment);
        target_language_menu_menu_->RunMenuAt(menu_position, alignment);
      }
      break;
    }

    default:
      NOTREACHED() << "Invalid source menu.";
      break;
  }
}

// TranslateInfoBar, menus::SimpleMenuModel::Delegate implementation: ----------

bool TranslateInfoBar::IsCommandIdChecked(int command_id) const {
  TranslateInfoBarDelegate* translate_delegate = GetDelegate();
  switch (command_id) {
    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG :
      return translate_delegate->IsLanguageBlacklisted();

    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_SITE :
      return translate_delegate->IsSiteBlacklisted();

    case IDC_TRANSLATE_OPTIONS_ALWAYS :
      return translate_delegate->ShouldAlwaysTranslate();

    default:
      NOTREACHED() << "Invalid command_id from menu";
      break;
  }
  return false;
}

bool TranslateInfoBar::IsCommandIdEnabled(int command_id) const {
  TranslateInfoBarDelegate* translate_delegate = GetDelegate();
  switch (command_id) {
    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG :
    case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_SITE :
      return !translate_delegate->ShouldAlwaysTranslate();

    case IDC_TRANSLATE_OPTIONS_ALWAYS :
      return (!translate_delegate->IsLanguageBlacklisted() &&
          !translate_delegate->IsSiteBlacklisted());

    default:
      break;
  }
  return true;
}

bool TranslateInfoBar::GetAcceleratorForCommandId(int command_id,
    menus::Accelerator* accelerator) {
  return false;
}

void TranslateInfoBar::ExecuteCommand(int command_id) {
  if (command_id >= IDC_TRANSLATE_TARGET_LANGUAGE_BASE) {
    UMA_HISTOGRAM_COUNTS("Translate.ModifyTargetLang", 1);
    OnLanguageModified(target_language_menu_button_,
        command_id - IDC_TRANSLATE_TARGET_LANGUAGE_BASE);
  } else if (command_id >= IDC_TRANSLATE_ORIGINAL_LANGUAGE_BASE) {
    UMA_HISTOGRAM_COUNTS("Translate.ModifyOriginalLang", 1);
    OnLanguageModified(original_language_menu_button_,
        command_id - IDC_TRANSLATE_ORIGINAL_LANGUAGE_BASE);
  } else {
    switch (command_id) {
      case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG:
        UMA_HISTOGRAM_COUNTS("Translate.NeverTranslateLang", 1);
        GetDelegate()->ToggleLanguageBlacklist();
        break;

      case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_SITE:
        UMA_HISTOGRAM_COUNTS("Translate.NeverTranslateSite", 1);
        GetDelegate()->ToggleSiteBlacklist();
        break;

      case IDC_TRANSLATE_OPTIONS_ALWAYS:
        UMA_HISTOGRAM_COUNTS("Translate.AlwaysTranslateLang", 1);
        GetDelegate()->ToggleAlwaysTranslate();
        break;

      case IDC_TRANSLATE_OPTIONS_ABOUT: {
        TabContents* tab_contents = GetDelegate()->tab_contents();
        if (tab_contents) {
          string16 url = l10n_util::GetStringUTF16(
              IDS_ABOUT_GOOGLE_TRANSLATE_URL);
          tab_contents->OpenURL(GURL(url), GURL(), NEW_FOREGROUND_TAB,
              PageTransition::LINK);
        }
        break;
      }

      default:
        NOTREACHED() << "Invalid command id from menu.";
        break;
    }
  }
}

// TranslateInfoBar, views::ButtonListener overrides: --------------------------

void TranslateInfoBar::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == accept_button_ || sender == retry_button_) {
    GetDelegate()->Translate();
    UpdateState(GetDelegate()->state(), GetDelegate()->translation_pending(),
                GetDelegate()->error_type());
    UMA_HISTOGRAM_COUNTS("Translate.Translate", 1);
  } else if (sender == deny_button_) {
    GetDelegate()->TranslationDeclined();
    UMA_HISTOGRAM_COUNTS("Translate.DeclineTranslate", 1);
    RemoveInfoBar();
  } else if (sender == revert_button_) {
    GetDelegate()->RevertTranslation();
  } else {  // Let base InfoBar handle close button.
    InfoBar::ButtonPressed(sender, event);
  }
}

// TranslateInfoBar, AnimationDelegate overrides: ------------------------------

void TranslateInfoBar::AnimationProgressed(const Animation* animation) {
  if (animation == error_animation_.get())
    SchedulePaint();
  else
    InfoBar::AnimationProgressed(animation);
}

// TranslateInfoBar, NotificationObserver overrides: ---------------------------

void TranslateInfoBar::Observe(NotificationType type,
    const NotificationSource& source, const NotificationDetails& details) {
  if (type.value != NotificationType::PAGE_TRANSLATED)
    return;
  TabContents* tab = Source<TabContents>(source).ptr();
  if (tab != GetDelegate()->tab_contents())
    return;
  PageTranslatedDetails* page_translated_details =
      Details<PageTranslatedDetails>(details).ptr();
  UpdateState((page_translated_details->error_type == TranslateErrors::NONE ?
      TranslateInfoBarDelegate::kAfterTranslate :
      TranslateInfoBarDelegate::kTranslateError), false,
      page_translated_details->error_type);
}

// TranslateInfoBar, private: --------------------------------------------------

void TranslateInfoBar::CreateLabels() {
  // Determine text for labels.
  std::vector<size_t> offsets;
  string16 message_text_utf16;
  GetDelegate()->GetMessageText(state_, &message_text_utf16, &offsets,
      &swapped_language_placeholders_);

  std::wstring message_text = UTF16ToWideHack(message_text_utf16);

  // Create label controls.
  std::wstring label_1 = message_text.substr(0, offsets[0]);
  if (label_1_) {
    label_1_->SetText(label_1);
  } else {
    label_1_ = CreateLabel(label_1);
    AddChildView(label_1_);
  }

  std::wstring label_2 = message_text.substr(offsets[0],
      offsets[1] - offsets[0]);
  if (label_2_) {
    label_2_->SetText(label_2);
  } else {
    label_2_ = CreateLabel(label_2);
    AddChildView(label_2_);
  }

  if (offsets.size() == 3) {
    std::wstring label_3 = message_text.substr(offsets[1],
        offsets[2] - offsets[1]);
    if (label_3_) {
      label_3_->SetText(label_3);
    } else {
      label_3_ = CreateLabel(label_3);
      AddChildView(label_3_);
    }
  } else if (label_3_) {
    label_3_->SetText(std::wstring());
  }
}

views::Label* TranslateInfoBar::CreateLabel(const std::wstring& text) {
  views::Label* label = new views::Label(text,
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  label->SetColor(SK_ColorBLACK);
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  return label;
}

views::MenuButton* TranslateInfoBar::CreateMenuButton(int menu_id,
    const std::wstring& text, bool normal_has_border) {
  // Don't use text to instantiate MenuButton because we need to set font before
  // setting text so that the button will resize to fit entire text.
  views::MenuButton* menu_button =
      new views::MenuButton(NULL, std::wstring(), this, true);
  menu_button->SetID(menu_id);
  menu_button->set_border(new TranslateButtonBorder);
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
  menu_button->SetText(text);
  menu_button->ClearMaxTextSize();
  menu_button->SizeToPreferredSize();
  return menu_button;
}

gfx::Point TranslateInfoBar::DetermineMenuPositionAndAlignment(
    views::MenuButton* menu_button, views::Menu2::Alignment* alignment) {
  gfx::Rect lb = menu_button->GetLocalBounds(true);
  gfx::Point menu_position(lb.origin());
  menu_position.Offset(2, lb.height() - 3);
  *alignment = views::Menu2::ALIGN_TOPLEFT;
  if (UILayoutIsRightToLeft()) {
    menu_position.Offset(lb.width() - 4, 0);
  }
  View::ConvertPointToScreen(menu_button, &menu_position);
#if defined(OS_WIN)
  int left_bound = GetSystemMetrics(SM_XVIRTUALSCREEN);
  if (menu_position.x() < left_bound)
    menu_position.set_x(left_bound);
#endif
  return menu_position;
}

void TranslateInfoBar::OnLanguageModified(views::MenuButton* menu_button,
    int new_language_index) {
  // Only proceed if a different language has been selected.
  if (menu_button == original_language_menu_button_) {
    if (new_language_index == GetDelegate()->original_lang_index())
      return;
    GetDelegate()->ModifyOriginalLanguage(new_language_index);
  } else {
    if (new_language_index == GetDelegate()->target_lang_index())
      return;
    GetDelegate()->ModifyTargetLanguage(new_language_index);
  }

  string16 new_language = GetDelegate()->GetDisplayNameForLocale(
      GetDelegate()->GetLocaleFromIndex(new_language_index));
  menu_button->SetText(UTF16ToWideHack(new_language));
  menu_button->ClearMaxTextSize();
  menu_button->SizeToPreferredSize();

  // Clear options menu model so that it'll be created with new language.
  options_menu_model_.reset();

  // Selecting an item from the "from language" menu in the before translate
  // phase shouldn't trigger translation - http://crbug.com/36666
  if (state_ == TranslateInfoBarDelegate::kAfterTranslate) {
    GetDelegate()->Translate();
    UpdateState(GetDelegate()->state(), GetDelegate()->translation_pending(),
                GetDelegate()->error_type());
  }

  Layout();
  SchedulePaint();
}

void TranslateInfoBar::FadeBackground(gfx::Canvas* canvas,
    double animation_value, TranslateInfoBarDelegate::TranslateState state) {
  // Draw background into an offscreen buffer with alpha value per animation
  // value, then blend it back into the current canvas.
  canvas->saveLayerAlpha(NULL, static_cast<int>(animation_value * 255),
      SkCanvas::kARGB_NoClipLayer_SaveFlag);
  canvas->drawARGB(0, 255, 255, 255, SkXfermode::kClear_Mode);
  GetBackground(state)->Paint(canvas, this);
  canvas->restore();
}

inline TranslateInfoBarDelegate* TranslateInfoBar::GetDelegate() const {
  return static_cast<TranslateInfoBarDelegate*>(delegate());
}

inline InfoBarBackground* TranslateInfoBar::GetBackground(
    TranslateInfoBarDelegate::TranslateState state) const {
  return (state == TranslateInfoBarDelegate::kTranslateError ?
      error_background_.get() : normal_background_.get());
}

inline int TranslateInfoBar::GetSpacingAfterFirstLanguageButton() const {
  return (state_ == TranslateInfoBarDelegate::kAfterTranslate ?
      kButtonInLabelSpacing : 10);
}

// TranslateInfoBarDelegate, InfoBarDelegate overrides: ------------------

InfoBar* TranslateInfoBarDelegate::CreateInfoBar() {
  return new TranslateInfoBar(this);
}
