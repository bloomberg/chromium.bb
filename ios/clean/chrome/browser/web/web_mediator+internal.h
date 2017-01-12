// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// ======                        New Architecture                         =====
// =         This code is only used in the new iOS Chrome architecture.       =
// ============================================================================

#ifndef IOS_CHROME_BROWSER_WEB_WEB_MEDIATOR_INTERNAL_H_
#define IOS_CHROME_BROWSER_WEB_WEB_MEDIATOR_INTERNAL_H_

#import <Foundation/Foundation.h>

#import "ios/clean/chrome/browser/web/web_mediator.h"

// Internal API for use by WebMediator categories.
@interface WebMediator (Internal)

// Dictionary to store injector objects. Categories should define their
// own keys.
@property(nonatomic, readonly)
    NSMutableDictionary<NSString*, NSObject*>* injectors;

@end

#endif  // IOS_CHROME_BROWSER_WEB_WEB_MEDIATOR_INTERNAL_H_
