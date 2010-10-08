// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/captcha_view.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/login/image_downloader.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
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

CaptchaView::CaptchaView(const GURL& captcha_url)
    : delegate_(NULL),
      captcha_url_(captcha_url),
      captcha_image_(NULL),
      captcha_textfield_(NULL) {
}

bool CaptchaView::Accept() {
  if (delegate_)
    delegate_->OnCaptchaEntered(UTF16ToUTF8(captcha_textfield_->text()));
  return true;
}

std::wstring CaptchaView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_LOGIN_CAPTCHA_DIALOG_TITLE);
}

gfx::Size CaptchaView::GetPreferredSize() {
  // TODO(nkostylev): Once UI is finalized, create locale settings.
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_CAPTCHA_INPUT_DIALOG_WIDTH_CHARS,
      IDS_CAPTCHA_INPUT_DIALOG_HEIGHT_LINES));
}

void CaptchaView::ViewHierarchyChanged(bool is_add,
                                       views::View* parent,
                                       views::View* child) {
  // Can't init before we're inserted into a Container, because we require
  // a HWND to parent native child controls to.
  if (is_add && child == this)
    Init();
}

bool CaptchaView::HandleKeystroke(views::Textfield* sender,
    const views::Textfield::Keystroke& keystroke) {
  if (sender == captcha_textfield_ &&
      keystroke.GetKeyboardCode() == app::VKEY_RETURN) {
    GetDialogClientView()->AcceptWindow();
  }
  return false;
}

void CaptchaView::OnImageDecoded(const SkBitmap& decoded_image) {
  captcha_image_->SetImage(decoded_image);
  SchedulePaint();
  Layout();
}

void CaptchaView::Init() {
  views::GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  int column_view_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_view_set_id);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, column_view_set_id);
  Label* label =
      new views::Label(l10n_util::GetString(IDS_LOGIN_CAPTCHA_INSTRUCTIONS));
  label->SetMultiLine(true);
  layout->AddView(label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, column_view_set_id);
  captcha_image_ = new views::ImageView();
  layout->AddView(captcha_image_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, column_view_set_id);
  captcha_textfield_ = new views::Textfield(
      views::Textfield::STYLE_DEFAULT);
  captcha_textfield_->SetController(this);
  layout->AddView(captcha_textfield_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, column_view_set_id);
  label = new views::Label(
      l10n_util::GetString(IDS_SYNC_GAIA_CAPTCHA_CASE_INSENSITIVE_TIP));
  label->SetMultiLine(true);
  layout->AddView(label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  captcha_textfield_->RequestFocus();

  // ImageDownloader will disable itself once URL is fetched.
  new ImageDownloader(this, GURL(captcha_url_), std::string());
}

}  // namespace chromeos
