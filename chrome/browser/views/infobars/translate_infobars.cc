// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/infobars/translate_infobars.h"

#include <algorithm>
#include <vector>

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/common/notification_service.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/translate/languages_menu_model.h"
#include "chrome/browser/translate/options_menu_model.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/button/text_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/message_box_view.h"

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
    : TextButton(listener, l10n_util::GetString(label_id)) {
  set_border(new TranslateButtonBorder);
  SetNormalHasBorder(true);  // Normal button state has border.
  SetAnimationDuration(0);  // Disable animation during state change.
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
      label_1_(NULL),
      label_2_(NULL),
      label_3_(NULL),
      translating_label_(NULL),
      accept_button_(NULL),
      deny_button_(NULL),
      target_language_menu_button_(NULL),
      swapped_language_placeholders_(false) {
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
      UTF16ToWideHack(language_name));
  AddChildView(original_language_menu_button_);

  // Create options menu button.
  options_menu_button_ = CreateMenuButton(kMenuIDOptions,
      l10n_util::GetString(IDS_TRANSLATE_INFOBAR_OPTIONS));
  AddChildView(options_menu_button_);

  // Create state-dependent controls.
  UpdateState(GetDelegate()->state());

  // Register for PAGE_TRANSLATED notification.
  notification_registrar_.Add(this, NotificationType::PAGE_TRANSLATED,
      Source<TabContents>(GetDelegate()->tab_contents()));
}

TranslateInfoBar::~TranslateInfoBar() {
}

