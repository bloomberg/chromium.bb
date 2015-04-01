// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/chrome_translate_client.h"

#include "components/infobars/core/infobar.h"
#include "components/translate/core/browser/translate_infobar_delegate.h"

// ChromeTranslateClient::CreateInfoBar() is implemented in platform-specific
// files, except the TOOLKIT_VIEWS implementation, which has been removed.
scoped_ptr<infobars::InfoBar> ChromeTranslateClient::CreateInfoBar(
    scoped_ptr<translate::TranslateInfoBarDelegate> delegate) const {
  return scoped_ptr<infobars::InfoBar>();
}
