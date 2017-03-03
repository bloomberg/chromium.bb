// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_SESSION_CONTROLLER_H_
#define IOS_WEB_NAVIGATION_CRW_SESSION_CONTROLLER_H_

#import <Foundation/Foundation.h>
#include <vector>

#include "ios/web/public/navigation_item_list.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@class CRWSessionEntry;
@class CRWSessionCertificatePolicyManager;

namespace web {
class BrowserState;
class NavigationItemImpl;
class NavigationManagerImpl;
enum class NavigationInitiationType;
struct Referrer;
}

// A CRWSessionController is similar to a NavigationController object in desktop
// Chrome. It maintains information needed to save/restore a tab with its
// complete session history. There is one of these for each tab.
// DEPRECATED, do not use this class and do not add any methods to it.
// Use web::NavigationManager instead.
// TODO(crbug.com/454984): Remove this class.
@interface CRWSessionController : NSObject<NSCopying>

@property(nonatomic, readonly, assign) NSInteger currentNavigationIndex;
@property(nonatomic, readonly, assign) NSInteger previousNavigationIndex;
// The index of the pending item if it is in |items|, or -1 if |pendingItem|
// corresponds with a new navigation (created by addPendingItem:).
@property(nonatomic, readwrite, assign) NSInteger pendingItemIndex;
// Indicates whether the page was opened by DOM (e.g. with |window.open|
// JavaScript call or by clicking a link with |_blank| target).
@property(nonatomic, readonly, getter=isOpenedByDOM) BOOL openedByDOM;
@property(nonatomic, readonly, strong)
    CRWSessionCertificatePolicyManager* sessionCertificatePolicyManager;

// The list of CRWSessionEntries in |_entries|'s NavigationItemImpls.
@property(nonatomic, readonly) web::NavigationItemList items;
// The number of elements in |self.items|.
@property(nonatomic, readonly) NSUInteger itemCount;
// The current NavigationItem.  During a pending navigation, returns the
// NavigationItem for that navigation.  If a transient NavigationItem exists,
// this NavigationItem will be returned.
@property(nonatomic, readonly) web::NavigationItemImpl* currentItem;
// Returns the NavigationItem whose URL should be displayed to the user.
@property(nonatomic, readonly) web::NavigationItemImpl* visibleItem;
// Returns the NavigationItem corresponding to a load for which no data has yet
// been received.
@property(nonatomic, readonly) web::NavigationItemImpl* pendingItem;
// Returns the NavigationItem corresponding with a transient navigation (i.e.
// SSL interstitials).
@property(nonatomic, readonly) web::NavigationItemImpl* transientItem;
// Returns the NavigationItem corresponding with the last committed load.
@property(nonatomic, readonly) web::NavigationItemImpl* lastCommittedItem;
// Returns the NavigationItem corresponding with the previously loaded page.
@property(nonatomic, readonly) web::NavigationItemImpl* previousItem;
// Returns most recent NavigationItem that is not a redirect. Returns nil if
// |items| is empty.
@property(nonatomic, readonly) web::NavigationItemImpl* lastUserItem;
// Returns a list of all non-redirected NavigationItems whose index precedes
// |currentNavigationIndex|.
@property(nonatomic, readonly) web::NavigationItemList backwardItems;
// Returns a list of all non-redirected NavigationItems whose index follow
// |currentNavigationIndex|.
@property(nonatomic, readonly) web::NavigationItemList forwardItems;

// DEPRECATED: Don't add new usage of these properties.  Instead, use the
// NavigationItem versions of these properties above.
@property(nonatomic, readonly, strong) NSArray* entries;
@property(nonatomic, readonly, strong) CRWSessionEntry* currentEntry;
@property(nonatomic, readonly, strong) CRWSessionEntry* visibleEntry;
@property(nonatomic, readonly, strong) CRWSessionEntry* pendingEntry;
@property(nonatomic, readonly, strong) CRWSessionEntry* transientEntry;
@property(nonatomic, readonly, strong) CRWSessionEntry* lastCommittedEntry;
@property(nonatomic, readonly, strong) CRWSessionEntry* previousEntry;
@property(nonatomic, readonly, strong) CRWSessionEntry* lastUserEntry;
@property(nonatomic, readonly, weak) NSArray* backwardEntries;
@property(nonatomic, readonly, weak) NSArray* forwardEntries;

// CRWSessionController doesn't have public constructors. New
// CRWSessionControllers are created by deserialization, or via a
// NavigationManager.

// Sets the corresponding NavigationManager.
- (void)setNavigationManager:(web::NavigationManagerImpl*)navigationManager;
// Sets the corresponding BrowserState.
- (void)setBrowserState:(web::BrowserState*)browserState;

// Add a new item with the given url, referrer, and navigation type, making it
// the current item. If pending item is the same as current item, this does
// nothing. |referrer| may be nil if there isn't one. The item starts out as
// pending, and will be lost unless |-commitPendingItem| is called.
- (void)addPendingItem:(const GURL&)url
              referrer:(const web::Referrer&)referrer
            transition:(ui::PageTransition)type
        initiationType:(web::NavigationInitiationType)initiationType;

// Updates the URL of the yet to be committed pending item. Useful for page
// redirects. Does nothing if there is no pending item.
- (void)updatePendingItem:(const GURL&)url;

// Commits the current pending item. No changes are made to the item during
// this process, it is just moved from pending to committed.
// TODO(pinkerton): Desktop Chrome broadcasts a notification here, should we?
- (void)commitPendingItem;

// Adds a transient item with the given URL. A transient item will be
// discarded on any navigation.
// TODO(stuartmorgan): Make this work more like upstream, where the item can
// be made from outside and then handed in.
- (void)addTransientItemWithURL:(const GURL&)URL;

// Creates a new CRWSessionEntry with the given URL and state object. A state
// object is a serialized generic JavaScript object that contains details of the
// UI's state for a given CRWSessionEntry/URL. The current item's URL is the
// new item's referrer.
- (void)pushNewItemWithURL:(const GURL&)URL
               stateObject:(NSString*)stateObject
                transition:(ui::PageTransition)transition;

// Updates the URL and state object for the current item.
- (void)updateCurrentItemWithURL:(const GURL&)url
                     stateObject:(NSString*)stateObject;

// Removes the pending and transient NavigationItems.
- (void)discardNonCommittedItems;

// Inserts history state from |otherController| to the front of |items|.  This
// function transfers ownership of |otherController|'s NavigationItems to the
// receiver.
- (void)insertStateFromSessionController:(CRWSessionController*)otherController;

// Sets |currentNavigationIndex_| to the |index| if it's in the entries bounds.
- (void)goToItemAtIndex:(NSInteger)index;

// Removes the item at |index| after discarding any noncomitted entries.
// |index| must not be the index of the last committed item, or a noncomitted
// item.
- (void)removeItemAtIndex:(NSInteger)index;

// Determines whether a navigation between |firstEntry| and |secondEntry| is a
// same-document navigation.  Entries can be passed in any order.
- (BOOL)isSameDocumentNavigationBetweenItem:(web::NavigationItem*)firstItem
                                    andItem:(web::NavigationItem*)secondItem;

// Returns the index of |item| in |items|.
- (NSInteger)indexOfItem:(const web::NavigationItem*)item;

// Returns the item at |index| in |items|.
- (web::NavigationItemImpl*)itemAtIndex:(NSInteger)index;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_SESSION_CONTROLLER_H_
