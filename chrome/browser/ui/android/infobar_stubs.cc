// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"

// Infobars are implemented in Java on Android.

InfoBar* ConfirmInfoBarDelegate::CreateInfoBar(InfoBarTabHelper* owner) {
  NOTIMPLEMENTED();
  return NULL;
}

InfoBar* ExtensionInfoBarDelegate::CreateInfoBar(InfoBarTabHelper* owner) {
  NOTIMPLEMENTED();
  return NULL;
}
