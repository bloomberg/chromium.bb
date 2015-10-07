// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/media/media_throttle_infobar_delegate.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

// static
void MediaThrottleInfoBarDelegate::Create(
    content::WebContents* web_contents,
    const DecodeRequestGrantedCallback& callback) {
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  scoped_ptr<infobars::InfoBar> new_infobar(
      infobar_service->CreateConfirmInfoBar(scoped_ptr<ConfirmInfoBarDelegate>(
          new MediaThrottleInfoBarDelegate(callback))));

  for (size_t i = 0; i < infobar_service->infobar_count(); ++i) {
    infobars::InfoBar* old_infobar = infobar_service->infobar_at(i);
    MediaThrottleInfoBarDelegate* delegate =
        old_infobar->delegate()->AsMediaThrottleInfoBarDelegate();
    if (delegate != nullptr) {
      infobar_service->ReplaceInfoBar(old_infobar, new_infobar.Pass());
      return;
    }
  }

  infobar_service->AddInfoBar(new_infobar.Pass());
}

MediaThrottleInfoBarDelegate::MediaThrottleInfoBarDelegate(
    const DecodeRequestGrantedCallback& callback)
    : decode_granted_callback_(callback) {
}

MediaThrottleInfoBarDelegate::~MediaThrottleInfoBarDelegate() {}

MediaThrottleInfoBarDelegate*
    MediaThrottleInfoBarDelegate::AsMediaThrottleInfoBarDelegate() {
  return this;
}

base::string16 MediaThrottleInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_MEDIA_THROTTLE_INFOBAR_TEXT);
}

base::string16 MediaThrottleInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_MEDIA_THROTTLE_INFOBAR_BLOCK_BUTTON :
      IDS_MEDIA_THROTTLE_INFOBAR_ALLOW_BUTTON);
}

bool MediaThrottleInfoBarDelegate::Accept() {
  decode_granted_callback_.Run(false);
  return true;
}

bool MediaThrottleInfoBarDelegate::Cancel() {
  decode_granted_callback_.Run(true);
  return true;
}

void MediaThrottleInfoBarDelegate::InfoBarDismissed() {
  decode_granted_callback_.Run(false);
}
