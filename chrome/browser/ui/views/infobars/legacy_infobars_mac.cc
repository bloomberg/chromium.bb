// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "chrome/browser/password_manager/save_password_infobar_delegate.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/views/infobars/confirm_infobar.h"

// On Mac, some infobars are still used that have been migrated to a bubble UI
// on other views platforms. When building the toolkit-views browser window on
// Mac, the bubble UI can be used, but interfaces built for Cocoa on Mac still
// depend on symbols to provide infobars. This file provides the symbols wanted
// elsewhere for OS_MACOSX, but aren't otherwise needed for a views browser.
// TODO(tapted): Delete this file when Mac uses the bubbles by default.

std::unique_ptr<infobars::InfoBar> CreateSavePasswordInfoBar(
    std::unique_ptr<SavePasswordInfoBarDelegate> delegate) {
  return base::WrapUnique(new ConfirmInfoBar(std::move(delegate)));
}

std::unique_ptr<infobars::InfoBar> ChromeTranslateClient::CreateInfoBar(
    std::unique_ptr<translate::TranslateInfoBarDelegate> delegate) const {
  NOTREACHED();
  return nullptr;
}
