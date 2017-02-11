// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_session_controller.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <utility>

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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWSessionController () {
  // Weak pointer back to the owning NavigationManager. This is to facilitate
  // the incremental merging of the two classes.
  web::NavigationManagerImpl* _navigationManager;

  NSString* _tabId;  // Unique id of the tab.
  NSString* _openerId;  // Id of tab who opened this tab, empty/nil if none.
  // Navigation index of the tab which opened this tab. Do not rely on the
  // value of this member variable to indicate whether or not this tab has
  // an opener, as both 0 and -1 are used as navigationIndex values.
  NSInteger _openerNavigationIndex;
  // Identifies the index of the current navigation in the CRWSessionEntry
  // array.
  NSInteger _currentNavigationIndex;
  // Identifies the index of the previous navigation in the CRWSessionEntry
  // array.
  NSInteger _previousNavigationIndex;

  // The window name associated with the session.
  NSString* _windowName;

   // Stores the certificate policies decided by the user.
  CRWSessionCertificatePolicyManager* _sessionCertificatePolicyManager;

  // The timestamp of the last time this tab is visited, represented in time
  // interval since 1970.
  NSTimeInterval _lastVisitedTimestamp;

  // If |YES|, override |currentEntry.useDesktopUserAgent| and create the
  // pending item using the desktop user agent.
  BOOL _useDesktopUserAgentForNextPendingItem;

  // The browser state associated with this CRWSessionController;
  web::BrowserState* _browserState;  // weak

  // Time smoother for navigation item timestamps; see comment in
  // navigation_controller_impl.h
  web::TimeSmoother _timeSmoother;

  // Backing objects for properties of the same name.
  web::ScopedNavigationItemImplList _items;
  // |_pendingItem| only contains a NavigationItem for non-history navigations.
  // For back/forward navigations within session history, _pendingItemIndex will
  // be equal to -1, and self.pendingItem will return an item contained within
  // |_items|.
  std::unique_ptr<web::NavigationItemImpl> _pendingItem;
  std::unique_ptr<web::NavigationItemImpl> _transientItem;
}

// Redefine as readwrite.
@property(nonatomic, readwrite, assign) NSInteger currentNavigationIndex;

// TODO(rohitrao): These properties must be redefined readwrite to work around a
// clang bug. crbug.com/228650
@property(nonatomic, readwrite, copy) NSString* tabId;
@property(nonatomic, readwrite, strong)
    CRWSessionCertificatePolicyManager* sessionCertificatePolicyManager;

// Expose setters for serialization properties.  These are exposed in a category
// in SessionStorageBuilder, and will be removed as ownership of
// their backing ivars moves to NavigationManagerImpl.
@property(nonatomic, readwrite, copy) NSString* openerId;
@property(nonatomic, readwrite, getter=isOpenedByDOM) BOOL openedByDOM;
@property(nonatomic, readwrite, assign) NSInteger openerNavigationIndex;
@property(nonatomic, readwrite, assign) NSInteger previousNavigationIndex;

- (NSString*)uniqueID;
// Removes all items after currentNavigationIndex_.
- (void)clearForwardItems;
// Discards the transient item, if any.
- (void)discardTransientItem;
// Creates a NavigationItemImpl with the specified properties.
- (std::unique_ptr<web::NavigationItemImpl>)
        itemWithURL:(const GURL&)url
           referrer:(const web::Referrer&)referrer
         transition:(ui::PageTransition)transition
useDesktopUserAgent:(BOOL)useDesktopUserAgent
  rendererInitiated:(BOOL)rendererInitiated;
// Returns YES if the PageTransition for the underlying navigationItem at
// |index| in |items| has ui::PAGE_TRANSITION_IS_REDIRECT_MASK.
- (BOOL)isRedirectTransitionForItemAtIndex:(size_t)index;
// Returns the CRWSessionEntry corresponding with |item|.
- (CRWSessionEntry*)entryForItem:(web::NavigationItemImpl*)item;
// Returns an autoreleased NSArray containing CRWSessionEntries corresponding
// with the NavigationItems in |itemList|.
- (NSArray*)entryListForItemList:(const web::NavigationItemList&)itemList;

