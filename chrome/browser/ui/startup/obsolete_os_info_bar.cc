// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/obsolete_os_info_bar.h"

#include "chrome/browser/api/infobars/infobar_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

using content::OpenURLParams;
using content::Referrer;

namespace chrome {

ObsoleteOSInfoBar::ObsoleteOSInfoBar(InfoBarService* infobar_service,
                                     const string16& message,
                                     const GURL& url)
    : ConfirmInfoBarDelegate(infobar_service),
      message_(message),
      learn_more_url_(url) {
}

ObsoleteOSInfoBar::~ObsoleteOSInfoBar() {
}

string16 ObsoleteOSInfoBar::GetMessageText() const {
  return message_;
}

int ObsoleteOSInfoBar::GetButtons() const {
  return BUTTON_NONE;
}

string16 ObsoleteOSInfoBar::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool ObsoleteOSInfoBar::LinkClicked(WindowOpenDisposition disposition) {
  OpenURLParams params(
      learn_more_url_, Referrer(), disposition, content::PAGE_TRANSITION_LINK,
      false);
  owner()->GetWebContents()->OpenURL(params);
  return false;
}

}  // namespace chrome
