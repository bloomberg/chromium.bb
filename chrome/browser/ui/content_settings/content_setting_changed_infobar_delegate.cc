// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/content_setting_changed_infobar_delegate.h"

#include "base/logging.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

// static
void ContentSettingChangedInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    int icon,
    int message_text) {
  infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new ContentSettingChangedInfoBarDelegate(infobar_service,
                                               icon,
                                               message_text)));
}

ContentSettingChangedInfoBarDelegate::ContentSettingChangedInfoBarDelegate(
    InfoBarService* infobar_service,
    int icon,
    int message_text)
    : ConfirmInfoBarDelegate(infobar_service),
      icon_(icon),
      message_text_(message_text) {
}

ContentSettingChangedInfoBarDelegate::~ContentSettingChangedInfoBarDelegate() {
}

gfx::Image* ContentSettingChangedInfoBarDelegate::GetIcon() const {
  return &ResourceBundle::GetSharedInstance().GetNativeImageNamed(icon_);
}

InfoBarDelegate::Type ContentSettingChangedInfoBarDelegate::GetInfoBarType()
    const {
  return PAGE_ACTION_TYPE;
}

string16 ContentSettingChangedInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(message_text_);
}

int ContentSettingChangedInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

string16 ContentSettingChangedInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_CONTENT_SETTING_CHANGED_INFOBAR_BUTTON);
}

bool ContentSettingChangedInfoBarDelegate::Accept() {
  owner()->GetWebContents()->GetController().Reload(true);
  return true;
}
