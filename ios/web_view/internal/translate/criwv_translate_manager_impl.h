// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_TRANSLATE_CRIWV_TRANSLATE_MANAGER_IMPL_H_
#define IOS_WEB_VIEW_INTERNAL_TRANSLATE_CRIWV_TRANSLATE_MANAGER_IMPL_H_

#include <string>

#import "ios/web_view/public/criwv_translate_manager.h"

namespace translate {
class TranslateManager;
}

// CRIWVTranslateManagerImpl is mostly an Objective-C wrapper around
// translate::TranslateUIDelegate.
@interface CRIWVTranslateManagerImpl : NSObject<CRIWVTranslateManager>

- (instancetype)init NS_UNAVAILABLE;

// |manager| is expexted to outlive this CRIWVTranslateManagerImpl.
- (instancetype)initWithTranslateManager:(translate::TranslateManager*)manager
                          sourceLanguage:(const std::string&)source
                          targetLanguage:(const std::string&)target
    NS_DESIGNATED_INITIALIZER;

@end

#endif  // IOS_WEB_VIEW_INTERNAL_TRANSLATE_CRIWV_TRANSLATE_MANAGER_IMPL_H_
