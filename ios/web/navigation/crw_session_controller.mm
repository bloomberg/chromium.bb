// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_session_controller.h"

#include <stddef.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/format_macros.h"
#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "ios/web/history_state_util.h"
#import "ios/web/navigation/crw_session_certificate_policy_manager.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/crw_session_entry.h"
#import "ios/web/navigation/navigation_item_impl.h"
#include "ios/web/navigation/navigation_manager_facade_delegate.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/navigation/time_smoother.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/browser_url_rewriter.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/ssl_status.h"
#import "ios/web/public/web_client.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWSessionController () {
  // Weak pointer back to the owning NavigationManager. This is to facilitate
  // the incremental merging of the two classes.
  web::NavigationManagerImpl* _navigationManager;

  // Identifies the index of the current navigation in the CRWSessionEntry
  // array.
  NSInteger _currentNavigationIndex;
  // Identifies the index of the previous navigation in the CRWSessionEntry
  // array.
  NSInteger _previousNavigationIndex;
  // Ordered array of |CRWSessionEntry| objects, one for each site in session
  // history. End of the list is the most recent load.
  NSMutableArray* _entries;

  // An entry we haven't gotten a response for yet. This will be discarded
  // when we navigate again. It's used only so we know what the currently
  // displayed tab is.  It backs the property of the same name and should only
  // be set through its setter.
  base::scoped_nsobject<CRWSessionEntry> _pendingEntry;

  // The transient entry, if any. A transient entry is discarded on any
  // navigation, and is used for representing interstitials that need to be
  // represented in the session.  It backs the property of the same name and
  // should only be set through its setter.
  base::scoped_nsobject<CRWSessionEntry> _transientEntry;

   // Stores the certificate policies decided by the user.
  CRWSessionCertificatePolicyManager* _sessionCertificatePolicyManager;

  // The browser state associated with this CRWSessionController;
  web::BrowserState* _browserState;  // weak

  // Time smoother for navigation entry timestamps; see comment in
  // navigation_controller_impl.h
  web::TimeSmoother _timeSmoother;
}

// Redefine as readwrite.
@property(nonatomic, readwrite, assign) NSInteger currentNavigationIndex;

// TODO(rohitrao): These properties must be redefined readwrite to work around a
// clang bug. crbug.com/228650
@property(nonatomic, readwrite, strong) NSArray* entries;
@property(nonatomic, readwrite, strong)
    CRWSessionCertificatePolicyManager* sessionCertificatePolicyManager;

// Expose setters for serialization properties.  These are exposed in a category
// in SessionStorageBuilder, and will be removed as ownership of
// their backing ivars moves to NavigationManagerImpl.
@property(nonatomic, readwrite, getter=isOpenedByDOM) BOOL openedByDOM;
@property(nonatomic, readwrite, assign) NSInteger previousNavigationIndex;

// Removes all entries after currentNavigationIndex_.
- (void)clearForwardItems;
// Discards the transient entry, if any.
- (void)discardTransientItem;
// Create a new autoreleased session entry.
- (CRWSessionEntry*)sessionEntryWithURL:(const GURL&)url
                               referrer:(const web::Referrer&)referrer
                             transition:(ui::PageTransition)transition
                         initiationType:
                             (web::NavigationInitiationType)initiationType;
// Returns YES if the PageTransition for the underlying navigationItem at
// |index| in |entries_| has ui::PAGE_TRANSITION_IS_REDIRECT_MASK.
- (BOOL)isRedirectTransitionForItemAtIndex:(NSInteger)index;
// Returns a NavigationItemList containing the NavigationItems from |entries|.
- (web::NavigationItemList)itemListForEntryList:(NSArray*)entries;
@end

@implementation CRWSessionController

