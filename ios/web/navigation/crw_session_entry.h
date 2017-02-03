// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_SESSION_ENTRY_H_
#define IOS_WEB_NAVIGATION_CRW_SESSION_ENTRY_H_

#import <Foundation/Foundation.h>
#include <stdint.h>

#include <memory>

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace web {
class NavigationItem;
class NavigationItemImpl;
}

// A CRWSessionEntry is similar to a NavigationEntry object in desktop Chrome.
// It maintains the information needed to save/restore a single entry in session
// history (i.e., one site) for a tab. A tab may have multiple of these objects
// comprising its entire session history.
// DEPRECATED, do not use this class and do not add any methods to it.
// Use web::NavigationItem instead.
// TODO(crbug.com/454984): Remove this class.
@interface CRWSessionEntry : NSObject<NSCopying>

// Pointer to the NavigationItem associated with this CRWSessionEntry.
// Eventually, this will replace CRWSessionEntry entirely.
@property(nonatomic, readonly) web::NavigationItem* navigationItem;

// Pointer to the NavigationItemImpl associated with this CRWSessionEntry.
@property(nonatomic, readonly) web::NavigationItemImpl* navigationItemImpl;

// Initialize the session entry with the given NavigationItem.
- (instancetype)initWithNavigationItem:
    (std::unique_ptr<web::NavigationItem>)item;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_SESSION_ENTRY_H_
