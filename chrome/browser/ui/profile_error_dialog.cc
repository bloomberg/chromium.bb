// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/profile_error_dialog.h"

#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

void ShowProfileErrorDialog(int message_id) {
  // Parent the dialog to the current browser. During startup there may be no
  // browser.
  Browser* browser = BrowserList::GetLastActive();
  platform_util::SimpleErrorBox(
      browser ? browser->window()->GetNativeHandle() : NULL,
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME),
      l10n_util::GetStringUTF16(message_id));
}