@synthesize currentNavigationIndex = _currentNavigationIndex;
@synthesize previousNavigationIndex = _previousNavigationIndex;
@synthesize pendingItemIndex = _pendingItemIndex;
@synthesize entries = _entries;
@synthesize openedByDOM = _openedByDOM;
@synthesize sessionCertificatePolicyManager = _sessionCertificatePolicyManager;

- (instancetype)initWithBrowserState:(web::BrowserState*)browserState
                         openedByDOM:(BOOL)openedByDOM {
  self = [super init];
  if (self) {
    _openedByDOM = openedByDOM;
    _browserState = browserState;
    _entries = [NSMutableArray array];
    _currentNavigationIndex = -1;
    _previousNavigationIndex = -1;
    _pendingItemIndex = -1;
    _sessionCertificatePolicyManager =
        [[CRWSessionCertificatePolicyManager alloc] init];
  }
  return self;
}

- (instancetype)initWithBrowserState:(web::BrowserState*)browserState
                     navigationItems:(web::ScopedNavigationItemList)items
                        currentIndex:(NSUInteger)currentIndex {
  self = [super init];
  if (self) {
    _browserState = browserState;

    // Create entries array from list of navigations.
    _entries = [[NSMutableArray alloc] initWithCapacity:items.size()];
    for (auto& item : items) {
      base::scoped_nsobject<CRWSessionEntry> entry(
          [[CRWSessionEntry alloc] initWithNavigationItem:std::move(item)]);
      [_entries addObject:entry];
    }
    self.currentNavigationIndex = currentIndex;
    // Prior to M34, 0 was used as "no index" instead of -1; adjust for that.
    if (![_entries count])
      self.currentNavigationIndex = -1;
    if (_currentNavigationIndex >= static_cast<NSInteger>(items.size())) {
      self.currentNavigationIndex = static_cast<NSInteger>(items.size()) - 1;
    }
    _previousNavigationIndex = -1;
    _pendingItemIndex = -1;
    _sessionCertificatePolicyManager =
        [[CRWSessionCertificatePolicyManager alloc] init];
  }
  return self;
}

- (id)copyWithZone:(NSZone*)zone {
  CRWSessionController* copy = [[[self class] alloc] init];
  copy->_openedByDOM = _openedByDOM;
  copy->_currentNavigationIndex = _currentNavigationIndex;
  copy->_previousNavigationIndex = _previousNavigationIndex;
  copy->_pendingItemIndex = _pendingItemIndex;
  copy->_entries =
      [[NSMutableArray alloc] initWithArray:_entries copyItems:YES];
  copy->_sessionCertificatePolicyManager =
      [_sessionCertificatePolicyManager copy];
  return copy;
}

- (void)setCurrentNavigationIndex:(NSInteger)currentNavigationIndex {
  if (_currentNavigationIndex != currentNavigationIndex) {
    _currentNavigationIndex = currentNavigationIndex;
    if (_navigationManager)
      _navigationManager->RemoveTransientURLRewriters();
  }
}

- (void)setPendingItemIndex:(NSInteger)index {
  DCHECK_GE(index, -1);
  DCHECK_LT(index, static_cast<NSInteger>(_entries.count));
  _pendingItemIndex = index;
  CRWSessionEntry* entry = index != -1 ? _entries[index] : nil;
  _pendingEntry.reset(entry);
  DCHECK(_pendingItemIndex == -1 || _pendingEntry);
}

- (void)setNavigationManager:(web::NavigationManagerImpl*)navigationManager {
  _navigationManager = navigationManager;
  if (_navigationManager) {
    // _browserState will be nullptr if CRWSessionController has been
    // initialized with -initWithCoder: method. Take _browserState from
    // NavigationManagerImpl if that's the case.
    if (!_browserState) {
      _browserState = _navigationManager->GetBrowserState();
    }
    DCHECK_EQ(_browserState, _navigationManager->GetBrowserState());
  }
}

- (void)setBrowserState:(web::BrowserState*)browserState {
  _browserState = browserState;
  DCHECK(!_navigationManager ||
         _navigationManager->GetBrowserState() == _browserState);
}

