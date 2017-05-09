// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/translate/cwv_translation_language_internal.h"

#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVTranslationLanguage

@synthesize languageCode = _languageCode;
@synthesize languageName = _languageName;

- (instancetype)initWithLanguageCode:(const std::string&)languageCode
                        languageName:(const base::string16&)languageName {
  self = [super init];
  if (self) {
    _languageCode = base::SysUTF8ToNSString(languageCode);
    _languageName = base::SysUTF16ToNSString(languageName);
  }
  return self;
}

@end
