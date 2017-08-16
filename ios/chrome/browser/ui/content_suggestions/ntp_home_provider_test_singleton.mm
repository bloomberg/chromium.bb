// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_provider_test_singleton.h"

#include "base/memory/ptr_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using namespace ntp_snippets;

@implementation ContentSuggestionsTestSingleton {
  MockContentSuggestionsProvider* _provider;
}

+ (instancetype)sharedInstance {
  static ContentSuggestionsTestSingleton* sharedInstance = nil;
  static dispatch_once_t onceToken;
  dispatch_once(&onceToken, ^{
    sharedInstance = [[self alloc] init];
  });
  return sharedInstance;
}

- (MockContentSuggestionsProvider*)provider {
  return _provider;
}

- (void)registerArticleProvider:(ContentSuggestionsService*)service {
  Category articles = Category::FromKnownCategory(KnownCategories::ARTICLES);
  std::unique_ptr<MockContentSuggestionsProvider> provider =
      base::MakeUnique<MockContentSuggestionsProvider>(
          service, std::vector<Category>{articles});
  _provider = provider.get();
  service->RegisterProvider(std::move(provider));
}

@end
