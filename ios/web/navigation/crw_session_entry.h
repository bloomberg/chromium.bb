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
struct Referrer;
}

// A CRWSessionEntry is similar to a NavigationEntry object in desktop Chrome.
// It maintains the information needed to save/restore a single entry in session
// history (i.e., one site) for a tab. A tab may have multiple of these objects
// comprising its entire session history.
// TODO(rohitrao): Fold CRWSessionEntry's logic into NavigationItem.
@interface CRWSessionEntry : NSObject<NSCoding, NSCopying>

@property(nonatomic, assign) NSInteger index;
@property(nonatomic, readonly) const GURL& originalUrl;
@property(nonatomic, assign) BOOL useDesktopUserAgent;
@property(nonatomic, assign) BOOL usedDataReductionProxy;
@property(nonatomic, retain) NSString* serializedStateObject;
@property(nonatomic, assign) BOOL createdFromPushState;
@property(nonatomic, retain) NSData* POSTData;
@property(nonatomic, readonly) NSDictionary* httpHeaders;
@property(nonatomic, assign) BOOL skipResubmitDataConfirmation;

// Initialize the session entry with the given url.
- (id)initWithUrl:(const GURL&)url
               referrer:(const web::Referrer&)referrer
             transition:(ui::PageTransition)transition
    useDesktopUserAgent:(BOOL)useDesktopUserAgent
      rendererInitiated:(BOOL)rendererInitiated;

// Initialize the session entry with the given NavigationItem.
- (id)initWithNavigationItem:(scoped_ptr<web::NavigationItem>)item
                       index:(int)index;

// Returns a pointer to the NavigationItem associated with this CRWSessionEntry.
// Eventually, this will replace CRWSessionEntry entirely.
- (web::NavigationItem*)navigationItem;

// Adds headers from |moreHTTPHeaders| to |httpHeaders|; existing headers with
// the same key will be overridden.
- (void)addHTTPHeaders:(NSDictionary*)moreHTTPHeaders;

// Removes the header for the given key from |httpHeaders|.
- (void)removeHTTPHeaderForKey:(NSString*)key;

// Resets |httpHeaders| to nil.
- (void)resetHTTPHeaders;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_SESSION_ENTRY_H_
