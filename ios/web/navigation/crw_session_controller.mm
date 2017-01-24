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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* const kCertificatePolicyManagerKey = @"certificatePolicyManager";
NSString* const kCurrentNavigationIndexKey = @"currentNavigationIndex";
NSString* const kEntriesKey = @"entries";
NSString* const kLastVisitedTimestampKey = @"lastVisitedTimestamp";
NSString* const kOpenerIdKey = @"openerId";
NSString* const kOpenedByDOMKey = @"openedByDOM";
NSString* const kOpenerNavigationIndexKey = @"openerNavigationIndex";
NSString* const kPreviousNavigationIndexKey = @"previousNavigationIndex";
NSString* const kTabIdKey = @"tabId";
NSString* const kWindowNameKey = @"windowName";
}  // namespace

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

  // The window name associated with the session.
  NSString* _windowName;

   // Stores the certificate policies decided by the user.
  CRWSessionCertificatePolicyManager* _sessionCertificatePolicyManager;

  // The timestamp of the last time this tab is visited, represented in time
  // interval since 1970.
  NSTimeInterval _lastVisitedTimestamp;

  // If |YES|, override |currentEntry.useDesktopUserAgent| and create the
  // pending entry using the desktop user agent.
  BOOL _useDesktopUserAgentForNextPendingEntry;

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
@property(nonatomic, readwrite, copy) NSString* tabId;
@property(nonatomic, readwrite, strong) NSArray* entries;
@property(nonatomic, readwrite, strong)
    CRWSessionCertificatePolicyManager* sessionCertificatePolicyManager;

- (NSString*)uniqueID;
// Removes all entries after currentNavigationIndex_.
- (void)clearForwardEntries;
// Discards the transient entry, if any.
- (void)discardTransientEntry;
// Create a new autoreleased session entry.
- (CRWSessionEntry*)sessionEntryWithURL:(const GURL&)url
                               referrer:(const web::Referrer&)referrer
                             transition:(ui::PageTransition)transition
                    useDesktopUserAgent:(BOOL)useDesktopUserAgent
                      rendererInitiated:(BOOL)rendererInitiated;
// Returns YES if the PageTransition for the underlying navigationItem at
// |index| in |entries_| has ui::PAGE_TRANSITION_IS_REDIRECT_MASK.
- (BOOL)isRedirectTransitionForEntryAtIndex:(NSInteger)index;
@end

@implementation CRWSessionController

@synthesize tabId = _tabId;
@synthesize currentNavigationIndex = _currentNavigationIndex;
@synthesize previousNavigationIndex = _previousNavigationIndex;
@synthesize pendingEntryIndex = _pendingEntryIndex;
@synthesize entries = _entries;
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
    _entries = [NSMutableArray array];
    _lastVisitedTimestamp = [[NSDate date] timeIntervalSince1970];
    _currentNavigationIndex = -1;
    _previousNavigationIndex = -1;
    _pendingEntryIndex = -1;
    _sessionCertificatePolicyManager =
        [[CRWSessionCertificatePolicyManager alloc] init];
  }
  return self;
}

- (id)initWithNavigationItems:
          (std::vector<std::unique_ptr<web::NavigationItem>>)items
                 currentIndex:(NSUInteger)currentIndex
                 browserState:(web::BrowserState*)browserState {
  self = [super init];
  if (self) {
    _tabId = [[self uniqueID] copy];
    _openerId = nil;
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
    _pendingEntryIndex = -1;
    _lastVisitedTimestamp = [[NSDate date] timeIntervalSince1970];
    _sessionCertificatePolicyManager =
        [[CRWSessionCertificatePolicyManager alloc] init];
  }
  return self;
}

