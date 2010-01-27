// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/infobars/translate_infobars.h"

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
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

  TextButtonBorder::Paint(view, canvas, *set);
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

// TranslateButtonBorder, protected:--------------------------------------------
bool TranslateTextButton::OnMousePressed(const views::MouseEvent& e) {
  return views::CustomButton::OnMousePressed(e);
}

// TranslateInfoBar, public: ---------------------------------------------------

TranslateInfoBar::TranslateInfoBar(TranslateInfoBarDelegate* delegate,
    bool before_translate, int label_id)
    : InfoBar(delegate),
      before_translate_(before_translate),
      swapped_language_placeholders_(false) {
  icon_ = new views::ImageView;
  SkBitmap* image = delegate->GetIcon();
  if (image)
    icon_->SetImage(image);
  AddChildView(icon_);

  // Set up the labels.
  std::vector<size_t> offsets;
  std::wstring message_text = l10n_util::GetStringF(label_id, std::wstring(),
      std::wstring(), &offsets);
  if (!offsets.empty() && offsets.size() <= 2) {
    // Sort the offsets if necessary.
    if (offsets.size() == 2 && offsets[0] > offsets[1]) {
      size_t offset0 = offsets[0];
      offsets[0] = offsets[1];
      offsets[1] = offset0;
      swapped_language_placeholders_ = true;
    }
    if (offsets[offsets.size() - 1] != message_text.length())
      offsets.push_back(message_text.length());
  } else {
    NOTREACHED() << "Invalid no. of placeholders in label.";
  }

  const gfx::Font& font = ResourceBundle::GetSharedInstance().GetFont(
      ResourceBundle::MediumFont);
  label_1_ = new views::Label(message_text.substr(0, offsets[0]), font);
  label_1_->SetColor(SK_ColorBLACK);
  label_1_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label_1_);

  label_2_ = new views::Label(
      message_text.substr(offsets[0], offsets[1] - offsets[0]), font);
  label_2_->SetColor(SK_ColorBLACK);
  label_2_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label_2_);

  if (offsets.size() == 3) {
    label_3_ = new views::Label(
        message_text.substr(offsets[1], offsets[2] - offsets[1]), font);
    label_3_->SetColor(SK_ColorBLACK);
    label_3_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(label_3_);
  } else {
     label_3_ = NULL;
  }

  original_language_menu_button_ = CreateMenuButton(kMenuIDOriginalLanguage,
      ASCIIToWide(GetDelegate()->original_language()));
  AddChildView(original_language_menu_button_);

  options_menu_button_ = CreateMenuButton(kMenuIDOptions,
      l10n_util::GetString(IDS_TRANSLATE_INFOBAR_OPTIONS));
  AddChildView(options_menu_button_);
}

TranslateInfoBar::~TranslateInfoBar() {

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

  // Layout the controls between icon and options i.e. labels, original language
  // menu button, and if available, target language menu button.
  // We layout target language menu button here (as opposed to in
  // AfterTranslateInfoBar) because the 2 language menu buttons could be swapped
  // for different locales.
  views::MenuButton* button1 = original_language_menu_button_;
  views::MenuButton* button2 = target_language_menu_button();
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
  int total_text_width = text1_width + text2_width + text3_width;
  if (total_text_width > available_text_width) {
    double ratio = static_cast<double>(available_text_width) /
        static_cast<double>(total_text_width);
    text1_width = static_cast<int>(text1_width * ratio);
    text2_width = static_cast<int>(text2_width * ratio);
    text3_width = static_cast<int>(text3_width * ratio);
  }
  label_1_->SetBounds(icon_->bounds().right() + InfoBar::kIconLabelSpacing,
      InfoBar::OffsetY(this, label1_ps), text1_width, label1_ps.height());

  // Place first language menu button after label_1.
  button1->SetBounds(label_1_->bounds().right() + InfoBar::kButtonSpacing,
      OffsetY(this, button1_ps), button1_ps.width(), button1_ps.height());

  // Place label_2 after first language menu button.
  label_2_->SetBounds(button1->bounds().right() + InfoBar::kButtonSpacing,
      InfoBar::OffsetY(this, label2_ps), text2_width, label2_ps.height());

  // If second language menu button is available, place it after label_2.
  if (button2) {
    gfx::Size button2_ps = button2->GetPreferredSize();
    button2->SetBounds(label_2_->bounds().right() + InfoBar::kButtonSpacing,
        OffsetY(this, button2_ps), button2_ps.width(), button2_ps.height());

    if (label_3_) {
      gfx::Size label3_ps = label_3_->GetPreferredSize();
      // Place label_3 after first language menu button.
      label_3_->SetBounds(button2->bounds().right() + InfoBar::kButtonSpacing,
          InfoBar::OffsetY(this, label3_ps), text3_width, label3_ps.height());
    }
  }
}