- (NSString*)description {
  return [NSString stringWithFormat:@"current index: %" PRIdNS
                                    @"\nprevious index: %" PRIdNS
                                    @"\npending index: %" PRIdNS
                                    @"\n%@\npending: %@\ntransient: %@\n",
                                    _currentNavigationIndex,
                                    _previousNavigationIndex, _pendingItemIndex,
                                    _entries, _pendingEntry.get(),
                                    _transientEntry.get()];
}

- (web::NavigationItemList)items {
  return [self itemListForEntryList:self.entries];
}

- (NSUInteger)itemCount {
  return self.entries.count;
}

- (web::NavigationItemImpl*)currentItem {
  return self.currentEntry.navigationItemImpl;
}

- (web::NavigationItemImpl*)visibleItem {
  return self.visibleEntry.navigationItemImpl;
}

- (web::NavigationItemImpl*)pendingItem {
  return self.pendingEntry.navigationItemImpl;
}

- (web::NavigationItemImpl*)transientItem {
  return self.transientEntry.navigationItemImpl;
}

- (web::NavigationItemImpl*)lastCommittedItem {
  return self.lastCommittedEntry.navigationItemImpl;
}

- (web::NavigationItemImpl*)previousItem {
  return self.previousEntry.navigationItemImpl;
}

- (web::NavigationItemImpl*)lastUserItem {
  return self.lastUserEntry.navigationItemImpl;
}

- (web::NavigationItemList)backwardItems {
  return [self itemListForEntryList:self.backwardEntries];
}

- (web::NavigationItemList)forwardItems {
  return [self itemListForEntryList:self.forwardEntries];
}

// Returns the current entry in the session list, or the pending entry if there
// is a navigation in progress.
- (CRWSessionEntry*)currentEntry {
  if (_transientEntry)
    return _transientEntry.get();
  if (_pendingEntry)
    return _pendingEntry.get();
  return [self lastCommittedEntry];
}

// See NavigationController::GetVisibleEntry for the motivation for this
// distinction.
- (CRWSessionEntry*)visibleEntry {
  if (_transientEntry)
    return _transientEntry.get();
  // Only return the pending_entry for new (non-history), browser-initiated
  // navigations in order to prevent URL spoof attacks.
  web::NavigationItemImpl* pendingItem = [_pendingEntry navigationItemImpl];

  if (pendingItem) {
    bool isUserInitiated = pendingItem->NavigationInitiationType() ==
                           web::NavigationInitiationType::USER_INITIATED;
    bool safeToShowPending = isUserInitiated && _pendingItemIndex == -1;

    if (safeToShowPending)
      return _pendingEntry.get();
  }

  return [self lastCommittedEntry];
}

- (CRWSessionEntry*)pendingEntry {
  return _pendingEntry.get();
}

- (CRWSessionEntry*)transientEntry {
  return _transientEntry.get();
}

- (CRWSessionEntry*)lastCommittedEntry {
  if (_currentNavigationIndex == -1)
    return nil;
  return [_entries objectAtIndex:_currentNavigationIndex];
}

// Returns the previous entry in the session list, or nil if there isn't any.
- (CRWSessionEntry*)previousEntry {
  if ((_previousNavigationIndex < 0) || (![_entries count]))
    return nil;
  return [_entries objectAtIndex:_previousNavigationIndex];
}

