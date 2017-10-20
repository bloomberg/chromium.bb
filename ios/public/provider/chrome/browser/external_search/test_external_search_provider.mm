// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/external_search/test_external_search_provider.h"

bool TestExternalSearchProvider::IsExternalSearchEnabled() {
  return true;
}

NSURL* TestExternalSearchProvider::GetLaunchURL() {
  return [NSURL URLWithString:@"testexternalsearch://"];
}

NSString* TestExternalSearchProvider::GetButtonImageName() {
  return nil;
}

int TestExternalSearchProvider::GetButtonIdsAccessibilityLabel() {
  return 0;
}

NSString* TestExternalSearchProvider::GetButtonAccessibilityIdentifier() {
  return nil;
}
