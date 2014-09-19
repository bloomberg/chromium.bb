// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/obsolete_system_infobar_delegate.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_MACOSX)
#include "chrome/browser/mac/obsolete_system.h"
#endif

// static
void ObsoleteSystemInfoBarDelegate::Create(InfoBarService* infobar_service) {
#if defined(OS_MACOSX)
  if (!ObsoleteSystemMac::Is32BitObsoleteNowOrSoon() ||
      !ObsoleteSystemMac::Has32BitOnlyCPU()) {
    return;
  }
  infobar_service->AddInfoBar(ConfirmInfoBarDelegate::CreateInfoBar(
      scoped_ptr<ConfirmInfoBarDelegate>(new ObsoleteSystemInfoBarDelegate())));
#else
  // No other platforms currently show this infobar.
  return;
#endif
}

ObsoleteSystemInfoBarDelegate::ObsoleteSystemInfoBarDelegate()
    : ConfirmInfoBarDelegate() {
}

ObsoleteSystemInfoBarDelegate::~ObsoleteSystemInfoBarDelegate() {
}

base::string16 ObsoleteSystemInfoBarDelegate::GetMessageText() const {
#if defined(OS_MACOSX)
  return ObsoleteSystemMac::LocalizedObsoleteSystemString();
#else
  return l10n_util::GetStringUTF16(IDS_SYSTEM_OBSOLETE_MESSAGE);
#endif
}

int ObsoleteSystemInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

base::string16 ObsoleteSystemInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool ObsoleteSystemInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  InfoBarService::WebContentsFromInfoBar(infobar())->OpenURL(
      content::OpenURLParams(
#if defined(OS_MACOSX)
          GURL(chrome::kMac32BitDeprecationURL),
#else
          GURL("http://www.google.com/support/chrome/bin/"
               "answer.py?answer=95411"),
#endif
          content::Referrer(),
          (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
          ui::PAGE_TRANSITION_LINK, false));
  return false;
}