- (void)addPendingItem:(const GURL&)url
              referrer:(const web::Referrer&)ref
            transition:(ui::PageTransition)trans
        initiationType:(web::NavigationInitiationType)initiationType {
  [self discardTransientItem];
  _pendingItemIndex = -1;

  // Don't create a new entry if it's already the same as the current entry,
  // allowing this routine to be called multiple times in a row without issue.
  // Note: CRWSessionController currently has the responsibility to distinguish
  // between new navigations and history stack navigation, hence the inclusion
  // of specific transiton type logic here, in order to make it reliable with
  // real-world observed behavior.
  // TODO(crbug.com/676129): Fix the way changes are detected/reported elsewhere
  // in the web layer so that this hack can be removed.
  // Remove the workaround code from -presentSafeBrowsingWarningForResource:.
  CRWSessionEntry* currentEntry = self.currentEntry;
  if (currentEntry) {
    web::NavigationItem* item = [currentEntry navigationItem];

    BOOL hasSameURL = item->GetURL() == url;
    BOOL isPendingTransitionFormSubmit =
        PageTransitionCoreTypeIs(trans, ui::PAGE_TRANSITION_FORM_SUBMIT);
    BOOL isCurrentTransitionFormSubmit = PageTransitionCoreTypeIs(
        item->GetTransitionType(), ui::PAGE_TRANSITION_FORM_SUBMIT);
    BOOL shouldCreatePendingItem =
        !hasSameURL ||
        (isPendingTransitionFormSubmit && !isCurrentTransitionFormSubmit);

    if (!shouldCreatePendingItem) {
      // Send the notification anyway, to preserve old behavior. It's unknown
      // whether anything currently relies on this, but since both this whole
      // hack and the content facade will both be going away, it's not worth
      // trying to unwind.
      if (_navigationManager && _navigationManager->GetFacadeDelegate()) {
        _navigationManager->GetFacadeDelegate()->OnNavigationItemPending();
      }
      return;
    }
  }

  _pendingEntry.reset([self sessionEntryWithURL:url
                                       referrer:ref
                                     transition:trans
                                 initiationType:initiationType]);

  if (_navigationManager && _navigationManager->GetFacadeDelegate()) {
    _navigationManager->GetFacadeDelegate()->OnNavigationItemPending();
  }
}

- (void)updatePendingItem:(const GURL&)url {
  // If there is no pending entry, navigation is probably happening within the
  // session history. Don't modify the entry list.
  if (!_pendingEntry)
    return;

  web::NavigationItemImpl* item = [_pendingEntry navigationItemImpl];
  if (url != item->GetURL()) {
    // Assume a redirection, and discard any transient entry.
    // TODO(stuartmorgan): Once the current safe browsing code is gone,
    // consider making this a DCHECK that there's no transient entry.
    [self discardTransientItem];

    item->SetURL(url);
    item->SetVirtualURL(url);
    // Redirects (3xx response code), or client side navigation must change
    // POST requests to GETs.
    item->SetPostData(nil);
    item->ResetHttpRequestHeaders();
  }

  // This should probably not be sent if the URLs matched, but that's what was
  // done before, so preserve behavior in case something relies on it.
  if (_navigationManager && _navigationManager->GetFacadeDelegate()) {
    _navigationManager->GetFacadeDelegate()->OnNavigationItemPending();
  }
}

- (void)clearForwardItems {
  DCHECK_EQ(_pendingItemIndex, -1);
  [self discardTransientItem];

  NSInteger forwardItemStartIndex = _currentNavigationIndex + 1;
  DCHECK(forwardItemStartIndex >= 0);

  if (forwardItemStartIndex >= static_cast<NSInteger>([_entries count]))
    return;

  NSRange remove = NSMakeRange(forwardItemStartIndex,
                               [_entries count] - forwardItemStartIndex);
  // Store removed items in temporary NSArray so they can be deallocated after
  // their facades.
  base::scoped_nsobject<NSArray> removedItems(
      [_entries subarrayWithRange:remove]);
  [_entries removeObjectsInRange:remove];
  if (_previousNavigationIndex >= forwardItemStartIndex)
    _previousNavigationIndex = -1;
  if (_navigationManager) {
    _navigationManager->OnNavigationItemsPruned(remove.length);
  }
}

