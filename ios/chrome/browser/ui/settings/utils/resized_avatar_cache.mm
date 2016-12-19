// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/utils/resized_avatar_cache.h"

#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#include "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"

namespace {
const CGFloat kAccountProfilePhotoDimension = 40.0f;
}  // namespace

@implementation ResizedAvatarCache {
  // Retains resized images. Key is Chrome Identity.
  base::scoped_nsobject<NSCache<ChromeIdentity*, UIImage*>> _resizedImages;

  // Holds weak references to the cached avatar image from the
  // ChromeIdentityService. Key is Chrome Identity.
  base::scoped_nsobject<NSMapTable<ChromeIdentity*, UIImage*>> _originalImages;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _resizedImages.reset([[NSCache alloc] init]);
    _originalImages.reset([[NSMapTable strongToWeakObjectsMapTable] retain]);
  }
  return self;
}

- (UIImage*)resizedAvatarForIdentity:(ChromeIdentity*)identity {
  UIImage* image = ios::GetChromeBrowserProvider()
                       ->GetChromeIdentityService()
                       ->GetCachedAvatarForIdentity(identity);
  if (!image) {
    image = ios::GetChromeBrowserProvider()
                ->GetSigninResourcesProvider()
                ->GetDefaultAvatar();
    // No cached image, trigger a fetch, which will notify all observers.
    ios::GetChromeBrowserProvider()
        ->GetChromeIdentityService()
        ->GetAvatarForIdentity(identity, ^(UIImage*){
                               });
  }

  // If the currently used image has already been resized, use it.
  if ([_resizedImages objectForKey:identity] &&
      [_originalImages objectForKey:identity] == image)
    return [_resizedImages objectForKey:identity];

  [_originalImages setObject:image forKey:identity];

  // Resize the profile image.
  CGFloat dimension = kAccountProfilePhotoDimension;
  if (image.size.width != dimension || image.size.height != dimension) {
    image = ResizeImage(image, CGSizeMake(dimension, dimension),
                        ProjectionMode::kAspectFit);
  }
  [_resizedImages setObject:image forKey:identity];
  return image;
}

@end
