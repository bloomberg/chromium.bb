// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/reader_mode/reader_mode_infobar_delegate.h"

#include "base/strings/utf_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ReaderModeInfoBarDelegate::ReaderModeInfoBarDelegate(
    const base::Closure& callback)
    : callback_(callback) {}

ReaderModeInfoBarDelegate::~ReaderModeInfoBarDelegate() {}

infobars::InfoBarDelegate::InfoBarIdentifier
ReaderModeInfoBarDelegate::GetIdentifier() const {
  return READER_MODE_INFOBAR_DELEGATE_IOS;
}

base::string16 ReaderModeInfoBarDelegate::GetMessageText() const {
  // TODO(noyau): Waiting for text to put here.
  return base::ASCIIToUTF16("Open in reader mode");
}

bool ReaderModeInfoBarDelegate::Accept() {
  callback_.Run();
  return true;
}