- (void)commitPendingItem {
  if (_pendingEntry) {
    NSInteger newNavigationIndex = _pendingItemIndex;
    if (_pendingItemIndex == -1) {
      [self clearForwardItems];
      // Add the new entry at the end.
      [_entries addObject:_pendingEntry];
      newNavigationIndex = [_entries count] - 1;
    }
    _previousNavigationIndex = _currentNavigationIndex;
    self.currentNavigationIndex = newNavigationIndex;
    // Once an entry is committed it's not renderer-initiated any more. (Matches
    // the implementation in NavigationController.)
    [_pendingEntry navigationItemImpl]->ResetForCommit();
    _pendingEntry.reset();
    _pendingItemIndex = -1;
  }

  CRWSessionEntry* currentEntry = self.currentEntry;
  web::NavigationItem* item = currentEntry.navigationItem;
  // Update the navigation timestamp now that it's actually happened.
  if (item)
    item->SetTimestamp(_timeSmoother.GetSmoothedTime(base::Time::Now()));

  if (_navigationManager && item)
    _navigationManager->OnNavigationItemCommitted();
  DCHECK_EQ(_pendingItemIndex, -1);
}

- (void)addTransientItemWithURL:(const GURL&)URL {
  _transientEntry.reset([self
      sessionEntryWithURL:URL
                 referrer:web::Referrer()
               transition:ui::PAGE_TRANSITION_CLIENT_REDIRECT
           initiationType:web::NavigationInitiationType::USER_INITIATED]);

  web::NavigationItem* navigationItem = [_transientEntry navigationItem];
  DCHECK(navigationItem);
  navigationItem->SetTimestamp(
      _timeSmoother.GetSmoothedTime(base::Time::Now()));
}

- (void)pushNewItemWithURL:(const GURL&)URL
               stateObject:(NSString*)stateObject
                transition:(ui::PageTransition)transition {
  DCHECK(![self pendingEntry]);
  DCHECK([self currentEntry]);

  web::NavigationItem* lastCommittedItem =
      self.lastCommittedEntry.navigationItem;
  CHECK(web::history_state_util::IsHistoryStateChangeValid(
      lastCommittedItem->GetURL(), URL));

  web::Referrer referrer(lastCommittedItem->GetURL(),
                         web::ReferrerPolicyDefault);
  base::scoped_nsobject<CRWSessionEntry> pushedEntry([self
      sessionEntryWithURL:URL
                 referrer:referrer
               transition:transition
           initiationType:web::NavigationInitiationType::USER_INITIATED]);

  web::NavigationItemImpl* pushedItem = [pushedEntry navigationItemImpl];
  pushedItem->SetUserAgentType(lastCommittedItem->GetUserAgentType());
  pushedItem->SetSerializedStateObject(stateObject);
  pushedItem->SetIsCreatedFromPushState(true);
  pushedItem->GetSSL() = lastCommittedItem->GetSSL();

  [self clearForwardItems];
  // Add the new entry at the end.
  [_entries addObject:pushedEntry];
  _previousNavigationIndex = _currentNavigationIndex;
  self.currentNavigationIndex = [_entries count] - 1;

  if (_navigationManager)
    _navigationManager->OnNavigationItemCommitted();
}

- (void)updateCurrentItemWithURL:(const GURL&)url
                     stateObject:(NSString*)stateObject {
  DCHECK(!_transientEntry);
  CRWSessionEntry* currentEntry = self.currentEntry;
  web::NavigationItemImpl* currentItem = self.currentEntry.navigationItemImpl;
  currentItem->SetURL(url);
  currentItem->SetSerializedStateObject(stateObject);
  currentItem->SetHasStateBeenReplaced(true);
  currentItem->SetPostData(nil);
  currentEntry.navigationItem->SetURL(url);
  // If the change is to a committed entry, notify interested parties.
  if (currentEntry != self.pendingEntry && _navigationManager)
    _navigationManager->OnNavigationItemChanged();
}

