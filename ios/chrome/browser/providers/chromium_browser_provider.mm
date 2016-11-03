// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/providers/chromium_browser_provider.h"

#include "base/memory/ptr_util.h"
#import "ios/chrome/browser/providers/chromium_logo_controller.h"
#import "ios/chrome/browser/providers/chromium_voice_search_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ChromiumBrowserProvider::ChromiumBrowserProvider()
    : voice_search_provider_(base::MakeUnique<ChromiumVoiceSearchProvider>()) {}

ChromiumBrowserProvider::~ChromiumBrowserProvider() {}

VoiceSearchProvider* ChromiumBrowserProvider::GetVoiceSearchProvider() const {
  return voice_search_provider_.get();
}

id<LogoVendor> ChromiumBrowserProvider::CreateLogoVendor(
    ios::ChromeBrowserState* browser_state,
    id<UrlLoader> loader) const {
  return [[ChromiumLogoController alloc] init];
}
