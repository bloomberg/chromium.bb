// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/save_password_infobar_delegate.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"

// On Mac, some infobars are still used that have been migrated to a bubble UI
// on other views platforms. When building the toolkit-views browser window on
// Mac, the bubble UI can be used, but interfaces built for Cocoa on Mac still
// depend on symbols to provide infobars. This file provides the symbols wanted
// elsewhere for OS_MACOSX, but aren't otherwise needed for a views browser.
// TODO(tapted): Delete this file when Mac uses the bubbles by default.

scoped_ptr<infobars::InfoBar> CreateSavePasswordInfoBar(
    scoped_ptr<SavePasswordInfoBarDelegate> delegate) {
  return make_scoped_ptr(new ConfirmInfoBar(std::move(delegate)));
}

scoped_ptr<infobars::InfoBar> ChromeTranslateClient::CreateInfoBar(
    scoped_ptr<translate::TranslateInfoBarDelegate> delegate) const {
  NOTREACHED();
  return nullptr;
}
