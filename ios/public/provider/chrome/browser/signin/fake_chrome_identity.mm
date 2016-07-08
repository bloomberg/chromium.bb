// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/signin/fake_chrome_identity.h"

#import "base/mac/scoped_nsobject.h"

@implementation FakeChromeIdentity {
  base::scoped_nsobject<NSString> _userEmail;
  base::scoped_nsobject<NSString> _gaiaID;
  base::scoped_nsobject<NSString> _userFullName;
  base::scoped_nsobject<NSString> _hashedGaiaID;
}

+ (FakeChromeIdentity*)identityWithEmail:(NSString*)email
                                  gaiaID:(NSString*)gaiaID
                                    name:(NSString*)name {
  return
      [[[FakeChromeIdentity alloc] initWithEmail:email gaiaID:gaiaID name:name]
          autorelease];
}

- (instancetype)initWithEmail:(NSString*)email
                       gaiaID:(NSString*)gaiaID
                         name:(NSString*)name {
  self = [super init];
  if (self) {
    _userEmail.reset([email copy]);
    _gaiaID.reset([gaiaID copy]);
    _userFullName.reset([name copy]);
    _hashedGaiaID.reset(
        [[NSString stringWithFormat:@"%@_hashID", name] retain]);
  }
  return self;
}

- (NSString*)userEmail {
  return _userEmail;
}

- (NSString*)gaiaID {
  return _gaiaID;
}

- (NSString*)userFullName {
  return _userFullName;
}

- (NSString*)hashedGaiaID {
  return _hashedGaiaID;
}

@end