@end

@implementation CRWSessionController

@synthesize tabId = _tabId;
@synthesize currentNavigationIndex = _currentNavigationIndex;
@synthesize previousNavigationIndex = _previousNavigationIndex;
@synthesize pendingItemIndex = _pendingItemIndex;
@synthesize windowName = _windowName;
@synthesize lastVisitedTimestamp = _lastVisitedTimestamp;
@synthesize openerId = _openerId;
@synthesize openedByDOM = _openedByDOM;
@synthesize openerNavigationIndex = _openerNavigationIndex;
@synthesize sessionCertificatePolicyManager = _sessionCertificatePolicyManager;

- (id)initWithWindowName:(NSString*)windowName
                openerId:(NSString*)openerId
             openedByDOM:(BOOL)openedByDOM
   openerNavigationIndex:(NSInteger)openerIndex
            browserState:(web::BrowserState*)browserState {
  self = [super init];
  if (self) {
    self.windowName = windowName;
    _tabId = [[self uniqueID] copy];
    _openerId = [openerId copy];
    _openedByDOM = openedByDOM;
    _openerNavigationIndex = openerIndex;
    _browserState = browserState;
    _lastVisitedTimestamp = [[NSDate date] timeIntervalSince1970];
    _currentNavigationIndex = -1;
    _previousNavigationIndex = -1;
    _pendingItemIndex = -1;
    _sessionCertificatePolicyManager =
        [[CRWSessionCertificatePolicyManager alloc] init];
  }
  return self;
}

- (id)initWithNavigationItems:(web::ScopedNavigationItemList)items
                 currentIndex:(NSUInteger)currentIndex
                 browserState:(web::BrowserState*)browserState {
  self = [super init];
  if (self) {
    _tabId = [[self uniqueID] copy];
    _openerId = nil;
    _browserState = browserState;
    _items = web::CreateScopedNavigationItemImplList(std::move(items));
    self.currentNavigationIndex = currentIndex;
    if (_items.empty())
      self.currentNavigationIndex = -1;
    _currentNavigationIndex = std::min(
        _currentNavigationIndex, static_cast<NSInteger>(_items.size() - 1));
    _previousNavigationIndex = -1;
    _pendingItemIndex = -1;
    _lastVisitedTimestamp = [[NSDate date] timeIntervalSince1970];
    _sessionCertificatePolicyManager =
        [[CRWSessionCertificatePolicyManager alloc] init];
  }
  return self;
}

#pragma mark - Accessors

- (void)setCurrentNavigationIndex:(NSInteger)currentNavigationIndex {
  if (_currentNavigationIndex != currentNavigationIndex) {
    _currentNavigationIndex = currentNavigationIndex;
    if (_navigationManager)
      _navigationManager->RemoveTransientURLRewriters();
  }
}

- (void)setPendingItemIndex:(NSInteger)pendingItemIndex {
  DCHECK_GE(pendingItemIndex, -1);
  DCHECK_LT(pendingItemIndex, static_cast<NSInteger>(self.items.size()));
  _pendingItemIndex = pendingItemIndex;
  DCHECK(_pendingItemIndex == -1 || self.pendingItem);
}

- (const web::ScopedNavigationItemImplList&)items {
  return _items;
}

- (web::NavigationItemImpl*)currentItem {
  if (self.transientItem)
    return self.transientItem;
  if (self.pendingItem)
    return self.pendingItem;
  return self.lastCommittedItem;
}

- (web::NavigationItemImpl*)visibleItem {
  if (self.transientItem)
    return self.transientItem;
  // Only return the |pendingItem| for new (non-history), browser-initiated
  // navigations in order to prevent URL spoof attacks.
  web::NavigationItemImpl* pendingItem = self.pendingItem;
  bool safeToShowPending = pendingItem &&
                           !pendingItem->is_renderer_initiated() &&
                           _pendingItemIndex == -1;
  if (safeToShowPending)
    return pendingItem;
  return self.lastCommittedItem;
}

- (web::NavigationItemImpl*)pendingItem {
  if (self.pendingItemIndex == -1)
    return _pendingItem.get();
  return self.items[self.pendingItemIndex].get();
}

