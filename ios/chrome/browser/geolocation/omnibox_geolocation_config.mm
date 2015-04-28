// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/omnibox_geolocation_config.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "url/gurl.h"

namespace {

NSString* const kEligibleDomainsKey = @"EligibleDomains";

}  // namespace

@interface OmniboxGeolocationConfig () {
  // Whitelist of domains eligible for Omnibox geolocation.
  std::set<std::string> _eligibleDomains;
}

// Loads |eligibleDomains_| from config file.
- (void)loadEligibleDomains;

@end

@implementation OmniboxGeolocationConfig

+ (OmniboxGeolocationConfig*)sharedInstance {
  static OmniboxGeolocationConfig* instance =
      [[OmniboxGeolocationConfig alloc] init];
  return instance;
}

- (id)init {
  self =
      [super initWithAppId:nil version:nil plist:@"OmniboxGeolocation.plist"];
  if (self) {
    [self loadEligibleDomains];
  }
  return self;
}

- (BOOL)URLHasEligibleDomain:(const GURL&)url {
  // Here we iterate through the eligible domains and check url.DomainIs() for
  // each domain rather than extracting url.host() and checking the domain for
  // membership in |eligibleDomains_|, because GURL::DomainIs() is more robust
  // and contains logic that we want to reuse.
  for (const auto& eligibleDomain : _eligibleDomains) {
    if (url.DomainIs(eligibleDomain.c_str()))
      return YES;
  }
  return NO;
}

#pragma mark - Private

- (void)loadEligibleDomains {
  _eligibleDomains.clear();

  NSDictionary* configData = [self dictionaryFromConfig];
  NSArray* eligibleDomains = [configData objectForKey:kEligibleDomainsKey];
  if (eligibleDomains) {
    DCHECK([eligibleDomains isKindOfClass:[NSArray class]]);

    for (NSString* domain in eligibleDomains) {
      DCHECK([domain isKindOfClass:[NSString class]]);
      if ([domain length]) {
        _eligibleDomains.insert(
            base::SysNSStringToUTF8([domain lowercaseString]));
      }
    }
  }

  DCHECK(!_eligibleDomains.empty());
}

@end