- (id)initWithCoder:(NSCoder*)aDecoder {
  self = [super init];
  if (self) {
    NSString* uuid = [aDecoder decodeObjectForKey:kTabIdKey];
    if (!uuid)
      uuid = [self uniqueID];

    self.windowName = [aDecoder decodeObjectForKey:kWindowNameKey];
    _tabId = [uuid copy];
    _openerId = [[aDecoder decodeObjectForKey:kOpenerIdKey] copy];
    _openedByDOM = [aDecoder decodeBoolForKey:kOpenedByDOMKey];
    _openerNavigationIndex =
        [aDecoder decodeIntForKey:kOpenerNavigationIndexKey];
    _currentNavigationIndex =
        [aDecoder decodeIntForKey:kCurrentNavigationIndexKey];
    _previousNavigationIndex =
        [aDecoder decodeIntForKey:kPreviousNavigationIndexKey];
    _pendingEntryIndex = -1;
    _lastVisitedTimestamp =
       [aDecoder decodeDoubleForKey:kLastVisitedTimestampKey];
    NSMutableArray* temp =
        [NSMutableArray arrayWithArray:
            [aDecoder decodeObjectForKey:kEntriesKey]];
    _entries = temp;
    // Prior to M34, 0 was used as "no index" instead of -1; adjust for that.
    if (![_entries count])
      _currentNavigationIndex = -1;
    _sessionCertificatePolicyManager =
        [aDecoder decodeObjectForKey:kCertificatePolicyManagerKey];
    if (!_sessionCertificatePolicyManager) {
      _sessionCertificatePolicyManager =
          [[CRWSessionCertificatePolicyManager alloc] init];
    }
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  [aCoder encodeObject:_tabId forKey:kTabIdKey];
  [aCoder encodeObject:_openerId forKey:kOpenerIdKey];
  [aCoder encodeBool:_openedByDOM forKey:kOpenedByDOMKey];
  [aCoder encodeInt:_openerNavigationIndex forKey:kOpenerNavigationIndexKey];
  [aCoder encodeObject:_windowName forKey:kWindowNameKey];
  [aCoder encodeInt:_currentNavigationIndex forKey:kCurrentNavigationIndexKey];
  [aCoder encodeInt:_previousNavigationIndex
             forKey:kPreviousNavigationIndexKey];
  [aCoder encodeDouble:_lastVisitedTimestamp forKey:kLastVisitedTimestampKey];
  [aCoder encodeObject:_entries forKey:kEntriesKey];
  [aCoder encodeObject:_sessionCertificatePolicyManager
                forKey:kCertificatePolicyManagerKey];
  // rendererInitiated is deliberately not preserved, as upstream.
}

- (id)copyWithZone:(NSZone*)zone {
  CRWSessionController* copy = [[[self class] alloc] init];
  copy->_tabId = [_tabId copy];
  copy->_openerId = [_openerId copy];
  copy->_openedByDOM = _openedByDOM;
  copy->_openerNavigationIndex = _openerNavigationIndex;
  copy.windowName = self.windowName;
  copy->_currentNavigationIndex = _currentNavigationIndex;
  copy->_previousNavigationIndex = _previousNavigationIndex;
  copy->_pendingEntryIndex = _pendingEntryIndex;
  copy->_lastVisitedTimestamp = _lastVisitedTimestamp;
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

- (void)setPendingEntryIndex:(NSInteger)index {
  DCHECK_GE(index, -1);
  DCHECK_LT(index, static_cast<NSInteger>(_entries.count));
  _pendingEntryIndex = index;
  CRWSessionEntry* entry = index != -1 ? _entries[index] : nil;
  _pendingEntry.reset(entry);
  DCHECK(_pendingEntryIndex == -1 || _pendingEntry);
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

- (NSString*)description {
  return [NSString
      stringWithFormat:
          @"id: %@\nname: %@\nlast visit: %f\ncurrent index: %" PRIdNS
          @"\nprevious index: %" PRIdNS @"\npending index: %" PRIdNS
                                        @"\n%@\npending: %@\ntransient: %@\n",
          _tabId, self.windowName, _lastVisitedTimestamp,
          _currentNavigationIndex, _previousNavigationIndex, _pendingEntryIndex,
          _entries, _pendingEntry.get(), _transientEntry.get()];
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
  bool safeToShowPending = pendingItem &&
                           !pendingItem->is_renderer_initiated() &&
                           _pendingEntryIndex == -1;
  if (safeToShowPending) {
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

- (void)addPendingEntry:(const GURL&)url
               referrer:(const web::Referrer&)ref
             transition:(ui::PageTransition)trans
      rendererInitiated:(BOOL)rendererInitiated {
  [self discardTransientEntry];
  _pendingEntryIndex = -1;

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
    if (item->GetURL() == url &&
        (!PageTransitionCoreTypeIs(trans, ui::PAGE_TRANSITION_FORM_SUBMIT) ||
         PageTransitionCoreTypeIs(item->GetTransitionType(),
                                  ui::PAGE_TRANSITION_FORM_SUBMIT))) {
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

  BOOL useDesktopUserAgent =
      _useDesktopUserAgentForNextPendingEntry ||
      (self.currentEntry.navigationItem &&
       self.currentEntry.navigationItem->IsOverridingUserAgent());
  _useDesktopUserAgentForNextPendingEntry = NO;
  _pendingEntry.reset([self sessionEntryWithURL:url
                                       referrer:ref
                                     transition:trans
                            useDesktopUserAgent:useDesktopUserAgent
                              rendererInitiated:rendererInitiated]);

  if (_navigationManager && _navigationManager->GetFacadeDelegate()) {
    _navigationManager->GetFacadeDelegate()->OnNavigationItemPending();
  }
}

- (void)updatePendingEntry:(const GURL&)url {
  // If there is no pending entry, navigation is probably happening within the
  // session history. Don't modify the entry list.
  if (!_pendingEntry)
    return;

  web::NavigationItemImpl* item = [_pendingEntry navigationItemImpl];
  if (url != item->GetURL()) {
    // Assume a redirection, and discard any transient entry.
    // TODO(stuartmorgan): Once the current safe browsing code is gone,
    // consider making this a DCHECK that there's no transient entry.
    [self discardTransientEntry];

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

- (void)clearForwardEntries {
  DCHECK_EQ(_pendingEntryIndex, -1);
  [self discardTransientEntry];

  NSInteger forwardEntryStartIndex = _currentNavigationIndex + 1;
  DCHECK(forwardEntryStartIndex >= 0);

  if (forwardEntryStartIndex >= static_cast<NSInteger>([_entries count]))
    return;

  NSRange remove = NSMakeRange(forwardEntryStartIndex,
                               [_entries count] - forwardEntryStartIndex);
  // Store removed items in temporary NSArray so they can be deallocated after
  // their facades.
  base::scoped_nsobject<NSArray> removedItems(
      [_entries subarrayWithRange:remove]);
  [_entries removeObjectsInRange:remove];
  if (_previousNavigationIndex >= forwardEntryStartIndex)
    _previousNavigationIndex = -1;
  if (_navigationManager) {
    _navigationManager->OnNavigationItemsPruned(remove.length);
  }
}

- (void)commitPendingEntry {
  if (_pendingEntry) {
    NSInteger newNavigationIndex = _pendingEntryIndex;
    if (_pendingEntryIndex == -1) {
      [self clearForwardEntries];
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
    _pendingEntryIndex = -1;
  }

  CRWSessionEntry* currentEntry = self.currentEntry;
  web::NavigationItem* item = currentEntry.navigationItem;
  // Update the navigation timestamp now that it's actually happened.
  if (item)
    item->SetTimestamp(_timeSmoother.GetSmoothedTime(base::Time::Now()));

  if (_navigationManager && item)
    _navigationManager->OnNavigationItemCommitted();
  DCHECK_EQ(_pendingEntryIndex, -1);
}

- (void)addTransientEntryWithURL:(const GURL&)URL {
  _transientEntry.reset([self
      sessionEntryWithURL:URL
                 referrer:web::Referrer()
               transition:ui::PAGE_TRANSITION_CLIENT_REDIRECT
      useDesktopUserAgent:NO
        rendererInitiated:NO]);

  web::NavigationItem* navigationItem = [_transientEntry navigationItem];
  DCHECK(navigationItem);
  navigationItem->SetTimestamp(
      _timeSmoother.GetSmoothedTime(base::Time::Now()));
}

- (void)pushNewEntryWithURL:(const GURL&)URL
                stateObject:(NSString*)stateObject
                 transition:(ui::PageTransition)transition {
  DCHECK(![self pendingEntry]);
  DCHECK([self currentEntry]);
  web::NavigationItem* item = [self currentEntry].navigationItem;
  CHECK(
      web::history_state_util::IsHistoryStateChangeValid(item->GetURL(), URL));
  web::Referrer referrer(item->GetURL(), web::ReferrerPolicyDefault);
  bool overrideUserAgent =
      self.currentEntry.navigationItem->IsOverridingUserAgent();
  base::scoped_nsobject<CRWSessionEntry> pushedEntry([self
      sessionEntryWithURL:URL
                 referrer:referrer
               transition:transition
      useDesktopUserAgent:overrideUserAgent
        rendererInitiated:NO]);
  web::NavigationItemImpl* pushedItem = [pushedEntry navigationItemImpl];
  pushedItem->SetSerializedStateObject(stateObject);
  pushedItem->SetIsCreatedFromPushState(true);
  web::SSLStatus& sslStatus = [self currentEntry].navigationItem->GetSSL();
  pushedEntry.get().navigationItem->GetSSL() = sslStatus;

  [self clearForwardEntries];
  // Add the new entry at the end.
  [_entries addObject:pushedEntry];
  _previousNavigationIndex = _currentNavigationIndex;
  self.currentNavigationIndex = [_entries count] - 1;

  if (_navigationManager)
    _navigationManager->OnNavigationItemCommitted();
}

- (void)updateCurrentEntryWithURL:(const GURL&)url
                      stateObject:(NSString*)stateObject {
  DCHECK(!_transientEntry);
  CRWSessionEntry* currentEntry = self.currentEntry;
  web::NavigationItemImpl* currentItem = self.currentEntry.navigationItemImpl;
  currentItem->SetURL(url);
  currentItem->SetSerializedStateObject(stateObject);
  currentItem->SetHasStateBeenReplaced(true);
  currentEntry.navigationItem->SetURL(url);
  // If the change is to a committed entry, notify interested parties.
  if (currentEntry != self.pendingEntry && _navigationManager)
    _navigationManager->OnNavigationItemChanged();
}

- (void)discardNonCommittedEntries {
  [self discardTransientEntry];
  _pendingEntry.reset();
  _pendingEntryIndex = -1;
}

- (void)discardTransientEntry {
  // Keep the entry alive temporarily. There are flows that get the current
  // entry, do some navigation operation, and then try to use that old current
  // entry; since navigations clear the transient entry, these flows might
  // crash. (This should be removable once more session management is handled
  // within this class and/or NavigationManager).
  _transientEntry.reset();
}

- (BOOL)hasPendingEntry {
  return _pendingEntry != nil;
}

- (void)insertStateFromSessionController:(CRWSessionController*)sourceSession {
  DCHECK(sourceSession);
  self.windowName = sourceSession.windowName;

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
  if (_pendingEntryIndex != -1)
    _pendingEntryIndex += lastIndexToCopy + 1;

  DCHECK_LT(static_cast<NSUInteger>(_currentNavigationIndex), _entries.count);
  DCHECK(_pendingEntryIndex == -1 || _pendingEntry);
}

- (void)goToEntryAtIndex:(NSInteger)index {
  if (index < 0 || static_cast<NSUInteger>(index) >= _entries.count)
    return;

  if (index < _currentNavigationIndex) {
    // Going back.
    [self discardNonCommittedEntries];
  } else if (_currentNavigationIndex < index) {
    // Going forward.
    [self discardTransientEntry];
  } else {
    // |delta| is 0, no need to change current navigation index.
    return;
  }

  _previousNavigationIndex = _currentNavigationIndex;
  _currentNavigationIndex = index;
}

- (void)removeEntryAtIndex:(NSInteger)index {
  DCHECK(index < static_cast<NSInteger>([_entries count]));
  DCHECK(index != _currentNavigationIndex);
  DCHECK(index >= 0);

  [self discardNonCommittedEntries];

  [_entries removeObjectAtIndex:index];
  if (_currentNavigationIndex > index)
    _currentNavigationIndex--;
  if (_previousNavigationIndex >= index)
    _previousNavigationIndex--;
}

- (NSArray*)backwardEntries {
  NSMutableArray* entries = [NSMutableArray array];
  for (NSInteger index = _currentNavigationIndex; index > 0; --index) {
    if (![self isRedirectTransitionForEntryAtIndex:index])
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

- (BOOL)isSameDocumentNavigationBetweenEntry:(CRWSessionEntry*)firstEntry
                                    andEntry:(CRWSessionEntry*)secondEntry {
  if (!firstEntry || !secondEntry || firstEntry == secondEntry)
    return NO;
  NSUInteger firstIndex = [_entries indexOfObject:firstEntry];
  NSUInteger secondIndex = [_entries indexOfObject:secondEntry];
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
    if (!web::history_state_util::IsHistoryStateChangeValid(
            firstEntry.navigationItem->GetURL(), item->GetURL()))
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
  while (index > 0 && [self isRedirectTransitionForEntryAtIndex:index]) {
    --index;
  }
  return [_entries objectAtIndex:index];
}

- (void)useDesktopUserAgentForNextPendingEntry {
  if (_pendingEntry)
    [_pendingEntry navigationItem]->SetIsOverridingUserAgent(true);
  else
    _useDesktopUserAgentForNextPendingEntry = YES;
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

- (CRWSessionEntry*)sessionEntryWithURL:(const GURL&)url
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
  return [[CRWSessionEntry alloc] initWithNavigationItem:std::move(item)];
}

- (BOOL)isRedirectTransitionForEntryAtIndex:(NSInteger)index {
  ui::PageTransition transition =
      [_entries[index] navigationItem]->GetTransitionType();
  return (transition & ui::PAGE_TRANSITION_IS_REDIRECT_MASK) ? YES : NO;
}

@end
