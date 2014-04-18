// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/content_settings/media_setting_changed_infobar_delegate.h"

#include "base/logging.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

// static
void MediaSettingChangedInfoBarDelegate::Create(
    InfoBarService* infobar_service) {
  infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(
          new MediaSettingChangedInfoBarDelegate())));
}

MediaSettingChangedInfoBarDelegate::MediaSettingChangedInfoBarDelegate()
    : ConfirmInfoBarDelegate() {
}

MediaSettingChangedInfoBarDelegate::~MediaSettingChangedInfoBarDelegate() {
}

int MediaSettingChangedInfoBarDelegate::GetIconID() const {
  return IDR_INFOBAR_MEDIA_STREAM_CAMERA;
}

infobars::InfoBarDelegate::Type
MediaSettingChangedInfoBarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

base::string16 MediaSettingChangedInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(
      IDS_MEDIASTREAM_SETTING_CHANGED_INFOBAR_MESSAGE);
}

int MediaSettingChangedInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

base::string16 MediaSettingChangedInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_CONTENT_SETTING_CHANGED_INFOBAR_BUTTON);
}

bool MediaSettingChangedInfoBarDelegate::Accept() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(infobar());
  web_contents->GetController().Reload(true);
  return true;
}