// TranslateInfoBar, InfoBar overrides: ----------------------------------------

int TranslateInfoBar::GetAvailableWidth() const {
  gfx::Size icon_ps = icon_->GetPreferredSize();
  gfx::Size language_ps = original_language_menu_button_->GetPreferredSize();
  gfx::Size options_ps = options_menu_button_->GetPreferredSize();
  return (InfoBar::GetAvailableWidth() - options_ps.width() -
      language_ps.width() - (InfoBar::kButtonSpacing * 3) -
      icon_ps.width() - InfoBar::kIconLabelSpacing);
}

// TranslateInfoBar, views::ViewMenuDelegate implementation: -------------------

void TranslateInfoBar::RunMenu(views::View* source, const gfx::Point& pt) {
  int menu_id = source->GetID();
  if (menu_id == kMenuIDOptions) {
    if (!options_menu_model_.get()) {
      options_menu_model_.reset(new OptionsMenuModel(this, GetDelegate(),
        before_translate_));
      options_menu_menu_.reset(new views::Menu2(options_menu_model_.get()));
    }
    options_menu_menu_->RunMenuAt(pt,
        (UILayoutIsRightToLeft() ? views::Menu2::ALIGN_TOPLEFT :
             views::Menu2::ALIGN_TOPRIGHT));
  } else if (menu_id == kMenuIDOriginalLanguage) {
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
  } else {
    NOTREACHED() << "Invalid source menu.";
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
  if (command_id >= IDC_TRANSLATE_ORIGINAL_LANGUAGE_BASE) {
    string16 new_language = original_language_menu_model_->GetLabelAt(
        command_id - IDC_TRANSLATE_ORIGINAL_LANGUAGE_BASE);
    OnLanguageModified(original_language_menu_button_, new_language);
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

// TranslateInfoBar, protected: ------------------------------------------------

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

int TranslateInfoBar::GetAvailableX() const {
  return ((label_3_ ? label_3_->bounds().right() : label_2_->bounds().right()) +
      InfoBar::kButtonSpacing);
}

gfx::Point TranslateInfoBar::DetermineMenuPositionAndAlignment(
    views::MenuButton* menu_button, views::Menu2::Alignment* alignment) {
  gfx::Rect lb = menu_button->GetLocalBounds(true);
  gfx::Point menu_position(lb.origin());
  menu_position.Offset(2, lb.height() - 3);
  if (UILayoutIsRightToLeft()) {
    menu_position.Offset(lb.width() - 4, 0);
    *alignment = views::Menu2::ALIGN_TOPRIGHT;
  } else {
    *alignment = views::Menu2::ALIGN_TOPLEFT;
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
    const string16& new_language) {
  // Only proceed if a different language has been selected.
  if (new_language == WideToUTF16(menu_button->text()))
    return;
  std::string ascii_lang(UTF16ToUTF8(new_language));
  if (menu_button == original_language_menu_button_)
    GetDelegate()->ModifyOriginalLanguage(ascii_lang);
  else
    GetDelegate()->ModifyTargetLanguage(ascii_lang);
  menu_button->SetText(UTF16ToWide(new_language));
  menu_button->ClearMaxTextSize();
  menu_button->SizeToPreferredSize();
  Layout();
  SchedulePaint();
  // Clear options menu model so that it'll be created with new language.
  options_menu_model_.reset();
  if (!before_translate_)
    GetDelegate()->Translate();
}

TranslateInfoBarDelegate* TranslateInfoBar::GetDelegate() const {
  return static_cast<TranslateInfoBarDelegate*>(delegate());
}

// BeforeTranslateInfoBar, public: ---------------------------------------------

BeforeTranslateInfoBar::BeforeTranslateInfoBar(
    BeforeTranslateInfoBarDelegate* delegate)
    : TranslateInfoBar(delegate, true, IDS_TRANSLATE_INFOBAR_BEFORE_MESSAGE) {
  accept_button_ = new TranslateTextButton(this, IDS_TRANSLATE_INFOBAR_ACCEPT);
  AddChildView(accept_button_);

  deny_button_ = new TranslateTextButton(this, IDS_TRANSLATE_INFOBAR_DENY);
  AddChildView(deny_button_);
}

BeforeTranslateInfoBar::~BeforeTranslateInfoBar() {
}

// BeforeTranslateInfoBar, InfoBar overrides: ----------------------------------

int BeforeTranslateInfoBar::GetAvailableWidth() const {
  gfx::Size accept_ps = accept_button_->GetPreferredSize();
  gfx::Size deny_ps = deny_button_->GetPreferredSize();
  return (TranslateInfoBar::GetAvailableWidth() - accept_ps.width() -
      deny_ps.width() - (InfoBar::kButtonSpacing * 2));
}

// BeforeTranslateInfoBar, views::View overrides: ------------------------------

void BeforeTranslateInfoBar::Layout() {
  // Layout the icon, options menu, language menu and close buttons.
  TranslateInfoBar::Layout();

  // Layout accept button.
  gfx::Size accept_ps = accept_button_->GetPreferredSize();
  accept_button_->SetBounds(
      TranslateInfoBar::GetAvailableX() + InfoBar::kButtonSpacing,
      OffsetY(this, accept_ps), accept_ps.width(), accept_ps.height());

  // Layout deny button.
  gfx::Size deny_ps = deny_button_->GetPreferredSize();
  deny_button_->SetBounds(
      accept_button_->bounds().right() + InfoBar::kButtonSpacing,
      OffsetY(this, deny_ps), deny_ps.width(), deny_ps.height());
}

// BeforeTranslateInfoBar, views::ButtonListener overrides: --------------------

void BeforeTranslateInfoBar::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == accept_button_) {
    // TODO(kuan): If privacy warning box is needed (awaiting PM/UX decision),
    // implement it.
    GetDelegate()->Translate();
  } else if (sender != deny_button_) {  // Let base InfoBar handle close button.
    InfoBar::ButtonPressed(sender, event);
  }
  RemoveInfoBar();
}

// AfterTranslateInfoBar, public: ----------------------------------------------

AfterTranslateInfoBar::AfterTranslateInfoBar(
    AfterTranslateInfoBarDelegate* delegate)
    : TranslateInfoBar(delegate, false, IDS_TRANSLATE_INFOBAR_AFTER_MESSAGE) {
  target_language_menu_button_ = CreateMenuButton(kMenuIDTargetLanguage,
      ASCIIToWide(GetDelegate()->target_language()));
  AddChildView(target_language_menu_button_);
}

AfterTranslateInfoBar::~AfterTranslateInfoBar() {
}

// AfterTranslateInfoBar, InfoBar overrides: ----------------------------------

int AfterTranslateInfoBar::GetAvailableWidth() const {
  gfx::Size target_ps = target_language_menu_button_->GetPreferredSize();
  return (TranslateInfoBar::GetAvailableWidth() - target_ps.width() -
      InfoBar::kButtonSpacing);
}

// AfterTranslateInfoBar, views::ViewMenuDelegate implementation: --------------

void AfterTranslateInfoBar::RunMenu(views::View* source, const gfx::Point& pt) {
  if (source->GetID() == kMenuIDTargetLanguage) {
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
  } else {
    TranslateInfoBar::RunMenu(source, pt);
  }
}

// AfterTranslateInfoBar, menus::SimpleMenuModel::Delegate implementation: -----

void AfterTranslateInfoBar::ExecuteCommand(int command_id) {
  if (command_id >= IDC_TRANSLATE_TARGET_LANGUAGE_BASE) {
    string16 new_language = target_language_menu_model_->GetLabelAt(
        command_id - IDC_TRANSLATE_TARGET_LANGUAGE_BASE);
    OnLanguageModified(target_language_menu_button_, new_language);
  } else {
    TranslateInfoBar::ExecuteCommand(command_id);
  }
}

// BeforeTranslateInfoBarDelegate, InfoBarDelegate overrides: ------------------

InfoBar* BeforeTranslateInfoBarDelegate::CreateInfoBar() {
  return new BeforeTranslateInfoBar(this);
}

// AfterTranslateInfoBarDelegate, InfoBarDelegate overrides: ------------------

InfoBar* AfterTranslateInfoBarDelegate::CreateInfoBar() {
  return new AfterTranslateInfoBar(this);
}

