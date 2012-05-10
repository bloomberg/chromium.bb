// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/bad_flags_prompt.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/tab_contents/simple_alert_infobar_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace browser {

void ShowBadFlagsPrompt(Browser* browser) {
  // Unsupported flags for which to display a warning that "stability and
  // security will suffer".
  static const char* kBadFlags[] = {
    // These imply disabling the sandbox.
    switches::kSingleProcess,
    switches::kNoSandbox,
    switches::kInProcessWebGL,
    NULL
  };

  const char* bad_flag = NULL;
  for (const char** flag = kBadFlags; *flag; ++flag) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(*flag)) {
      bad_flag = *flag;
      break;
    }
  }

  if (bad_flag) {
    TabContentsWrapper* tab = browser->GetSelectedTabContentsWrapper();
    if (!tab)
      return;
    tab->infobar_tab_helper()->AddInfoBar(
        new SimpleAlertInfoBarDelegate(
            tab->infobar_tab_helper(), NULL,
            l10n_util::GetStringFUTF16(
            IDS_BAD_FLAGS_WARNING_MESSAGE,
            UTF8ToUTF16(std::string("--") + bad_flag)),
            false));
  }
}

}  // namespace browser
