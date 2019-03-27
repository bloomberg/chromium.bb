// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_deprecation_infobar_delegate.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "ui/base/l10n/l10n_util.h"

// static
void FlashDeprecationInfoBarDelegate::Create(InfoBarService* infobar_service) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
      std::make_unique<FlashDeprecationInfoBarDelegate>()));
}

infobars::InfoBarDelegate::InfoBarIdentifier
FlashDeprecationInfoBarDelegate::GetIdentifier() const {
  return FLASH_DEPRECATION_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& FlashDeprecationInfoBarDelegate::GetVectorIcon() const {
  // TODO(tommycli): Replace this placeholder vector icon.
  return kExtensionCrashedIcon;
}

base::string16 FlashDeprecationInfoBarDelegate::GetMessageText() const {
  // TODO(tommycli): Replace empty string with real IDS once we use reference
  // this infobar from production code. Currently, this IDS is stripped by
  // enable_resource_whitelist_generation = true, and this crashes the
  // Windows infobar browsertest (which tries to fetch the stripped IDS).
  // return l10n_util::GetStringUTF16(IDS_PLUGIN_FLASH_DEPRECATION_PROMPT);
  return base::string16();
}

int FlashDeprecationInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}