- (web::NavigationItemImpl*)transientItem {
  return _transientItem.get();
}

- (web::NavigationItemImpl*)lastCommittedItem {
  NSInteger index = self.currentNavigationIndex;
  return index == -1 ? nullptr : self.items[index].get();
}

- (web::NavigationItemImpl*)previousItem {
  NSInteger index = self.previousNavigationIndex;
  return index == -1 || self.items.empty() ? nullptr : self.items[index].get();
}

- (web::NavigationItemImpl*)lastUserItem {
  if (self.items.empty())
    return nil;

  NSInteger index = self.currentNavigationIndex;
  // This will return the first NavigationItem if all other items are
  // redirects, regardless of the transition state of the first item.
  while (index > 0 && [self isRedirectTransitionForItemAtIndex:index])
    --index;

  return self.items[index].get();
}

- (web::NavigationItemList)backwardItems {
  web::NavigationItemList items;
  for (size_t index = _currentNavigationIndex; index > 0; --index) {
    if (![self isRedirectTransitionForItemAtIndex:index])
      items.push_back(self.items[index - 1].get());
  }
  return items;
}

- (web::NavigationItemList)forwardItems {
  web::NavigationItemList items;
  NSUInteger lastNonRedirectedIndex = _currentNavigationIndex + 1;
  while (lastNonRedirectedIndex < self.items.size()) {
    web::NavigationItem* item = self.items[lastNonRedirectedIndex].get();
    if (!ui::PageTransitionIsRedirect(item->GetTransitionType()))
      items.push_back(item);
    ++lastNonRedirectedIndex;
  }
  return items;
}

// DEPRECATED
- (NSArray*)entries {
  return [self entryListForItemList:web::CreateNavigationItemList(_items)];
}

// DEPRECATED
- (CRWSessionEntry*)currentEntry {
  return [self entryForItem:self.currentItem];
}

// DEPRECATED
- (CRWSessionEntry*)visibleEntry {
  return [self entryForItem:self.visibleItem];
}

// DEPRECATED
- (CRWSessionEntry*)pendingEntry {
  return [self entryForItem:self.pendingItem];
}

// DEPRECATED
- (CRWSessionEntry*)transientEntry {
  return [self entryForItem:self.transientItem];
}

// DEPRECATED
- (CRWSessionEntry*)lastCommittedEntry {
  return [self entryForItem:self.lastCommittedItem];
}

// DEPRECATED
- (CRWSessionEntry*)previousEntry {
  return [self entryForItem:self.previousItem];
}

// DEPRECATED
- (CRWSessionEntry*)lastUserEntry {
  return [self entryForItem:self.lastUserItem];
}

// DEPRECATED
- (NSArray*)backwardEntries {
  return [self entryListForItemList:self.backwardItems];
}

// DEPRECATED
- (NSArray*)forwardEntries {
  return [self entryListForItemList:self.forwardItems];
}

#pragma mark - NSObject

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"id: %@\nname: %@\nlast visit: %f\ncurrent index: %" PRIdNS
          @"\nprevious index: %" PRIdNS @"\npending index: %" PRIdNS
                                        @"\n%@\npending: %@\ntransient: %@\n",
          _tabId, self.windowName, _lastVisitedTimestamp,
          _currentNavigationIndex, _previousNavigationIndex, _pendingItemIndex,
          self.entries, self.pendingEntry, self.transientEntry];
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  CRWSessionController* copy = [[[self class] alloc] init];
  copy->_tabId = [_tabId copy];
  copy->_openerId = [_openerId copy];
  copy->_openedByDOM = _openedByDOM;
  copy->_openerNavigationIndex = _openerNavigationIndex;
  copy.windowName = self.windowName;
  copy->_currentNavigationIndex = _currentNavigationIndex;
  copy->_previousNavigationIndex = _previousNavigationIndex;
  copy->_pendingItemIndex = _pendingItemIndex;
  copy->_lastVisitedTimestamp = _lastVisitedTimestamp;
  copy->_sessionCertificatePolicyManager =
      [_sessionCertificatePolicyManager copy];
  for (size_t index = 0; index < self.items.size(); ++index) {
    std::unique_ptr<web::NavigationItemImpl> itemCopy(
        new web::NavigationItemImpl(*self.items[index].get()));
    copy->_items.push_back(std::move(itemCopy));
  }
  return copy;
}