void TranslateInfoBar::UpdateState(
    TranslateInfoBarDelegate::TranslateState new_state) {
  // Create and initialize state-dependent controls if necessary.
  switch (new_state) {
    case TranslateInfoBarDelegate::kAfterTranslate:
      CreateLabels();
      if (!target_language_menu_button_) {
        string16 language_name =
            GetDelegate()->GetDisplayNameForLocale(
                GetDelegate()->target_lang_code());
        target_language_menu_button_ = CreateMenuButton(kMenuIDTargetLanguage,
            UTF16ToWideHack(language_name));
        AddChildView(target_language_menu_button_);
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

    case TranslateInfoBarDelegate::kTranslating:
      if (!label_1_)
        CreateLabels();
      if (!translating_label_) {
        translating_label_ = new views::Label(
            l10n_util::GetString(IDS_TRANSLATE_INFOBAR_TRANSLATING));
        translating_label_->SetColor(SK_ColorBLACK);
        translating_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
        AddChildView(translating_label_);
      }
      break;

    default:
      NOTREACHED() << "Invalid translate state change";
      break;
  }

  // Determine visibility of state-dependent controls.
  if (accept_button_)
    accept_button_->SetVisible(
        new_state == TranslateInfoBarDelegate::kBeforeTranslate);
  if (deny_button_)
    deny_button_->SetVisible(
        new_state == TranslateInfoBarDelegate::kBeforeTranslate);
  if (target_language_menu_button_)
    target_language_menu_button_->SetVisible(
        new_state == TranslateInfoBarDelegate::kAfterTranslate);
  if (translating_label_)
    translating_label_->SetVisible(
        new_state == TranslateInfoBarDelegate::kTranslating);

  // Trigger layout and repaint.
  Layout();
  SchedulePaint();

  // Clear options menu model so that it'll be created for new state.
  options_menu_model_.reset();
}

// TranslateInfoBar, views::View overrides: ------------------------------------

void TranslateInfoBar::Layout() {
  // Layout the close button.
  InfoBar::Layout();

  // Layout the icon on left of bar.
  gfx::Size icon_ps = icon_->GetPreferredSize();
  icon_->SetBounds(InfoBar::kHorizontalPadding, InfoBar::OffsetY(this, icon_ps),
      icon_ps.width(), icon_ps.height());

  // Layout the options menu button on right of bar.
  int available_width = InfoBar::GetAvailableWidth();
  gfx::Size options_ps = options_menu_button_->GetPreferredSize();
  options_menu_button_->SetBounds(available_width - options_ps.width(),
      OffsetY(this, options_ps), options_ps.width(), options_ps.height());

  TranslateInfoBarDelegate::TranslateState state = GetDelegate()->state();

  // Layout the controls between icon and options i.e. labels, original language
  // menu button, and if available, target language menu button.
  views::MenuButton* button1 = original_language_menu_button_;
  views::MenuButton* button2 =
      (state == TranslateInfoBarDelegate::kAfterTranslate ?
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
  gfx::Size translating_ps;
  if (label_3_)
    label3_ps = label_3_->GetPreferredSize();
  if (state == TranslateInfoBarDelegate::kTranslating)
    translating_ps = translating_label_->GetPreferredSize();
  int text1_width = label1_ps.width();
  int text2_width = label2_ps.width();
  int text3_width = label3_ps.width();
  int translating_width = translating_ps.width();
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

  // If second language menu button is available, place it after label_2.
  if (button2) {
    gfx::Size button2_ps = button2->GetPreferredSize();
    button2->SetBounds(label_2_->bounds().right() +
        InfoBar::kButtonInLabelSpacing, OffsetY(this, button2_ps),
        button2_ps.width(), button2_ps.height());

    // If label_3 is available, place it after second language menu button.
    if (label_3_) {
      gfx::Size label3_ps = label_3_->GetPreferredSize();
      label_3_->SetBounds(button2->bounds().right() +
          InfoBar::kButtonInLabelSpacing, InfoBar::OffsetY(this, label3_ps),
          text3_width, label3_ps.height());
    }
  }

  // If translate state is kBeforeTranslate, layout accept and deny butons.
  if (state == TranslateInfoBarDelegate::kBeforeTranslate) {
    gfx::Size accept_ps = accept_button_->GetPreferredSize();
    accept_button_->SetBounds(
        (label_3_ ? label_3_->bounds().right() : label_2_->bounds().right()) +
        InfoBar::kEndOfLabelSpacing,
        OffsetY(this, accept_ps), accept_ps.width(), accept_ps.height());
    gfx::Size deny_ps = deny_button_->GetPreferredSize();
    deny_button_->SetBounds(
        accept_button_->bounds().right() + InfoBar::kButtonButtonSpacing,
        OffsetY(this, deny_ps), deny_ps.width(), deny_ps.height());
  } else if (state == TranslateInfoBarDelegate::kTranslating) {
    // If translate state is kTranslating, layout "Translating..." label.
    translating_label_->SetBounds(
        label_2_->bounds().right() + InfoBar::kEndOfLabelSpacing,
        InfoBar::OffsetY(this, translating_ps),
        translating_width, translating_ps.height());
  }
}

// TranslateInfoBar, InfoBar overrides: ----------------------------------------

int TranslateInfoBar::GetAvailableWidth() const {
  gfx::Size icon_ps = icon_->GetPreferredSize();
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
  int available_width = (InfoBar::GetAvailableWidth() -
      options_ps.width() - options_spacing -
      language_ps.width() - language_spacing -
      icon_->bounds().right() - InfoBar::kIconLabelSpacing);
  TranslateInfoBarDelegate::TranslateState state = GetDelegate()->state();
  if (state == TranslateInfoBarDelegate::kBeforeTranslate) {
    gfx::Size accept_ps = accept_button_->GetPreferredSize();
    gfx::Size deny_ps = deny_button_->GetPreferredSize();
    available_width -= accept_ps.width() + InfoBar::kEndOfLabelSpacing +
        deny_ps.width() + InfoBar::kButtonButtonSpacing;
  } else if (state == TranslateInfoBarDelegate::kAfterTranslate) {
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
      if (GetDelegate()->state() != TranslateInfoBarDelegate::kTranslating) {
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
  return true;
}

bool TranslateInfoBar::GetAcceleratorForCommandId(int command_id,
    menus::Accelerator* accelerator) {
  return false;
}

void TranslateInfoBar::ExecuteCommand(int command_id) {
  if (command_id >= IDC_TRANSLATE_TARGET_LANGUAGE_BASE) {
    OnLanguageModified(target_language_menu_button_,
        command_id - IDC_TRANSLATE_TARGET_LANGUAGE_BASE);
  } else if (command_id >= IDC_TRANSLATE_ORIGINAL_LANGUAGE_BASE) {
    OnLanguageModified(original_language_menu_button_,
        command_id - IDC_TRANSLATE_ORIGINAL_LANGUAGE_BASE);
  } else {
    switch (command_id) {
      case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG:
        GetDelegate()->ToggleLanguageBlacklist();
        break;

      case IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_SITE:
        GetDelegate()->ToggleSiteBlacklist();
        break;

      case IDC_TRANSLATE_OPTIONS_ALWAYS:
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
  if (sender == accept_button_) {
    GetDelegate()->Translate();
    UpdateState(GetDelegate()->state());
  } else if (sender == deny_button_) {
    GetDelegate()->TranslationDeclined();
    RemoveInfoBar();
  } else {  // Let base InfoBar handle close button.
    InfoBar::ButtonPressed(sender, event);
  }
}

// TranslateInfoBar, NotificationObserver overrides: ---------------------------

void TranslateInfoBar::Observe(NotificationType type,
    const NotificationSource& source, const NotificationDetails& details) {
  if (type.value != NotificationType::PAGE_TRANSLATED)
    return;
  TabContents* tab = Source<TabContents>(source).ptr();
  if (tab != GetDelegate()->tab_contents())
    return;
  if (!target_language_menu_button_ ||
      !target_language_menu_button_->IsVisible())
    UpdateState(TranslateInfoBarDelegate::kAfterTranslate);
}

// TranslateInfoBar, private: --------------------------------------------------

void TranslateInfoBar::CreateLabels() {
  // Determine text for labels.
  std::vector<size_t> offsets;
  string16 message_text_utf16;
  GetDelegate()->GetMessageText(&message_text_utf16, &offsets,
      &swapped_language_placeholders_);

  std::wstring message_text = UTF16ToWideHack(message_text_utf16);

  // Create label controls.
  const gfx::Font& font = ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::MediumFont);
  std::wstring label_1 = message_text.substr(0, offsets[0]);
  if (label_1_) {
    label_1_->SetText(label_1);
  } else {
    label_1_ = new views::Label(label_1, font);
    label_1_->SetColor(SK_ColorBLACK);
    label_1_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(label_1_);
  }

  std::wstring label_2 = message_text.substr(offsets[0],
      offsets[1] - offsets[0]);
  if (label_2_) {
    label_2_->SetText(label_2);
  } else {
    label_2_ = new views::Label(label_2, font);
    label_2_->SetColor(SK_ColorBLACK);
    label_2_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(label_2_);
  }

  if (offsets.size() == 3) {
    std::wstring label_3 = message_text.substr(offsets[1],
        offsets[2] - offsets[1]);
    if (label_3_) {
      label_3_->SetText(label_3);
    } else {
      label_3_ = new views::Label(label_3, font);
      label_3_->SetColor(SK_ColorBLACK);
      label_3_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      AddChildView(label_3_);
    }
  }
}

views::MenuButton* TranslateInfoBar::CreateMenuButton(int menu_id,
    const std::wstring& label) {
  views::MenuButton* menu_button =
      new views::MenuButton(NULL, label, this, true);
  menu_button->SetID(menu_id);
  menu_button->set_border(new TranslateButtonBorder);
  menu_button->SetNormalHasBorder(true);
  menu_button->SetAnimationDuration(0);
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
  Layout();
  SchedulePaint();
  // Clear options menu model so that it'll be created with new language.
  options_menu_model_.reset();
  // Selecting an item from the "from language" menu in the before translate
  // phase shouldn't trigger translation - http://crbug.com/36666
  if (GetDelegate()->state() == TranslateInfoBarDelegate::kAfterTranslate)
    GetDelegate()->Translate();
}

inline TranslateInfoBarDelegate* TranslateInfoBar::GetDelegate() const {
  return static_cast<TranslateInfoBarDelegate*>(delegate());
}

inline int TranslateInfoBar::GetSpacingAfterFirstLanguageButton() const {
  return (GetDelegate()->state() == TranslateInfoBarDelegate::kBeforeTranslate ?
      10 : kButtonInLabelSpacing);
}

// TranslateInfoBarDelegate, InfoBarDelegate overrides: ------------------

InfoBar* TranslateInfoBarDelegate::CreateInfoBar() {
  return new TranslateInfoBar(this);
}
