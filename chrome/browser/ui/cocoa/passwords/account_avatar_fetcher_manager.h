// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_PASSWORDS_ACCOUNT_AVATAR_FETCHER_MANAGER_H_
#define CHROME_BROWSER_UI_COCOA_PASSWORDS_ACCOUNT_AVATAR_FETCHER_MANAGER_H_

#include <Cocoa/Cocoa.h>

#include "base/memory/ref_counted.h"
#include "content/public/common/url_loader_factory.mojom.h"

class AccountAvatarFetcherBridge;
@class CredentialItemButton;
class GURL;

// Handles retrieving avatar images for credential items.
@interface AccountAvatarFetcherManager : NSObject {
  std::vector<std::unique_ptr<AccountAvatarFetcherBridge>> bridges_;
  content::mojom::URLLoaderFactory* loaderFactory_;
}

// Initializes a manager with the specified URL loader factory.
- (id)initWithLoaderFactory:(content::mojom::URLLoaderFactory*)loaderFactory;

// Retrieves the image located at |avatarURL| and updates |view| if successful.
- (void)fetchAvatar:(const GURL&)avatarURL forView:(CredentialItemButton*)view;

@end

#endif  // CHROME_BROWSER_UI_COCOA_PASSWORDS_ACCOUNT_AVATAR_FETCHER_MANAGER_H_
