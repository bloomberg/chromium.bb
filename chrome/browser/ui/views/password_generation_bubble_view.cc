// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/password_generation_bubble_view.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/autofill/core/browser/password_generator.h"
#include "components/autofill/core/common/password_generation_util.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "content/public/browser/render_view_host.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/layout_constants.h"
#include "url/gurl.h"

namespace {
// Constants for PasswordGenerationBubbleView.
const int kBubbleMargin = 9;
const int kButtonHorizontalSpacing = 4;
const int kButtonWidth = 65;
const int kDefaultTextFieldChars = 18;
const int kTitleLabelVerticalOffset = -1;
const int kVerticalPadding = 8;

// Constants for Text fieldWrapper.
const int kTextfieldHorizontalPadding = 2;
const int kTextfieldVerticalPadding = 3;
const int kWrapperBorderSize = 1;

// This class handles layout so that it looks like a Textfield and ImageButton
// are part of one logical textfield with the button on the right side of the
// field. It also assumes that the textfield is already sized appropriately
// and will alter the image size to fit.
class TextfieldWrapper : public views::View {
 public:
  TextfieldWrapper(views::Textfield* textfield,
                   views::ImageButton* image_button);
  virtual ~TextfieldWrapper();

  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() const OVERRIDE;

 private:
  gfx::Size GetImageSize() const;

  views::Textfield* textfield_;
  views::ImageButton* image_button_;
};

TextfieldWrapper::TextfieldWrapper(views::Textfield* textfield,
                                   views::ImageButton* image_button)
    : textfield_(textfield),
      image_button_(image_button) {
  SetBorder(views::Border::CreateSolidBorder(kWrapperBorderSize, SK_ColorGRAY));

  AddChildView(textfield_);
  AddChildView(image_button);
}

TextfieldWrapper::~TextfieldWrapper() {}

void TextfieldWrapper::Layout() {
  // Add some spacing between the textfield and the border.
  textfield_->SetPosition(gfx::Point(kTextfieldHorizontalPadding,
                                     kTextfieldVerticalPadding));
  textfield_->SizeToPreferredSize();

  // Button should be offset one pixel from the end of the textfield so that
  // there is no overlap. It is also displaced down by the size of the border
  // so it doesn't overlap with it either.
  int button_x = (textfield_->GetPreferredSize().width() +
                  kTextfieldHorizontalPadding + 1);
  image_button_->SetPosition(gfx::Point(button_x,
                                        kWrapperBorderSize));

  // Make sure that the image is centered after cropping.
  image_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                   views::ImageButton::ALIGN_MIDDLE);

  image_button_->SetSize(GetImageSize());
}

gfx::Size TextfieldWrapper::GetPreferredSize() const {
  int width = (textfield_->GetPreferredSize().width() +
               GetImageSize().width() +
               kTextfieldHorizontalPadding * 3);
  int height = (textfield_->GetPreferredSize().height() +
                kTextfieldVerticalPadding * 2);

  return gfx::Size(width, height);
}

gfx::Size TextfieldWrapper::GetImageSize() const {
  // The image is sized so that it fills the space between the borders
  // completely.
  int size = (textfield_->GetPreferredSize().height() +
              (kTextfieldVerticalPadding - kWrapperBorderSize) * 2);
  return gfx::Size(size, size);
}
}  // namespace

PasswordGenerationBubbleView::PasswordGenerationBubbleView(
    const autofill::PasswordForm& form,
    const gfx::Rect& anchor_rect,
    views::View* anchor_view,
    content::RenderViewHost* render_view_host,
    password_manager::PasswordManager* password_manager,
    autofill::PasswordGenerator* password_generator,
    ui::ThemeProvider* theme_provider)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
      title_label_(NULL),
      accept_button_(NULL),
      textfield_(NULL),
      regenerate_button_(NULL),
      textfield_wrapper_(NULL),
      form_(form),
      anchor_rect_(anchor_rect),
      render_view_host_(render_view_host),
      password_manager_(password_manager),
      password_generator_(password_generator),
      theme_provider_(theme_provider) {}

PasswordGenerationBubbleView::~PasswordGenerationBubbleView() {}

