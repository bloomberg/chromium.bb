// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/installation_error_infobar_delegate.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

// static
void InstallationErrorInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    const extensions::CrxInstallError& error) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::unique_ptr<ConfirmInfoBarDelegate>(
          new InstallationErrorInfoBarDelegate(error))));
}

InstallationErrorInfoBarDelegate::InstallationErrorInfoBarDelegate(
    const extensions::CrxInstallError& error)
    : ConfirmInfoBarDelegate(), error_(error) {}

InstallationErrorInfoBarDelegate::~InstallationErrorInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
InstallationErrorInfoBarDelegate::GetIdentifier() const {
  return INSTALLATION_ERROR_INFOBAR_DELEGATE;
}

base::string16 InstallationErrorInfoBarDelegate::GetLinkText() const {
  return error_.type() == extensions::CrxInstallErrorType::OTHER &&
                 error_.detail() == extensions::CrxInstallErrorDetail::
                                        OFFSTORE_INSTALL_DISALLOWED
             ? l10n_util::GetStringUTF16(IDS_LEARN_MORE)
             : base::string16();
}

GURL InstallationErrorInfoBarDelegate::GetLinkURL() const {
  return GURL("https://support.google.com/chrome_webstore/?p=crx_warning");
}

base::string16 InstallationErrorInfoBarDelegate::GetMessageText() const {
  return error_.message();
}

int InstallationErrorInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}