#pragma mark - Public

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

- (void)addPendingItem:(const GURL&)url
              referrer:(const web::Referrer&)ref
            transition:(ui::PageTransition)trans
     rendererInitiated:(BOOL)rendererInitiated {
  [self discardTransientItem];
  self.pendingItemIndex = -1;

  // Don't create a new item if it's already the same as the current item,
  // allowing this routine to be called multiple times in a row without issue.
  // Note: CRWSessionController currently has the responsibility to distinguish
  // between new navigations and history stack navigation, hence the inclusion
  // of specific transiton type logic here, in order to make it reliable with
  // real-world observed behavior.
  // TODO(crbug.com/676129): Fix the way changes are detected/reported elsewhere
  // in the web layer so that this hack can be removed.
  // Remove the workaround code from -presentSafeBrowsingWarningForResource:.
  web::NavigationItemImpl* currentItem = self.currentItem;
  if (currentItem && currentItem->GetURL() == url &&
      (!PageTransitionCoreTypeIs(trans, ui::PAGE_TRANSITION_FORM_SUBMIT) ||
       PageTransitionCoreTypeIs(currentItem->GetTransitionType(),
                                ui::PAGE_TRANSITION_FORM_SUBMIT))) {
    // Send the notification anyway, to preserve old behavior. It's unknown
    // whether anything currently relies on this, but since both this whole
    // hack and the content facade will both be going away, it's not worth
    // trying to unwind.
    if (_navigationManager && _navigationManager->GetFacadeDelegate())
      _navigationManager->GetFacadeDelegate()->OnNavigationItemPending();
    return;
  }

  BOOL useDesktopUserAgent =
      _useDesktopUserAgentForNextPendingItem ||
      (currentItem && currentItem->IsOverridingUserAgent());
  _useDesktopUserAgentForNextPendingItem = NO;
  _pendingItem = [self itemWithURL:url
                          referrer:ref
                        transition:trans
               useDesktopUserAgent:useDesktopUserAgent
                 rendererInitiated:rendererInitiated];

  if (_navigationManager && _navigationManager->GetFacadeDelegate())
    _navigationManager->GetFacadeDelegate()->OnNavigationItemPending();
}

