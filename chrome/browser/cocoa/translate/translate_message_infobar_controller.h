// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/translate/translate_infobar_base.h"

@interface TranslateMessageInfobarController : TranslateInfoBarControllerBase {
  // This keeps track of whether the infobar is displaying a message or an
  // error. If it is an error it should have a try again button.
  TranslateInfoBarDelegate::Type state_;
}

@end
