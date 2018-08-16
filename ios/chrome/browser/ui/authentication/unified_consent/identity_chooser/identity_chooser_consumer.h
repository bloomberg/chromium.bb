// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_CHOOSER_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_CHOOSER_CONSUMER_H_

@class IdentityChooserItem;

// Consumer for the IdentityChooser.
@protocol IdentityChooserConsumer

// Sets the |items| displayed by this consumer.
- (void)setIdentityItems:(NSArray<IdentityChooserItem*>*)items;

// Notifies the consumer that the |changedItem| has changed.
- (void)itemHasChanged:(IdentityChooserItem*)changedItem;

// Returns an IdentityChooserItem based on a gaia ID.
- (IdentityChooserItem*)identityChooserItemWithGaiaID:(NSString*)gaiaID;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_CHOOSER_CONSUMER_H_
