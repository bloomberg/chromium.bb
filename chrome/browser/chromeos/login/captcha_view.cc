// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/captcha_view.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/helper.h"
#include "chrome/browser/chromeos/login/image_downloader.h"
#include "chrome/browser/chromeos/login/rounded_rect_painter.h"
#include "chrome/browser/chromeos/login/textfield_with_margin.h"
#include "chrome/browser/chromeos/views/copy_background.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/background.h"
#include "views/controls/button/text_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window.h"

using views::Label;
using views::Textfield;
using views::WidgetGtk;

namespace chromeos {

namespace {

// A Textfield for captcha input, which also sets focus to itself
// when a mouse is clicked on it. This is necessary in screen locker mode
// as mouse events are grabbed in the screen locker.
class CaptchaField : public TextfieldWithMargin {
 public:
  CaptchaField()
      : TextfieldWithMargin() {
  }

  // views::View overrides.
  virtual bool OnMousePressed(const views::MouseEvent& e) {
    RequestFocus();
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CaptchaField);
};

class WideTextButton : public views::TextButton {
 public:
  WideTextButton(views::ButtonListener* listener, const std::wstring& text)
      : TextButton(listener, text) {
    SetFont(font().DeriveFont(kFontSizeCorrectionDelta));
  }

  virtual ~WideTextButton() {}

 private:
  virtual gfx::Size GetPreferredSize() {
    gfx::Size preferred_size = TextButton::GetPreferredSize();
    // Set minimal width.
    if (preferred_size.width() < login::kButtonMinWidth)
      preferred_size.set_width(login::kButtonMinWidth);
    return preferred_size;
  }

  DISALLOW_COPY_AND_ASSIGN(WideTextButton);
};

}  // namespace

CaptchaView::CaptchaView(const GURL& captcha_url, bool is_standalone)
    : delegate_(NULL),
      captcha_url_(captcha_url),
      captcha_image_(NULL),
      captcha_textfield_(NULL),
      is_standalone_(is_standalone),
      ok_button_(NULL) {
}

bool CaptchaView::Accept() {
  if (delegate_)
    delegate_->OnCaptchaEntered(UTF16ToUTF8(captcha_textfield_->text()));
  return true;
}

std::wstring CaptchaView::GetWindowTitle() const {
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_LOGIN_CAPTCHA_DIALOG_TITLE));
}

void CaptchaView::SetCaptchaURL(const GURL& captcha_url) {
  captcha_url_ = captcha_url;
  captcha_textfield_->SetText(string16());
  // ImageDownloader will delete itself once URL is fetched.
  // TODO(nkostylev): Make sure that it works after view is deleted.
  new ImageDownloader(this, GURL(captcha_url_), std::string());
}

gfx::Size CaptchaView::GetPreferredSize() {
  // TODO(nkostylev): Once UI is finalized, create locale settings.
  gfx::Size size = gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_CAPTCHA_INPUT_DIALOG_WIDTH_CHARS,
      IDS_CAPTCHA_INPUT_DIALOG_HEIGHT_LINES));
  if (is_standalone_)
    size.set_height(size.height() + ok_button_->GetPreferredSize().height());

  return size;
}

void CaptchaView::ViewHierarchyChanged(bool is_add,
                                       views::View* parent,
                                       views::View* child) {
  // Can't focus before we're inserted into a Container.
  if (is_add && child == this)
    captcha_textfield_->RequestFocus();
}

bool CaptchaView::HandleKeyEvent(views::Textfield* sender,
                                 const views::KeyEvent& key_event) {
  if (sender == captcha_textfield_ &&
      key_event.GetKeyCode() == ui::VKEY_RETURN) {
    if (is_standalone_) {
      Accept();
    } else {
      GetDialogClientView()->AcceptWindow();
    }
  }
  return false;
}

void CaptchaView::OnImageDecoded(const SkBitmap& decoded_image) {
  captcha_image_->SetImage(decoded_image);
  captcha_textfield_->RequestFocus();
  Layout();
}

void CaptchaView::ButtonPressed(views::Button* sender,
                                const views::Event& event) {
  DCHECK(sender == ok_button_);
  Accept();
}

void CaptchaView::Init() {
  if (is_standalone_) {
    // Use rounded rect background.
    set_border(CreateWizardBorder(&BorderDefinition::kUserBorder));
    views::Painter* painter = CreateWizardPainter(
        &BorderDefinition::kUserBorder);
    set_background(views::Background::CreateBackgroundPainter(true, painter));
  }

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  int column_view_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_view_set_id);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, column_view_set_id);
  Label* label = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_LOGIN_CAPTCHA_INSTRUCTIONS)));
  label->SetMultiLine(true);
  layout->AddView(label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, column_view_set_id);
  captcha_image_ = new views::ImageView();
  layout->AddView(captcha_image_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, column_view_set_id);
  captcha_textfield_ = new CaptchaField();
  captcha_textfield_->SetController(this);
  if (is_standalone_)
    captcha_textfield_->set_background(new CopyBackground(this));
  layout->AddView(captcha_textfield_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, column_view_set_id);
  label = new views::Label(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_SYNC_GAIA_CAPTCHA_CASE_INSENSITIVE_TIP)));
  label->SetMultiLine(true);
  layout->AddView(label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  if (is_standalone_) {
    layout->StartRow(0, column_view_set_id);
    ok_button_ = new WideTextButton(
        this, UTF16ToWide(l10n_util::GetStringUTF16(IDS_OK)));
    ok_button_->set_alignment(views::TextButton::ALIGN_CENTER);
    ok_button_->SetFocusable(true);
    ok_button_->SetNormalHasBorder(true);
    ok_button_->set_animate_on_state_change(false);
    ok_button_->SetEnabledColor(SK_ColorBLACK);
    ok_button_->SetHighlightColor(SK_ColorBLACK);
    ok_button_->SetHoverColor(SK_ColorBLACK);
    layout->AddView(ok_button_, 1, 1,
                    views::GridLayout::CENTER, views::GridLayout::CENTER);
  }

  // ImageDownloader will delete itself once URL is fetched.
  // TODO(nkostylev): Make sure that it works after view is deleted.
  new ImageDownloader(this, GURL(captcha_url_), std::string());
}

}  // namespace chromeos