- (void)updatePendingItem:(const GURL&)url {
  // If there is no pending item, navigation is probably happening within the
  // session history. Don't modify the item list.
  web::NavigationItemImpl* item = self.pendingItem;
  if (!item)
    return;

  if (url != item->GetURL()) {
    // Assume a redirection, and discard any transient item.
    // TODO(stuartmorgan): Once the current safe browsing code is gone,
    // consider making this a DCHECK that there's no transient item.
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
  if (_navigationManager && _navigationManager->GetFacadeDelegate())
    _navigationManager->GetFacadeDelegate()->OnNavigationItemPending();
}

- (void)clearForwardItems {
  DCHECK_EQ(self.pendingItemIndex, -1);
  [self discardTransientItem];

  NSInteger forwardItemStartIndex = _currentNavigationIndex + 1;
  DCHECK(forwardItemStartIndex >= 0);

  size_t itemCount = self.items.size();
  if (forwardItemStartIndex >= static_cast<NSInteger>(itemCount))
    return;

  if (_previousNavigationIndex >= forwardItemStartIndex)
    _previousNavigationIndex = -1;
  if (_navigationManager)
    _navigationManager->OnNavigationItemsPruned(self.items.size() -
                                                forwardItemStartIndex);

  // Remove the NavigationItems.
  _items.erase(_items.begin() + forwardItemStartIndex, _items.end());
}

- (void)commitPendingItem {
  if (self.pendingItem) {
    // Once an item is committed it's not renderer-initiated any more. (Matches
    // the implementation in NavigationController.)
    self.pendingItem->ResetForCommit();

    NSInteger newNavigationIndex = self.pendingItemIndex;
    if (newNavigationIndex == -1) {
      [self clearForwardItems];
      // Add the new item at the end.
      _items.push_back(std::move(_pendingItem));
      newNavigationIndex = self.items.size() - 1;
    }
    _previousNavigationIndex = _currentNavigationIndex;
    self.currentNavigationIndex = newNavigationIndex;
    self.pendingItemIndex = -1;
  }

  web::NavigationItem* item = self.currentItem;
  // Update the navigation timestamp now that it's actually happened.
  if (item)
    item->SetTimestamp(_timeSmoother.GetSmoothedTime(base::Time::Now()));

  if (_navigationManager && item)
    _navigationManager->OnNavigationItemCommitted();
  DCHECK_EQ(self.pendingItemIndex, -1);
}

- (void)addTransientItemWithURL:(const GURL&)URL {
  _transientItem = [self itemWithURL:URL
                            referrer:web::Referrer()
                          transition:ui::PAGE_TRANSITION_CLIENT_REDIRECT
                 useDesktopUserAgent:NO
                   rendererInitiated:NO];
  _transientItem->SetTimestamp(
      _timeSmoother.GetSmoothedTime(base::Time::Now()));
}

- (void)pushNewItemWithURL:(const GURL&)URL
               stateObject:(NSString*)stateObject
                transition:(ui::PageTransition)transition {
  DCHECK(!self.pendingItem);
  web::NavigationItem* item = self.currentItem;
  DCHECK(item);
  CHECK(
      web::history_state_util::IsHistoryStateChangeValid(item->GetURL(), URL));
  web::Referrer referrer(item->GetURL(), web::ReferrerPolicyDefault);
  bool overrideUserAgent = self.currentItem->IsOverridingUserAgent();
  std::unique_ptr<web::NavigationItemImpl> pushedItem =
      [self itemWithURL:URL
                     referrer:referrer
                   transition:transition
          useDesktopUserAgent:overrideUserAgent
            rendererInitiated:NO];
  pushedItem->SetSerializedStateObject(stateObject);
  pushedItem->SetIsCreatedFromPushState(true);
  web::SSLStatus& sslStatus = self.currentItem->GetSSL();
  pushedItem->GetSSL() = sslStatus;

  [self clearForwardItems];
  // Add the new item at the end.
  _items.push_back(std::move(pushedItem));
  _previousNavigationIndex = _currentNavigationIndex;
  self.currentNavigationIndex = self.items.size() - 1;

  if (_navigationManager)
    _navigationManager->OnNavigationItemCommitted();
}

- (void)updateCurrentItemWithURL:(const GURL&)url
                     stateObject:(NSString*)stateObject {
  DCHECK(!self.transientItem);
  web::NavigationItemImpl* currentItem = self.currentItem;
  currentItem->SetURL(url);
  currentItem->SetSerializedStateObject(stateObject);
  currentItem->SetHasStateBeenReplaced(true);
  currentItem->SetPostData(nil);
  // If the change is to a committed item, notify interested parties.
  if (currentItem != self.pendingItem && _navigationManager)
    _navigationManager->OnNavigationItemChanged();
}

- (void)discardNonCommittedItems {
  [self discardTransientItem];
  _pendingItem.reset();
  self.pendingItemIndex = -1;
}

- (void)discardTransientItem {
  _transientItem.reset();
}

- (void)insertStateFromSessionController:(CRWSessionController*)sourceSession {
  DCHECK(sourceSession);
  self.windowName = sourceSession.windowName;

  // The other session may not have any items, in which case there is nothing
  // to insert.  The other session's currentItem will be bogus
  // in such cases, so ignore it and return early.
  web::ScopedNavigationItemImplList& sourceItems = sourceSession->_items;
  if (sourceItems.empty())
    return;

  // Cycle through the items from the other session and insert them before any
  // items from this session.  Do not copy anything that comes after the other
  // session's current item.
  NSInteger lastIndexToCopy = sourceSession.currentNavigationIndex;
  for (NSInteger i = 0; i <= lastIndexToCopy; ++i) {
    std::unique_ptr<web::NavigationItemImpl> sourceItemCopy(
        new web::NavigationItemImpl(*sourceItems[i].get()));
    _items.insert(_items.begin() + i, std::move(sourceItemCopy));
  }

  // Update state to reflect inserted NavigationItems.
  _previousNavigationIndex = -1;
  _currentNavigationIndex += lastIndexToCopy + 1;
  if (self.pendingItemIndex != -1)
    self.pendingItemIndex += lastIndexToCopy + 1;

  DCHECK_LT(static_cast<NSUInteger>(_currentNavigationIndex),
            self.items.size());
  DCHECK(self.pendingItemIndex == -1 || self.pendingItem);
}

- (void)goToItemAtIndex:(NSInteger)index {
  if (index < 0 || static_cast<NSUInteger>(index) >= self.items.size())
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
  DCHECK(index < static_cast<NSInteger>(self.items.size()));
  DCHECK(index != _currentNavigationIndex);
  DCHECK(index >= 0);

  [self discardNonCommittedItems];

  _items.erase(_items.begin() + index);
  if (_currentNavigationIndex > index)
    _currentNavigationIndex--;
  if (_previousNavigationIndex >= index)
    _previousNavigationIndex--;
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
    web::NavigationItemImpl* item = self.items[i].get();
    // Every item in the sequence has to be created from a hash change or
    // pushState() call.
    if (!item->IsCreatedFromPushState() && !item->IsCreatedFromHashChange())
      return NO;
    // Every item in the sequence has to have a URL that could have been
    // created from a pushState() call.
    if (!web::history_state_util::IsHistoryStateChangeValid(firstItem->GetURL(),
                                                            item->GetURL()))
      return NO;
  }
  return YES;
}

