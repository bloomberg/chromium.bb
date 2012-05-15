// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/obsolete_os_prompt.h"

#include "base/mac/mac_util.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/startup/obsolete_os_info_bar.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// The URL for the Mac OS X 10.5 deprecation help center article.
const char kMacLeopardDeprecationUrl[] =
    "https://support.google.com/chrome/?p=ui_mac_leopard_support";

}  // namespace

namespace browser {

void ShowObsoleteOSPrompt(Browser* browser) {
  if (!base::mac::IsOSLeopard())
    return;

  TabContentsWrapper* tab = browser->GetSelectedTabContentsWrapper();
  if (!tab)
    return;
  tab->infobar_tab_helper()->AddInfoBar(
      new ObsoleteOSInfoBar(
          tab->infobar_tab_helper(),
          l10n_util::GetStringFUTF16(IDS_MAC_10_5_LEOPARD_DEPRECATED,
              l10n_util::GetStringUTF16(IDS_PRODUCT_NAME)),
          GURL(kMacLeopardDeprecationUrl)));
}

}  // namespace browser
