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
@synthesize localizedName = _localizedName;
@synthesize nativeName = _nativeName;

- (instancetype)initWithLanguageCode:(const std::string&)languageCode
                       localizedName:(const base::string16&)localizedName
                          nativeName:(const base::string16&)nativeName {
  self = [super init];
  if (self) {
    _languageCode = base::SysUTF8ToNSString(languageCode);
    _localizedName = base::SysUTF16ToNSString(localizedName);
    _nativeName = base::SysUTF16ToNSString(nativeName);
  }
  return self;
}

- (NSString*)description {
  return
      [NSString stringWithFormat:@"%@ name:%@(%@) code:%@", [super description],
                                 _localizedName, _nativeName, _languageCode];
}

@end
