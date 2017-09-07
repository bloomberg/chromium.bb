// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/safe_browsing/password_reuse_modal_warning_dialog.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/browser/ui/views/harmony/chrome_typography.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"

namespace safe_browsing {

#if !defined(OS_MACOSX) || BUILDFLAG(MAC_VIEWS_BROWSER)
void ShowPasswordReuseModalWarningDialog(
    content::WebContents* web_contents,
    ChromePasswordProtectionService* service,
    OnWarningDone done_callback) {
  PasswordReuseModalWarningDialog* dialog = new PasswordReuseModalWarningDialog(
      web_contents, service, std::move(done_callback));
  constrained_window::CreateBrowserModalDialogViews(
      dialog, web_contents->GetTopLevelNativeWindow())
      ->Show();
}
#endif  // !OS_MACOSX || MAC_VIEWS_BROWSER

PasswordReuseModalWarningDialog::PasswordReuseModalWarningDialog(
    content::WebContents* web_contents,
    ChromePasswordProtectionService* service,
    OnWarningDone done_callback)
    : show_softer_warning_(
          PasswordProtectionService::ShouldShowSofterWarning()),
      done_callback_(std::move(done_callback)),
      service_(service),
      url_(web_contents->GetLastCommittedURL()) {
  // |service| maybe NULL in tests.
  if (service_)
    service_->AddObserver(this);

  set_margins(ChromeLayoutProvider::Get()->GetInsetsMetric(
      views::INSETS_DIALOG_CONTENTS));

  // TODO(jialiul): Dialog message should align with title.
  views::GridLayout* layout = views::GridLayout::CreateAndInstall(this);
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::FIXED, 400, 0);

  views::Label* message_body_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_PAGE_INFO_CHANGE_PASSWORD_DETAILS));
  message_body_label->SetMultiLine(true);
  message_body_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  layout->StartRow(0, 0);
  layout->AddView(message_body_label);
}

PasswordReuseModalWarningDialog::~PasswordReuseModalWarningDialog() {
  if (service_)
    service_->RemoveObserver(this);
}

ui::ModalType PasswordReuseModalWarningDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 PasswordReuseModalWarningDialog::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      show_softer_warning_ ? IDS_PAGE_INFO_CHANGE_PASSWORD_SUMMARY_SOFTER
                           : IDS_PAGE_INFO_CHANGE_PASSWORD_SUMMARY);
}

bool PasswordReuseModalWarningDialog::ShouldShowCloseButton() const {
  return false;
}

gfx::ImageSkia PasswordReuseModalWarningDialog::GetWindowIcon() {
  constexpr int kIconSize = 24;
  return show_softer_warning_
             ? gfx::CreateVectorIcon(kSecurityIcon, kIconSize,
                                     gfx::kChromeIconGrey)
             : gfx::CreateVectorIcon(vector_icons::kWarningIcon, kIconSize,
                                     gfx::kGoogleRed700);
}

bool PasswordReuseModalWarningDialog::ShouldShowWindowIcon() const {
  return true;
}

bool PasswordReuseModalWarningDialog::Cancel() {
  std::move(done_callback_).Run(PasswordProtectionService::IGNORE_WARNING);
  return true;
}

bool PasswordReuseModalWarningDialog::Accept() {
  std::move(done_callback_).Run(PasswordProtectionService::CHANGE_PASSWORD);
  return true;
}

bool PasswordReuseModalWarningDialog::Close() {
  if (done_callback_)
    std::move(done_callback_).Run(PasswordProtectionService::CLOSE);
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

void PasswordReuseModalWarningDialog::OnStartingGaiaPasswordChange() {
  GetWidget()->Close();
}

void PasswordReuseModalWarningDialog::OnGaiaPasswordChanged() {
  GetWidget()->Close();
}

void PasswordReuseModalWarningDialog::OnMarkingSiteAsLegitimate(
    const GURL& url) {
  if (url_.GetWithEmptyPath() == url.GetWithEmptyPath())
    GetWidget()->Close();
}

}  // namespace safe_browsing