- (void)useDesktopUserAgentForNextPendingItem {
  if (self.pendingItem)
    self.pendingItem->SetIsOverridingUserAgent(true);
  else
    _useDesktopUserAgentForNextPendingItem = YES;
}

- (NSInteger)indexOfItem:(web::NavigationItem*)item {
  DCHECK(item);
  for (size_t index = 0; index < self.items.size(); ++index) {
    if (self.items[index].get() == item)
      return index;
  }
  return NSNotFound;
}

#pragma mark -
#pragma mark Private methods

- (NSString*)uniqueID {
  CFUUIDRef uuidRef = CFUUIDCreate(NULL);
  CFStringRef uuidStringRef = CFUUIDCreateString(NULL, uuidRef);
  CFRelease(uuidRef);

  NSString* uuid =
      [NSString stringWithString:base::mac::ObjCCastStrict<NSString>(
                                     CFBridgingRelease(uuidStringRef))];
  return uuid;
}

- (std::unique_ptr<web::NavigationItemImpl>)
        itemWithURL:(const GURL&)url
           referrer:(const web::Referrer&)referrer
         transition:(ui::PageTransition)transition
useDesktopUserAgent:(BOOL)useDesktopUserAgent
  rendererInitiated:(BOOL)rendererInitiated {
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
  item->SetIsOverridingUserAgent(useDesktopUserAgent);
  item->set_is_renderer_initiated(rendererInitiated);
  return item;
}

- (BOOL)isRedirectTransitionForItemAtIndex:(size_t)index {
  DCHECK_LT(index, self.items.size());
  ui::PageTransition transition = self.items[index]->GetTransitionType();
  return (transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK) ? YES : NO;
}

- (CRWSessionEntry*)entryForItem:(web::NavigationItemImpl*)item {
  if (!item)
    return nil;
  // CRWSessionEntries vended by a CRWSessionController should always correspond
  // with a NavigationItem that is owned by that CRWSessionController.
  DCHECK([self indexOfItem:item] != NSNotFound || item == _pendingItem.get() ||
         item == _transientItem.get());
  return [[CRWSessionEntry alloc] initWithNavigationItem:item];
}

- (NSArray*)entryListForItemList:(const web::NavigationItemList&)itemList {
  NSMutableArray* entryList =
      [[NSMutableArray alloc] initWithCapacity:itemList.size()];
  for (web::NavigationItem* item : itemList) {
    CRWSessionEntry* entry =
        [self entryForItem:static_cast<web::NavigationItemImpl*>(item)];
    [entryList addObject:entry];
  }
  return entryList;
}

@end