void PasswordGenerationBubbleView::Init() {
  set_margins(gfx::Insets(kBubbleMargin, kBubbleMargin,
                          kBubbleMargin, kBubbleMargin));

  // TODO(gcasto): Localize text after we have finalized the UI.
  // crbug.com/118062.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_BUBBLE_TITLE),
      rb.GetFontList(ui::ResourceBundle::MediumFont));
  AddChildView(title_label_);

  regenerate_button_ = new views::ImageButton(this);
  regenerate_button_->SetImage(
      views::CustomButton::STATE_NORMAL,
      theme_provider_->GetImageSkiaNamed(IDR_RELOAD_DIMMED));
  regenerate_button_->SetImage(
      views::CustomButton::STATE_HOVERED,
      theme_provider_->GetImageSkiaNamed(IDR_RELOAD));
  regenerate_button_->SetImage(
      views::CustomButton::STATE_PRESSED,
      theme_provider_->GetImageSkiaNamed(IDR_RELOAD));

  textfield_ = new views::Textfield();
  textfield_->set_default_width_in_chars(kDefaultTextFieldChars);
  textfield_->SetText(base::ASCIIToUTF16(password_generator_->Generate()));
  textfield_->set_controller(this);

  textfield_wrapper_ = new TextfieldWrapper(textfield_,
                                            regenerate_button_);
  AddChildView(textfield_wrapper_);

  accept_button_ = new views::LabelButton(
      this,
      l10n_util::GetStringUTF16(IDS_PASSWORD_GENERATION_BUTTON_TEXT));
  accept_button_->SetStyle(views::Button::STYLE_BUTTON);
  AddChildView(accept_button_);
}

void PasswordGenerationBubbleView::Layout() {
  // We have the title label shifted up to make the borders look more uniform.
  title_label_->SetPosition(gfx::Point(0, kTitleLabelVerticalOffset));
  title_label_->SizeToPreferredSize();

  int y = title_label_->GetPreferredSize().height() + kVerticalPadding;

  textfield_wrapper_->SetPosition(gfx::Point(0, y));
  textfield_wrapper_->SizeToPreferredSize();

  int button_x = (textfield_wrapper_->GetPreferredSize().width() +
                  kButtonHorizontalSpacing);
  accept_button_->SetBounds(
      button_x,
      y - kWrapperBorderSize,
      kButtonWidth,
      textfield_wrapper_->GetPreferredSize().height() + kWrapperBorderSize * 2);
}

gfx::Size PasswordGenerationBubbleView::GetPreferredSize() const {
  int width = (textfield_wrapper_->GetPreferredSize().width() +
               kButtonHorizontalSpacing +
               kButtonWidth - 1);
  int height = (title_label_->GetPreferredSize().height() +
                textfield_wrapper_->GetPreferredSize().height() +
                kVerticalPadding);
  return gfx::Size(width, height);
}

gfx::Rect PasswordGenerationBubbleView::GetAnchorRect() const {
  return anchor_rect_;
}

void PasswordGenerationBubbleView::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  if (sender == accept_button_) {
    render_view_host_->Send(new AutofillMsg_GeneratedPasswordAccepted(
        render_view_host_->GetRoutingID(), textfield_->text()));
    password_manager_->SetFormHasGeneratedPassword(form_);
    actions_.password_accepted = true;
    GetWidget()->Close();
  }
  if (sender == regenerate_button_) {
    textfield_->SetText(
        base::ASCIIToUTF16(password_generator_->Generate()));
    actions_.password_regenerated = true;
  }
}

void PasswordGenerationBubbleView::ContentsChanged(
    views::Textfield* sender,
    const base::string16& contents) {
  actions_.password_edited = true;
}

bool PasswordGenerationBubbleView::HandleKeyEvent(
    views::Textfield* sender,
    const ui::KeyEvent& key_event) {
  return false;
}

views::View* PasswordGenerationBubbleView::GetInitiallyFocusedView() {
  return textfield_;
}

void PasswordGenerationBubbleView::WindowClosing() {
  autofill::password_generation::LogUserActions(actions_);
}
