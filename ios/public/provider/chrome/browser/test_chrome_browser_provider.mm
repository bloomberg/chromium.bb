// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/test_chrome_browser_provider.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/ptr_util.h"
#include "ios/public/provider/chrome/browser/signin/fake_chrome_identity_service.h"
#import "ios/public/provider/chrome/browser/test_updatable_resource_provider.h"
#import "ios/public/provider/chrome/browser/voice/voice_search_language.h"

@interface TestStyledTextField : UITextField<TextFieldStyling>
@end

@implementation TestStyledTextField
@synthesize placeholderStyle = _placeholderStyle;
@synthesize textValidator = _textValidator;

- (void)setUseErrorStyling:(BOOL)error {
}
@end

namespace ios {

TestChromeBrowserProvider::TestChromeBrowserProvider()
    : test_updatable_resource_provider_(new TestUpdatableResourceProvider) {}

TestChromeBrowserProvider::~TestChromeBrowserProvider() {}

// static
TestChromeBrowserProvider* TestChromeBrowserProvider::GetTestProvider() {
  ChromeBrowserProvider* provider = GetChromeBrowserProvider();
  DCHECK(provider);
  return static_cast<TestChromeBrowserProvider*>(provider);
}

void TestChromeBrowserProvider::SetChromeIdentityServiceForTesting(
    std::unique_ptr<ChromeIdentityService> service) {
  chrome_identity_service_.swap(service);
}

ChromeIdentityService* TestChromeBrowserProvider::GetChromeIdentityService() {
  if (!chrome_identity_service_) {
    chrome_identity_service_.reset(new FakeChromeIdentityService());
  }
  return chrome_identity_service_.get();
}

UpdatableResourceProvider*
TestChromeBrowserProvider::GetUpdatableResourceProvider() {
  return test_updatable_resource_provider_.get();
}

UITextField<TextFieldStyling>* TestChromeBrowserProvider::CreateStyledTextField(
    CGRect frame) const {
  return [[TestStyledTextField alloc] initWithFrame:frame];
}

NSArray* TestChromeBrowserProvider::GetAvailableVoiceSearchLanguages() const {
  base::scoped_nsobject<VoiceSearchLanguage> en([[VoiceSearchLanguage alloc]
          initWithIdentifier:@"en-US"
                 displayName:@"English (US)"
      localizationPreference:nil]);
  return @[ en ];
}

}  // namespace ios
