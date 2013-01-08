// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/obsolete_os_info_bar.h"

#include "chrome/browser/api/infobars/infobar_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#endif

using content::OpenURLParams;
using content::Referrer;

namespace chrome {

// static
void ObsoleteOSInfoBar::Create(InfoBarService* infobar_service) {
#if defined(TOOLKIT_GTK)
  // We've deprecated support for Ubuntu Hardy.  Rather than attempting to
  // determine whether you're using that, we instead key off the GTK version;
  // this will also deprecate other distributions (including variants of Ubuntu)
  // that are of a similar age.
  // Version key:
  //   Ubuntu Hardy: GTK 2.12
  //   RHEL 6:       GTK 2.18
  //   Ubuntu Lucid: GTK 2.20
  if (!gtk_check_version(2, 18, 0))
    return;
#else
  // No other platforms currently show this infobar.
  return;
#endif

  string16 message = l10n_util::GetStringUTF16(IDS_SYSTEM_OBSOLETE_MESSAGE);
  // Link to an article in the help center on minimum system requirements.
  const char* kLearnMoreURL =
      "http://www.google.com/support/chrome/bin/answer.py?answer=95411";
  infobar_service->AddInfoBar(scoped_ptr<InfoBarDelegate>(
      new ObsoleteOSInfoBar(infobar_service, message, GURL(kLearnMoreURL))));
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
  OpenURLParams params(learn_more_url_, Referrer(),
      (disposition == CURRENT_TAB) ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK, false);
  owner()->GetWebContents()->OpenURL(params);
  return false;
}

ObsoleteOSInfoBar::ObsoleteOSInfoBar(InfoBarService* infobar_service,
                                     const string16& message,
                                     const GURL& url)
    : ConfirmInfoBarDelegate(infobar_service),
      message_(message),
      learn_more_url_(url) {
}

ObsoleteOSInfoBar::~ObsoleteOSInfoBar() {
}

}  // namespace chrome
