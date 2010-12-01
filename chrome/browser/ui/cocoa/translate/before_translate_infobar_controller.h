// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/translate/translate_infobar_base.h"

@interface BeforeTranslateInfobarController : TranslateInfoBarControllerBase {
  scoped_nsobject<NSButton> alwaysTranslateButton_;
  scoped_nsobject<NSButton> neverTranslateButton_;
}

// Creates and initializes the alwaysTranslate and neverTranslate buttons.
- (void)initializeExtraControls;

@end

@interface BeforeTranslateInfobarController (TestingAPI)

- (NSButton*)alwaysTranslateButton;
- (NSButton*)neverTranslateButton;

@end

