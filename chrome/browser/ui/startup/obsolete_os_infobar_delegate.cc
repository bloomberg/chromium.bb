// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/obsolete_os_infobar_delegate.h"

#include "chrome/browser/infobars/infobar_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#endif


// static
void ObsoleteOSInfoBarDelegate::Create(InfoBarService* infobar_service) {
#if defined(TOOLKIT_GTK)
  // We've deprecated support for Ubuntu Lucid.  Rather than attempting to
  // determine whether you're using that, we instead key off the GTK version;
  // this will also deprecate other distributions (including variants of Ubuntu)
  // that are of a similar age.
  // Version key:
  //   RHEL 6:             GTK 2.18
  //   Debian 6 (Squeeze): GTK 2.20
  //   Ubuntu Lucid:       GTK 2.20
  //   openSUSE 12.2       GTK 2.24
  //   Ubuntu Precise:     GTK 2.24
  if (!gtk_check_version(2, 24, 0))
    return;
#else
  // No other platforms currently show this infobar.
  return;
#endif

  infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new ObsoleteOSInfoBarDelegate(infobar_service)));
}

ObsoleteOSInfoBarDelegate::ObsoleteOSInfoBarDelegate(
    InfoBarService* infobar_service)
    : ConfirmInfoBarDelegate(infobar_service) {
}

ObsoleteOSInfoBarDelegate::~ObsoleteOSInfoBarDelegate() {
}

string16 ObsoleteOSInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_SYSTEM_OBSOLETE_MESSAGE);
}

int ObsoleteOSInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

string16 ObsoleteOSInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

bool ObsoleteOSInfoBarDelegate::LinkClicked(WindowOpenDisposition disposition) {
  web_contents()->OpenURL(content::OpenURLParams(
      GURL("http://www.google.com/support/chrome/bin/answer.py?answer=95411"),
      content::Referrer(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK, false));
  return false;
}
