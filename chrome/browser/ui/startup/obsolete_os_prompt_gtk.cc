// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/obsolete_os_prompt.h"

#include <gtk/gtk.h>

#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/startup/obsolete_os_info_bar.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace chrome {

void ShowObsoleteOSPrompt(Browser* browser) {
  // We've deprecated support for Ubuntu Hardy.  Rather than attempting to
  // determine whether you're using that, we instead key off the GTK version;
  // this will also deprecate other distributions (including variants of Ubuntu)
  // that are of a similar age.
  // Version key:
  //   Ubuntu Hardy: GTK 2.12
  //   RHEL 6:       GTK 2.18
  //   Ubuntu Lucid: GTK 2.20
  if (gtk_check_version(2, 18, 0)) {
    string16 message = l10n_util::GetStringUTF16(IDS_SYSTEM_OBSOLETE_MESSAGE);
    // Link to an article in the help center on minimum system requirements.
    const char* kLearnMoreURL =
        "http://www.google.com/support/chrome/bin/answer.py?answer=95411";
    content::WebContents* web_contents = chrome::GetActiveWebContents(browser);
    if (!web_contents)
      return;
    InfoBarService* infobar_service =
        InfoBarService::FromWebContents(web_contents);
    infobar_service->AddInfoBar(new ObsoleteOSInfoBar(infobar_service, message,
                                                      GURL(kLearnMoreURL)));
  }
}

}  // namespace chrome