- (void)discardNonCommittedItems {
  [self discardTransientItem];
  _pendingEntry.reset();
  _pendingItemIndex = -1;
}

- (void)discardTransientItem {
  // Keep the entry alive temporarily. There are flows that get the current
  // entry, do some navigation operation, and then try to use that old current
  // entry; since navigations clear the transient entry, these flows might
  // crash. (This should be removable once more session management is handled
  // within this class and/or NavigationManager).
  _transientEntry.reset();
}

- (void)insertStateFromSessionController:(CRWSessionController*)sourceSession {
  DCHECK(sourceSession);
  // The other session may not have any entries, in which case there is nothing
  // to insert.  The other session's currentNavigationEntry will be bogus
  // in such cases, so ignore it and return early.
  NSArray* sourceEntries = sourceSession.entries;
  if (!sourceEntries.count)
    return;

  // Cycle through the entries from the other session and insert them before any
  // entries from this session.  Do not copy anything that comes after the other
  // session's current entry.
  NSInteger lastIndexToCopy = sourceSession.currentNavigationIndex;
  for (NSInteger i = 0; i <= lastIndexToCopy; ++i) {
    [_entries insertObject:sourceEntries[i] atIndex:i];
  }

  _previousNavigationIndex = -1;
  _currentNavigationIndex += lastIndexToCopy + 1;
  if (_pendingItemIndex != -1)
    _pendingItemIndex += lastIndexToCopy + 1;

  DCHECK_LT(static_cast<NSUInteger>(_currentNavigationIndex), _entries.count);
  DCHECK(_pendingItemIndex == -1 || _pendingEntry);
}

- (void)goToItemAtIndex:(NSInteger)index {
  if (index < 0 || static_cast<NSUInteger>(index) >= _entries.count)
    return;

  if (index < _currentNavigationIndex) {
    // Going back.
    [self discardNonCommittedItems];
  } else if (_currentNavigationIndex < index) {
    // Going forward.
    [self discardTransientItem];
  } else {
    // |delta| is 0, no need to change current navigation index.
    return;
  }

  _previousNavigationIndex = _currentNavigationIndex;
  _currentNavigationIndex = index;
}

- (void)removeItemAtIndex:(NSInteger)index {
  DCHECK(index < static_cast<NSInteger>([_entries count]));
  DCHECK(index != _currentNavigationIndex);
  DCHECK(index >= 0);

  [self discardNonCommittedItems];

  [_entries removeObjectAtIndex:index];
  if (_currentNavigationIndex > index)
    _currentNavigationIndex--;
  if (_previousNavigationIndex >= index)
    _previousNavigationIndex--;
}

- (NSArray*)backwardEntries {
  NSMutableArray* entries = [NSMutableArray array];
  for (NSInteger index = _currentNavigationIndex; index > 0; --index) {
    if (![self isRedirectTransitionForItemAtIndex:index])
      [entries addObject:_entries[index - 1]];
  }
  return entries;
}

- (NSArray*)forwardEntries {
  NSMutableArray* entries = [NSMutableArray array];
  NSUInteger lastNonRedirectedIndex = _currentNavigationIndex + 1;
  while (lastNonRedirectedIndex < [_entries count]) {
    CRWSessionEntry* entry = [_entries objectAtIndex:lastNonRedirectedIndex];
    if (!ui::PageTransitionIsRedirect(
            entry.navigationItem->GetTransitionType())) {
      [entries addObject:entry];
    }
    ++lastNonRedirectedIndex;
  }
  return entries;
}

