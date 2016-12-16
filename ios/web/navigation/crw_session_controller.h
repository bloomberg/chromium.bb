// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_NAVIGATION_CRW_SESSION_CONTROLLER_H_
#define IOS_WEB_NAVIGATION_CRW_SESSION_CONTROLLER_H_

#import <Foundation/Foundation.h>
#include <vector>

#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

@class CRWSessionEntry;
@class CRWSessionCertificatePolicyManager;

namespace web {
class NavigationManagerImpl;
struct Referrer;
}

// A CRWSessionController is similar to a NavigationController object in desktop
// Chrome. It maintains information needed to save/restore a tab with its
// complete session history. There is one of these for each tab.
// DEPRECATED, do not use this class and do not add any methods to it.
// Use web::NavigationManager instead.
// TODO(crbug.com/454984): Remove this class.
@interface CRWSessionController : NSObject<NSCoding, NSCopying>

@property(nonatomic, readonly, copy) NSString* tabId;
@property(nonatomic, readonly, assign) NSInteger currentNavigationIndex;
@property(nonatomic, readonly, assign) NSInteger previousNavigationIndex;
// The index of the pending entry if it is in entries_, or -1 if pendingEntry is
// a new entry (created by addPendingEntry:).
@property(nonatomic, readwrite, assign) NSInteger pendingEntryIndex;
@property(nonatomic, readonly, strong) NSArray* entries;
@property(nonatomic, copy) NSString* windowName;
// Indicates whether the page was opened by DOM (e.g. with |window.open|
// JavaScript call or by clicking a link with |_blank| target).
@property(nonatomic, readonly, getter=isOpenedByDOM) BOOL openedByDOM;
@property(nonatomic, readonly, strong)
    CRWSessionCertificatePolicyManager* sessionCertificatePolicyManager;
// Returns the current entry in the session list, or the pending entry if there
// is a navigation in progress.
@property(nonatomic, readonly, strong) CRWSessionEntry* currentEntry;
// Returns the entry that should be displayed to users (e.g., in the omnibox).
@property(nonatomic, readonly, strong) CRWSessionEntry* visibleEntry;
// Returns the pending entry, if any.
@property(nonatomic, readonly, strong) CRWSessionEntry* pendingEntry;
// Returns the transient entry, if any.
@property(nonatomic, readonly, strong) CRWSessionEntry* transientEntry;
// Returns the last committed entry.
@property(nonatomic, readonly, strong) CRWSessionEntry* lastCommittedEntry;
// Returns the previous entry in the session list, or nil if there isn't any.
@property(nonatomic, readonly, strong) CRWSessionEntry* previousEntry;
@property(nonatomic, assign) NSTimeInterval lastVisitedTimestamp;
@property(nonatomic, readonly, copy) NSString* openerId;
@property(nonatomic, readonly, assign) NSInteger openerNavigationIndex;

// CRWSessionController doesn't have public constructors. New
// CRWSessionControllers are created by deserialization, or via a
// NavigationManager.

// Sets the corresponding NavigationManager.
- (void)setNavigationManager:(web::NavigationManagerImpl*)navigationManager;

// Add a new entry with the given url, referrer, and navigation type, making it
// the current entry. If |url| is the same as the current entry's url, this
// does nothing. |referrer| may be nil if there isn't one. The entry starts
// out as pending, and will be lost unless |-commitPendingEntry| is called.
- (void)addPendingEntry:(const GURL&)url
               referrer:(const web::Referrer&)referrer
             transition:(ui::PageTransition)type
      rendererInitiated:(BOOL)rendererInitiated;

// Updates the URL of the yet to be committed pending entry. Useful for page
// redirects. Does nothing if there is no pending entry.
- (void)updatePendingEntry:(const GURL&)url;

// Commits the current pending entry. No changes are made to the entry during
// this process, it is just moved from pending to committed.
// TODO(pinkerton): Desktop Chrome broadcasts a notification here, should we?
- (void)commitPendingEntry;

// Adds a transient entry with the given URL. A transient entry will be
// discarded on any navigation.
// TODO(stuartmorgan): Make this work more like upstream, where the entry can
// be made from outside and then handed in.
- (void)addTransientEntryWithURL:(const GURL&)URL;

// Creates a new CRWSessionEntry with the given URL and state object. A state
// object is a serialized generic JavaScript object that contains details of the
// UI's state for a given CRWSessionEntry/URL. The current entry's URL is the
// new entry's referrer.
- (void)pushNewEntryWithURL:(const GURL&)URL
                stateObject:(NSString*)stateObject
                 transition:(ui::PageTransition)transition;

// Updates the URL and state object for the current entry.
- (void)updateCurrentEntryWithURL:(const GURL&)url
                      stateObject:(NSString*)stateObject;

- (void)discardNonCommittedEntries;

// Returns YES if there is a pending entry.
- (BOOL)hasPendingEntry;

// Inserts history state from the given CRWSessionController to the front of
// this controller.
- (void)insertStateFromSessionController:(CRWSessionController*)otherController;

// Sets |currentNavigationIndex_| to the |index| if it's in the entries bounds.
- (void)goToEntryAtIndex:(NSInteger)index;

// Removes the entry at |index| after discarding any noncomitted entries.
// |index| must not be the index of the last committed entry, or a noncomitted
// entry.
- (void)removeEntryAtIndex:(NSInteger)index;

// Returns an array containing all of the non-redirected CRWSessionEntry objects
// whose index in |entries_| is less than |currentNavigationIndex_|.
- (NSArray*)backwardEntries;

// Returns an array containing all of the non-redirected CRWSessionEntry objects
// whose index in |entries_| is greater than |currentNavigationIndex_|.
- (NSArray*)forwardEntries;

// Returns the URLs in the entries that are redirected to the current entry.
- (std::vector<GURL>)currentRedirectedUrls;

// Determines whether a navigation between |firstEntry| and |secondEntry| is a
// same-document navigation.  Entries can be passed in any order.
- (BOOL)isSameDocumentNavigationBetweenEntry:(CRWSessionEntry*)firstEntry
                                    andEntry:(CRWSessionEntry*)secondEntry;

// Find the most recent session entry that is not a redirect. Returns nil if
// |entries_| is empty.
- (CRWSessionEntry*)lastUserEntry;

// Set |useDesktopUserAgentForNextPendingEntry_| to YES if there is no pending
// entry, otherwise set |useDesktopUserAgent| in the pending entry.
- (void)useDesktopUserAgentForNextPendingEntry;

@end

#endif  // IOS_WEB_NAVIGATION_CRW_SESSION_CONTROLLER_H_
