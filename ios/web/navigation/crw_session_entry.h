// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_SESSION_ENTRY_H_
#define IOS_WEB_NAVIGATION_CRW_SESSION_ENTRY_H_

#import <Foundation/Foundation.h>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace web {
class NavigationItem;
class NavigationItemImpl;
struct Referrer;
}

// A CRWSessionEntry is similar to a NavigationEntry object in desktop Chrome.
// It maintains the information needed to save/restore a single entry in session
// history (i.e., one site) for a tab. A tab may have multiple of these objects
// comprising its entire session history.
// TODO(rohitrao): Fold CRWSessionEntry's logic into NavigationItem.
@interface CRWSessionEntry : NSObject<NSCoding, NSCopying>

@property(nonatomic, readonly) const GURL& originalUrl;

// Pointer to the NavigationItem associated with this CRWSessionEntry.
// Eventually, this will replace CRWSessionEntry entirely.
@property(nonatomic, readonly) web::NavigationItem* navigationItem;

// Pointer to the NavigationItemImpl associated with this CRWSessionEntry.
// TODO(kkhorimoto): This is a convenience property to avoid requiring static
// casts every time the web layer needs access to members only available in
// NavigationItemImpl.  Remove once more navigation management moves into
// NavigationManager and CRWSessionEntry=>web::NavigationItemImpl conversions
// become less prominent.
@property(nonatomic, readonly) web::NavigationItemImpl* navigationItemImpl;

// Initialize the session entry with the given NavigationItem.
- (instancetype)initWithNavigationItem:(scoped_ptr<web::NavigationItem>)item;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_SESSION_ENTRY_H_
