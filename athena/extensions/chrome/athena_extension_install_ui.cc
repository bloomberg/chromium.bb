// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/chrome/athena_extension_install_ui.h"

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/chromium_strings.h"
#include "extensions/browser/install/crx_installer_error.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace athena {

namespace {

// Dialog delegate which displays a message and an 'OK' button.
class MessageDialogDelegate : public views::DialogDelegateView {
 public:
  explicit MessageDialogDelegate(const base::string16& message)
      : message_(message) {}

  ~MessageDialogDelegate() override {}

  // views::DialogDelegateView:
  base::string16 GetWindowTitle() const override { return message_; }

  int GetDialogButtons() const override { return ui::DIALOG_BUTTON_OK; }

 private:
  base::string16 message_;

  DISALLOW_COPY_AND_ASSIGN(MessageDialogDelegate);
};

}  // namespace

AthenaExtensionInstallUI::AthenaExtensionInstallUI()
    : skip_post_install_ui_(false) {
}

AthenaExtensionInstallUI::~AthenaExtensionInstallUI() {
}

void AthenaExtensionInstallUI::OnInstallSuccess(
    const extensions::Extension* extension,
    const SkBitmap* icon) {
  if (skip_post_install_ui_)
    return;

  base::string16 extension_name = base::UTF8ToUTF16(extension->name());
  base::i18n::AdjustStringForLocaleDirection(&extension_name);
  base::string16 message = l10n_util::GetStringFUTF16(
      IDS_EXTENSION_INSTALLED_HEADING, extension_name);
  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      new MessageDialogDelegate(message), nullptr, nullptr);
  widget->Show();
}

void AthenaExtensionInstallUI::OnInstallFailure(
    const extensions::CrxInstallerError& error) {
  if (skip_post_install_ui_)
    return;

  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      new MessageDialogDelegate(error.message()), nullptr, nullptr);
  widget->Show();
}

void AthenaExtensionInstallUI::SetUseAppInstalledBubble(bool use_bubble) {
}

void AthenaExtensionInstallUI::OpenAppInstalledUI(const std::string& app_id) {
  NOTREACHED();
}

void AthenaExtensionInstallUI::SetSkipPostInstallUI(bool skip_ui) {
  skip_post_install_ui_ = skip_ui;
}

gfx::NativeWindow AthenaExtensionInstallUI::GetDefaultInstallDialogParent() {
  return nullptr;
}

}  // namespace athena
