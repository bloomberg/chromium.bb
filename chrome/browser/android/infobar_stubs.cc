// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/api/infobars/confirm_infobar_delegate.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"

// Clank implementations of cross-platform API, never used
// since infobars are created using the InfoBarFactory but we need
// them implemented so the binary links both upstream and downstream.
InfoBar* ConfirmInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  NOTREACHED() << "ConfirmInfoBar: InfoBarFactory should be used on Android";
  return NULL;
}

InfoBar* TranslateInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  return NULL;
}
