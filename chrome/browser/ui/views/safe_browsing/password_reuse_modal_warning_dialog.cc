// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/password_reuse_modal_warning_dialog.h"

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"

namespace safe_browsing {

constexpr int kIconSize = 20;

void ShowPasswordReuseModalWarningDialog(
    content::WebContents* web_contents,
    ChromePasswordProtectionService* service,
    ReusedPasswordType password_type,
    OnWarningDone done_callback) {
  PasswordReuseModalWarningDialog* dialog = new PasswordReuseModalWarningDialog(
      web_contents, service, password_type, std::move(done_callback));
  constrained_window::CreateBrowserModalDialogViews(
      dialog, web_contents->GetTopLevelNativeWindow())
      ->Show();
}

PasswordReuseModalWarningDialog::PasswordReuseModalWarningDialog(
    content::WebContents* web_contents,
    ChromePasswordProtectionService* service,
    ReusedPasswordType password_type,
    OnWarningDone done_callback)
    : content::WebContentsObserver(web_contents),
      done_callback_(std::move(done_callback)),
      service_(service),
      url_(web_contents->GetLastCommittedURL()) {
  // |service| maybe NULL in tests.
  if (service_)
    service_->AddObserver(this);

  const ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();
  set_margins(
      provider->GetDialogInsetsForContentType(views::TEXT, views::TEXT));
  SetLayoutManager(std::make_unique<views::FillLayout>());

  views::Label* message_body_label = new views::Label(
      service_
          ? service_->GetWarningDetailText(password_type)
          : l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS));
  message_body_label->SetMultiLine(true);
  message_body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message_body_label->SetHandlesTooltips(false);
  // Makes message label align with title label.
  int horizontal_adjustment =
      kIconSize +
      provider->GetDistanceMetric(DISTANCE_UNRELATED_CONTROL_HORIZONTAL);
  if (base::i18n::IsRTL()) {
    message_body_label->SetBorder(
        views::CreateEmptyBorder(0, 0, 0, horizontal_adjustment));
  } else {
    message_body_label->SetBorder(
        views::CreateEmptyBorder(0, horizontal_adjustment, 0, 0));
  }
  AddChildView(message_body_label);
}

PasswordReuseModalWarningDialog::~PasswordReuseModalWarningDialog() {
  if (service_)
    service_->RemoveObserver(this);
}

gfx::Size PasswordReuseModalWarningDialog::CalculatePreferredSize() const {
  constexpr int kDialogWidth = 400;
  return gfx::Size(kDialogWidth, GetHeightForWidth(kDialogWidth));
}

ui::ModalType PasswordReuseModalWarningDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 PasswordReuseModalWarningDialog::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_SUMMARY);
}

bool PasswordReuseModalWarningDialog::ShouldShowCloseButton() const {
  return false;
}

gfx::ImageSkia PasswordReuseModalWarningDialog::GetWindowIcon() {
  return gfx::CreateVectorIcon(kSecurityIcon, kIconSize, gfx::kChromeIconGrey);
}

bool PasswordReuseModalWarningDialog::ShouldShowWindowIcon() const {
  return true;
}

bool PasswordReuseModalWarningDialog::Cancel() {
  std::move(done_callback_).Run(WarningAction::IGNORE_WARNING);
  return true;
}

bool PasswordReuseModalWarningDialog::Accept() {
  std::move(done_callback_).Run(WarningAction::CHANGE_PASSWORD);
  return true;
}

bool PasswordReuseModalWarningDialog::Close() {
  if (done_callback_)
    std::move(done_callback_).Run(WarningAction::CLOSE);
  return true;
}

int PasswordReuseModalWarningDialog::GetDefaultDialogButton() const {
  return ui::DIALOG_BUTTON_OK;
}

base::string16 PasswordReuseModalWarningDialog::GetDialogButtonLabel(
    ui::DialogButton button) const {
  switch (button) {
    case ui::DIALOG_BUTTON_OK:
      return l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_BUTTON);
    case ui::DIALOG_BUTTON_CANCEL:
      return l10n_util::GetStringUTF16(
          IDS_PAGE_INFO_IGNORE_PASSWORD_WARNING_BUTTON);
    default:
      NOTREACHED();
  }
  return base::string16();
}

void PasswordReuseModalWarningDialog::OnGaiaPasswordChanged() {
  GetWidget()->Close();
}

void PasswordReuseModalWarningDialog::OnMarkingSiteAsLegitimate(
    const GURL& url) {
  if (url_.GetWithEmptyPath() == url.GetWithEmptyPath())
    GetWidget()->Close();
}

void PasswordReuseModalWarningDialog::InvokeActionForTesting(
    WarningAction action) {
  switch (action) {
    case WarningAction::CHANGE_PASSWORD:
      Accept();
      break;
    case WarningAction::IGNORE_WARNING:
      Cancel();
      break;
    case WarningAction::CLOSE:
      Close();
      break;
    default:
      NOTREACHED();
      break;
  }
}

WarningUIType PasswordReuseModalWarningDialog::GetObserverType() {
  return WarningUIType::MODAL_DIALOG;
}

void PasswordReuseModalWarningDialog::WebContentsDestroyed() {
  GetWidget()->Close();
}

}  // namespace safe_browsing
