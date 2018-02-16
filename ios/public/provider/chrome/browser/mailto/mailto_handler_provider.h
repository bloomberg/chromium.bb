// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MAILTO_MAILTO_HANDLER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MAILTO_MAILTO_HANDLER_PROVIDER_H_

#import <UIKit/UIKit.h>
#include "base/macros.h"

@class ChromeIdentity;
namespace ios {
class ChromeIdentityService;
}  // namespace ios

typedef ChromeIdentity* (^SignedInIdentityBlock)(void);
typedef NSArray<ChromeIdentity*>* (^SignedInIdentitiesBlock)(void);

// An provider to handle the opening of mailto links.
class MailtoHandlerProvider {
 public:
  MailtoHandlerProvider();
  virtual ~MailtoHandlerProvider();

  // Set up mailto handling for the currently signed in users.
  // The Signed-In Identity Block should return the primary signed in user.
  // The Signed-In Identities Block should return all users signed in to Chrome.
  virtual void PrepareMailtoHandling(
      ios::ChromeIdentityService* identity_service,
      SignedInIdentityBlock signed_in_identity_block,
      SignedInIdentitiesBlock signed_in_identities_block);

  // Creates and returns a view controller for presenting the settings for
  // mailto handling to the user. Returns nil if mailto handling is not
  // supported by the provider.
  virtual UIViewController* MailtoHandlerSettingsController() const;

  // Dismisses any mailto handling UI immediately. Handling is cancelled.
  virtual void DismissAllMailtoHandlerInterfaces() const;

  // Handles the specified mailto: URL. The provider falls back on the built-in
  // URL handling in case of error.
  virtual void HandleMailtoURL(NSURL* url) const;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailtoHandlerProvider);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_MAILTO_MAILTO_HANDLER_PROVIDER_H_