- (BOOL)isSameDocumentNavigationBetweenItem:(web::NavigationItem*)firstItem
                                    andItem:(web::NavigationItem*)secondItem {
  if (!firstItem || !secondItem || firstItem == secondItem)
    return NO;
  NSUInteger firstIndex = [self indexOfItem:firstItem];
  NSUInteger secondIndex = [self indexOfItem:secondItem];
  if (firstIndex == NSNotFound || secondIndex == NSNotFound)
    return NO;
  NSUInteger startIndex = firstIndex < secondIndex ? firstIndex : secondIndex;
  NSUInteger endIndex = firstIndex < secondIndex ? secondIndex : firstIndex;

  for (NSUInteger i = startIndex + 1; i <= endIndex; i++) {
    web::NavigationItemImpl* item = [_entries[i] navigationItemImpl];
    // Every entry in the sequence has to be created from a hash change or
    // pushState() call.
    if (!item->IsCreatedFromPushState() && !item->IsCreatedFromHashChange())
      return NO;
    // Every entry in the sequence has to have a URL that could have been
    // created from a pushState() call.
    if (!web::history_state_util::IsHistoryStateChangeValid(firstItem->GetURL(),
                                                            item->GetURL()))
      return NO;
  }
  return YES;
}

- (CRWSessionEntry*)lastUserEntry {
  if (![_entries count])
    return nil;

  NSInteger index = _currentNavigationIndex;
  // This will return the first session entry if all other entries are
  // redirects, regardless of the transition state of the first entry.
  while (index > 0 && [self isRedirectTransitionForItemAtIndex:index]) {
    --index;
  }
  return [_entries objectAtIndex:index];
}

- (NSInteger)indexOfItem:(const web::NavigationItem*)item {
  for (NSUInteger index = 0; index < self.entries.count; ++index) {
    if ([self.entries[index] navigationItem] == item)
      return static_cast<NSInteger>(index);
  }
  return NSNotFound;
}

- (web::NavigationItemImpl*)itemAtIndex:(NSInteger)index {
  if (index < 0 || self.entries.count <= static_cast<NSUInteger>(index))
    return nullptr;
  return static_cast<web::NavigationItemImpl*>(
      [self.entries[index] navigationItem]);
}

#pragma mark -
#pragma mark Private methods

- (CRWSessionEntry*)sessionEntryWithURL:(const GURL&)url
                               referrer:(const web::Referrer&)referrer
                             transition:(ui::PageTransition)transition
                         initiationType:
                             (web::NavigationInitiationType)initiationType {
  GURL loaded_url(url);
  BOOL urlWasRewritten = NO;
  if (_navigationManager) {
    std::unique_ptr<std::vector<web::BrowserURLRewriter::URLRewriter>>
        transientRewriters = _navigationManager->GetTransientURLRewriters();
    if (transientRewriters) {
      urlWasRewritten = web::BrowserURLRewriter::RewriteURLWithWriters(
          &loaded_url, _browserState, *transientRewriters.get());
    }
  }
  if (!urlWasRewritten) {
    web::BrowserURLRewriter::GetInstance()->RewriteURLIfNecessary(
        &loaded_url, _browserState);
  }

  std::unique_ptr<web::NavigationItemImpl> item(new web::NavigationItemImpl());
  item->SetOriginalRequestURL(loaded_url);
  item->SetURL(loaded_url);
  item->SetReferrer(referrer);
  item->SetTransitionType(transition);
  item->SetNavigationInitiationType(initiationType);
  if (web::GetWebClient()->IsAppSpecificURL(loaded_url))
    item->SetUserAgentType(web::UserAgentType::NONE);
  return [[CRWSessionEntry alloc] initWithNavigationItem:std::move(item)];
}

- (BOOL)isRedirectTransitionForItemAtIndex:(NSInteger)index {
  ui::PageTransition transition =
      [_entries[index] navigationItem]->GetTransitionType();
  return (transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK) ? YES : NO;
}

- (web::NavigationItemList)itemListForEntryList:(NSArray*)entries {
  web::NavigationItemList list(entries.count);
  for (size_t index = 0; index < entries.count; ++index)
    list[index] = [entries[index] navigationItem];
  return list;
}

@end
