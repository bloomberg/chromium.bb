// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_session_controller.h"

#include <algorithm>
#include <vector>

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/mac/objc_property_releaser.h"
#import "base/mac/scoped_nsobject.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/web/history_state_util.h"
#import "ios/web/navigation/crw_session_certificate_policy_manager.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/crw_session_entry.h"
#include "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_facade_delegate.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/navigation/time_smoother.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/browser_url_rewriter.h"
#include "ios/web/public/referrer.h"
#include "ios/web/public/ssl_status.h"
#include "ios/web/public/user_metrics.h"

using base::UserMetricsAction;

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
NSString* const kXCallbackParametersKey = @"xCallbackParameters";
}  // anonymous namespace

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

  // XCallback parameters used to create (or clobber) the tab. Can be nil.
  XCallbackParameters* _xCallbackParameters;

  base::mac::ObjCPropertyReleaser _propertyReleaser_CRWSessionController;
}

// Redefine as readwrite.
@property(nonatomic, readwrite, assign) NSInteger currentNavigationIndex;

// TODO(rohitrao): These properties must be redefined readwrite to work around a
// clang bug. crbug.com/228650
@property(nonatomic, readwrite, retain) NSString* tabId;
@property(nonatomic, readwrite, retain) NSArray* entries;
@property(nonatomic, readwrite, retain)
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
// Return the PageTransition for the underlying navigationItem at |index| in
// |entries_|
- (ui::PageTransition)transitionForIndex:(NSUInteger)index;
@end

@implementation CRWSessionController

@synthesize tabId = _tabId;
@synthesize currentNavigationIndex = _currentNavigationIndex;
@synthesize previousNavigationIndex = _previousNavigationIndex;
@synthesize entries = _entries;
@synthesize windowName = _windowName;
@synthesize lastVisitedTimestamp = _lastVisitedTimestamp;
@synthesize openerId = _openerId;
@synthesize openedByDOM = _openedByDOM;
@synthesize openerNavigationIndex = _openerNavigationIndex;
@synthesize sessionCertificatePolicyManager = _sessionCertificatePolicyManager;
@synthesize xCallbackParameters = _xCallbackParameters;

- (id)initWithWindowName:(NSString*)windowName
                openerId:(NSString*)openerId
             openedByDOM:(BOOL)openedByDOM
   openerNavigationIndex:(NSInteger)openerIndex
            browserState:(web::BrowserState*)browserState {
  self = [super init];
  if (self) {
    _propertyReleaser_CRWSessionController.Init(self,
                                                [CRWSessionController class]);
    self.windowName = windowName;
    _tabId = [[self uniqueID] retain];
    _openerId = [openerId copy];
    _openedByDOM = openedByDOM;
    _openerNavigationIndex = openerIndex;
    _browserState = browserState;
    _entries = [[NSMutableArray array] retain];
    _lastVisitedTimestamp = [[NSDate date] timeIntervalSince1970];
    _currentNavigationIndex = -1;
    _previousNavigationIndex = -1;
    _sessionCertificatePolicyManager =
        [[CRWSessionCertificatePolicyManager alloc] init];
  }
  return self;
}

- (id)initWithNavigationItems:(ScopedVector<web::NavigationItem>)scoped_items
                 currentIndex:(NSUInteger)currentIndex
                 browserState:(web::BrowserState*)browserState {
  self = [super init];
  if (self) {
    _propertyReleaser_CRWSessionController.Init(self,
                                                [CRWSessionController class]);
    _tabId = [[self uniqueID] retain];
    _openerId = nil;
    _browserState = browserState;

    // Create entries array from list of navigations.
    _entries = [[NSMutableArray alloc] initWithCapacity:scoped_items.size()];
    std::vector<web::NavigationItem*> items;
    scoped_items.release(&items);

    for (size_t i = 0; i < items.size(); ++i) {
      scoped_ptr<web::NavigationItem> item(items[i]);
      base::scoped_nsobject<CRWSessionEntry> entry(
          [[CRWSessionEntry alloc] initWithNavigationItem:item.Pass()]);
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
    _lastVisitedTimestamp = [[NSDate date] timeIntervalSince1970];
    _sessionCertificatePolicyManager =
        [[CRWSessionCertificatePolicyManager alloc] init];
  }
  return self;
}

- (id)initWithCoder:(NSCoder*)aDecoder {
  self = [super init];
  if (self) {
    _propertyReleaser_CRWSessionController.Init(self,
                                                [CRWSessionController class]);
    NSString* uuid = [aDecoder decodeObjectForKey:kTabIdKey];
    if (!uuid)
      uuid = [self uniqueID];

    self.windowName = [aDecoder decodeObjectForKey:kWindowNameKey];
    _tabId = [uuid retain];
    _openerId = [[aDecoder decodeObjectForKey:kOpenerIdKey] copy];
    _openedByDOM = [aDecoder decodeBoolForKey:kOpenedByDOMKey];
    _openerNavigationIndex =
        [aDecoder decodeIntForKey:kOpenerNavigationIndexKey];
    _currentNavigationIndex =
        [aDecoder decodeIntForKey:kCurrentNavigationIndexKey];
    _previousNavigationIndex =
        [aDecoder decodeIntForKey:kPreviousNavigationIndexKey];
    _lastVisitedTimestamp =
       [aDecoder decodeDoubleForKey:kLastVisitedTimestampKey];
    NSMutableArray* temp =
        [NSMutableArray arrayWithArray:
            [aDecoder decodeObjectForKey:kEntriesKey]];
    _entries = [temp retain];
    // Prior to M34, 0 was used as "no index" instead of -1; adjust for that.
    if (![_entries count])
      _currentNavigationIndex = -1;
    _sessionCertificatePolicyManager =
       [[aDecoder decodeObjectForKey:kCertificatePolicyManagerKey] retain];
    if (!_sessionCertificatePolicyManager) {
      _sessionCertificatePolicyManager =
          [[CRWSessionCertificatePolicyManager alloc] init];
    }

    _xCallbackParameters =
        [[aDecoder decodeObjectForKey:kXCallbackParametersKey] retain];
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
  [aCoder encodeObject:_xCallbackParameters forKey:kXCallbackParametersKey];
  // rendererInitiated is deliberately not preserved, as upstream.
}

- (id)copyWithZone:(NSZone*)zone {
  CRWSessionController* copy = [[[self class] alloc] init];
  copy->_propertyReleaser_CRWSessionController.Init(
      copy, [CRWSessionController class]);
  copy->_tabId = [_tabId copy];
  copy->_openerId = [_openerId copy];
  copy->_openedByDOM = _openedByDOM;
  copy->_openerNavigationIndex = _openerNavigationIndex;
  copy.windowName = self.windowName;
  copy->_currentNavigationIndex = _currentNavigationIndex;
  copy->_previousNavigationIndex = _previousNavigationIndex;
  copy->_lastVisitedTimestamp = _lastVisitedTimestamp;
  copy->_entries =
      [[NSMutableArray alloc] initWithArray:_entries copyItems:YES];
  copy->_sessionCertificatePolicyManager =
      [_sessionCertificatePolicyManager copy];
  copy->_xCallbackParameters = [_xCallbackParameters copy];
  return copy;
}

- (void)setCurrentNavigationIndex:(NSInteger)currentNavigationIndex {
  if (_currentNavigationIndex != currentNavigationIndex) {
    _currentNavigationIndex = currentNavigationIndex;
    if (_navigationManager)
      _navigationManager->RemoveTransientURLRewriters();
  }
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
          @"\nprevious index: %" PRIdNS "\n%@\npending: %@\ntransient: %@\n"
          @"xCallback:\n%@\n",
          _tabId, self.windowName, _lastVisitedTimestamp,
          _currentNavigationIndex, _previousNavigationIndex, _entries,
          _pendingEntry.get(), _transientEntry.get(), _xCallbackParameters];
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
  // Only return the pending_entry for:
  //   (a) new (non-history), browser-initiated navigations, and
  //   (b) pending unsafe navigations (while showing the interstitial)
  // in order to prevent URL spoof attacks.
  web::NavigationItemImpl* pendingItem = [_pendingEntry navigationItemImpl];
  if (pendingItem &&
      (!pendingItem->is_renderer_initiated() || pendingItem->IsUnsafe())) {
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

  // Don't create a new entry if it's already the same as the current entry,
  // allowing this routine to be called multiple times in a row without issue.
  // Note: CRWSessionController currently has the responsibility to distinguish
  // between new navigations and history stack navigation, hence the inclusion
  // of specific transiton type logic here, in order to make it reliable with
  // real-world observed behavior.
  // TODO(stuartmorgan): Fix the way changes are detected/reported elsewhere
  // in the web layer so that this hack can be removed.
  // Remove the workaround code from -presentSafeBrowsingWarningForResource:.
  CRWSessionEntry* currentEntry = self.currentEntry;
  if (currentEntry) {
    // If the current entry is known-unsafe (and thus not visible and likely to
    // be removed), ignore any renderer-initated updates and don't worry about
    // sending a notification.
    web::NavigationItem* item = [currentEntry navigationItem];
    if (item->IsUnsafe() && rendererInitiated) {
      return;
    }
    if (item->GetURL() == url &&
        (!PageTransitionCoreTypeIs(trans, ui::PAGE_TRANSITION_FORM_SUBMIT) ||
         PageTransitionCoreTypeIs(item->GetTransitionType(),
                                  ui::PAGE_TRANSITION_FORM_SUBMIT) ||
         item->IsUnsafe())) {
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
  _pendingEntry.reset([[self sessionEntryWithURL:url
                                        referrer:ref
                                      transition:trans
                             useDesktopUserAgent:useDesktopUserAgent
                               rendererInitiated:rendererInitiated] retain]);

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
      [[_entries subarrayWithRange:remove] retain]);
  [_entries removeObjectsInRange:remove];
  if (_previousNavigationIndex >= forwardEntryStartIndex)
    _previousNavigationIndex = -1;
  if (_navigationManager) {
    _navigationManager->OnNavigationItemsPruned(remove.length);
  }
}

- (void)commitPendingEntry {
  if (_pendingEntry) {
    [self clearForwardEntries];
    // Add the new entry at the end.
    [_entries addObject:_pendingEntry];
    _previousNavigationIndex = _currentNavigationIndex;
    self.currentNavigationIndex = [_entries count] - 1;
    // Once an entry is committed it's not renderer-initiated any more. (Matches
    // the implementation in NavigationController.)
    [_pendingEntry navigationItemImpl]->ResetForCommit();
    _pendingEntry.reset();
  }

  CRWSessionEntry* currentEntry = self.currentEntry;
  web::NavigationItem* item = currentEntry.navigationItem;
  // Update the navigation timestamp now that it's actually happened.
  if (item)
    item->SetTimestamp(_timeSmoother.GetSmoothedTime(base::Time::Now()));

  if (_navigationManager && item)
    _navigationManager->OnNavigationItemCommitted();
}

- (void)addTransientEntryWithURL:(const GURL&)URL {
  _transientEntry.reset(
      [[self sessionEntryWithURL:URL
                        referrer:web::Referrer()
                      transition:ui::PAGE_TRANSITION_CLIENT_REDIRECT
             useDesktopUserAgent:NO
               rendererInitiated:NO] retain]);

  web::NavigationItem* navigationItem = [_transientEntry navigationItem];
  DCHECK(navigationItem);
  navigationItem->SetTimestamp(
      _timeSmoother.GetSmoothedTime(base::Time::Now()));
}

- (void)pushNewEntryWithURL:(const GURL&)URL
                stateObject:(NSString*)stateObject
                 transition:(ui::PageTransition)transition {
  DCHECK([self currentEntry]);
  web::NavigationItem* item = [self currentEntry].navigationItem;
  CHECK(
      web::history_state_util::IsHistoryStateChangeValid(item->GetURL(), URL));
  web::Referrer referrer(item->GetURL(), web::ReferrerPolicyDefault);
  bool overrideUserAgent =
      self.currentEntry.navigationItem->IsOverridingUserAgent();
  base::scoped_nsobject<CRWSessionEntry> pushedEntry(
      [[self sessionEntryWithURL:URL
                        referrer:referrer
                      transition:transition
             useDesktopUserAgent:overrideUserAgent
               rendererInitiated:NO] retain]);
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
  currentEntry.navigationItem->SetURL(url);
  // If the change is to a committed entry, notify interested parties.
  if (currentEntry != self.pendingEntry && _navigationManager)
    _navigationManager->OnNavigationItemChanged();
}

- (void)discardNonCommittedEntries {
  [self discardTransientEntry];
  _pendingEntry.reset();
}

- (void)discardTransientEntry {
  // Keep the entry alive temporarily. There are flows that get the current
  // entry, do some navigation operation, and then try to use that old current
  // entry; since navigations clear the transient entry, these flows might
  // crash. (This should be removable once more session management is handled
  // within this class and/or NavigationManager).
  [[_transientEntry retain] autorelease];
  _transientEntry.reset();
}

- (BOOL)hasPendingEntry {
  return _pendingEntry != nil;
}

- (void)copyStateFromAndPrune:(CRWSessionController*)otherSession
                 replaceState:(BOOL)replaceState {
  DCHECK(otherSession);
  if (replaceState) {
    [_entries removeAllObjects];
    self.currentNavigationIndex = -1;
    _previousNavigationIndex = -1;
  }
  self.xCallbackParameters =
      [[otherSession.xCallbackParameters copy] autorelease];
  self.windowName = otherSession.windowName;
  NSInteger numInitialEntries = [_entries count];

  // Cycle through the entries from the other session and insert them before any
  // entries from this session.  Do not copy anything that comes after the other
  // session's current entry unless replaceState is true.
  NSArray* otherEntries = [otherSession entries];

  // The other session may not have any entries, in which case there is nothing
  // to copy or prune.  The other session's currentNavigationEntry will be bogus
  // in such cases, so ignore it and return early.
  // TODO(rohitrao): Do we need to copy over any pending entries?  We might not
  // add the prerendered page into the back/forward history if we don't copy
  // pending entries.
  if (![otherEntries count])
    return;

  NSInteger maxCopyIndex = replaceState ? [otherEntries count] - 1 :
                                          [otherSession currentNavigationIndex];
  for (NSInteger i = 0; i <= maxCopyIndex; ++i) {
    [_entries insertObject:[otherEntries objectAtIndex:i] atIndex:i];
    ++_currentNavigationIndex;
    _previousNavigationIndex = -1;
  }

  // If this CRWSessionController has no entries initially, reset
  // |currentNavigationIndex_| to be in bounds.
  if (!numInitialEntries) {
    if (replaceState) {
      self.currentNavigationIndex = [otherSession currentNavigationIndex];
      _previousNavigationIndex = [otherSession previousNavigationIndex];
    } else {
      self.currentNavigationIndex = maxCopyIndex;
    }
  }
  DCHECK_LT((NSUInteger)_currentNavigationIndex, [_entries count]);
}

- (ui::PageTransition)transitionForIndex:(NSUInteger)index {
  return [[_entries objectAtIndex:index] navigationItem]->GetTransitionType();
}

- (BOOL)canGoBack {
  if ([_entries count] == 0)
    return NO;

  // A transient entry behaves from a user perspective in most ways like a
  // committed entry, so allow going back from a transient entry as long as
  // there is something to go back to.
  if (_transientEntry && [_entries count] > 0)
    return YES;

  NSInteger lastNonRedirectedIndex = _currentNavigationIndex;
  while (lastNonRedirectedIndex >= 0 &&
         ui::PageTransitionIsRedirect(
            [self transitionForIndex:lastNonRedirectedIndex])) {
    --lastNonRedirectedIndex;
  }

  return lastNonRedirectedIndex > 0;
}

- (BOOL)canGoForward {
  // In case there are pending entries return no since when the entry will be
  // committed the history will be cleared from that point forward.
  if (_pendingEntry)
    return NO;
  // If the current index is less than the last element, there are entries to
  // go forward to.
  const NSInteger count = [_entries count];
  return count && _currentNavigationIndex < (count - 1);
}

- (void)goBack {
  if (![self canGoBack])
    return;

  BOOL hadTransientEntry = _transientEntry != nil;

  [self discardNonCommittedEntries];

  // Going back from a transient entry doesn't require anything beyond
  // discarding the pending entry.
  if (hadTransientEntry)
    return;

  web::RecordAction(UserMetricsAction("Back"));
  _previousNavigationIndex = _currentNavigationIndex;
  // To stop the user getting 'stuck' on redirecting pages they weren't even
  // aware existed, it is necessary to pass over pages that would immediately
  // result in a redirect (the entry *before* the redirected page).
  while (_currentNavigationIndex &&
         [self transitionForIndex:_currentNavigationIndex] &
             ui::PAGE_TRANSITION_IS_REDIRECT_MASK) {
    --_currentNavigationIndex;
  }

  if (_currentNavigationIndex)
    --_currentNavigationIndex;
}

- (void)goForward {
  [self discardTransientEntry];

  web::RecordAction(UserMetricsAction("Forward"));
  if (_currentNavigationIndex + 1 < static_cast<NSInteger>([_entries count])) {
    _previousNavigationIndex = _currentNavigationIndex;
    ++_currentNavigationIndex;
  }
  // To reduce the chance of a redirect kicking in (truncating the history
  // stack) we skip over any pages that might do this; we detect this by
  // looking for when the *next* page had rediection transition type (was
  // auto redirected to).
  while (_currentNavigationIndex + 1 <
         (static_cast<NSInteger>([_entries count])) &&
         ([self transitionForIndex:_currentNavigationIndex + 1] &
          ui::PAGE_TRANSITION_IS_REDIRECT_MASK)) {
    ++_currentNavigationIndex;
  }
}

- (void)goDelta:(int)delta {
  if (delta < 0) {
    while ([self canGoBack] && delta < 0) {
      [self goBack];
      ++delta;
    }
  } else {
    while ([self canGoForward] && delta > 0) {
      [self goForward];
      --delta;
    }
  }
}

- (void)goToEntry:(CRWSessionEntry*)entry {
  DCHECK(entry);

  [self discardTransientEntry];

  // Check that |entries_| still contains |entry|. |entry| could have been
  // removed by -clearForwardEntries.
  if ([_entries containsObject:entry])
    self.currentNavigationIndex = [_entries indexOfObject:entry];
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
  NSInteger lastNonRedirectedIndex = _currentNavigationIndex;
  while (lastNonRedirectedIndex >= 0) {
    CRWSessionEntry* entry = [_entries objectAtIndex:lastNonRedirectedIndex];
    if (!ui::PageTransitionIsRedirect(
            entry.navigationItem->GetTransitionType())) {
      [entries addObject:entry];
    }
    --lastNonRedirectedIndex;
  }
  // Remove the currently displayed entry.
  [entries removeObjectAtIndex:0];
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

- (std::vector<GURL>)currentRedirectedUrls {
  std::vector<GURL> results;
  if (_pendingEntry) {
    web::NavigationItem* item = [_pendingEntry navigationItem];
    results.push_back(item->GetURL());

    if (!ui::PageTransitionIsRedirect(item->GetTransitionType()))
      return results;
  }

  if (![_entries count])
    return results;

  NSInteger index = _currentNavigationIndex;
  // Add urls in the redirected entries.
  while (index >= 0) {
    web::NavigationItem* item = [[_entries objectAtIndex:index] navigationItem];
    if (!ui::PageTransitionIsRedirect(item->GetTransitionType()))
      break;
    results.push_back(item->GetURL());
    --index;
  }
  // Add the last non-redirected entry.
  if (index >= 0) {
    web::NavigationItem* item = [[_entries objectAtIndex:index] navigationItem];
    results.push_back(item->GetURL());
  }
  return results;
}

- (BOOL)isPushStateNavigationBetweenEntry:(CRWSessionEntry*)firstEntry
                                 andEntry:(CRWSessionEntry*)secondEntry {
  DCHECK(firstEntry);
  DCHECK(secondEntry);
  if (firstEntry == secondEntry)
    return NO;
  NSUInteger firstIndex = [_entries indexOfObject:firstEntry];
  NSUInteger secondIndex = [_entries indexOfObject:secondEntry];
  if (firstIndex == NSNotFound || secondIndex == NSNotFound)
    return NO;
  NSUInteger startIndex = firstIndex < secondIndex ? firstIndex : secondIndex;
  NSUInteger endIndex = firstIndex < secondIndex ? secondIndex : firstIndex;

  for (NSUInteger i = startIndex + 1; i <= endIndex; i++) {
    web::NavigationItemImpl* item = [_entries[i] navigationItemImpl];
    // Every entry in the sequence has to be created from a pushState() call.
    if (!item->IsCreatedFromPushState())
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
  while (index > 0 &&
         [self transitionForIndex:index] &
         ui::PAGE_TRANSITION_IS_REDIRECT_MASK) {
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
  NSString* uuid = [NSString stringWithString:(NSString*)uuidStringRef];
  CFRelease(uuidStringRef);
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
    scoped_ptr<std::vector<web::BrowserURLRewriter::URLRewriter>>
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
  scoped_ptr<web::NavigationItemImpl> item(new web::NavigationItemImpl());
  item->SetURL(loaded_url);
  item->SetReferrer(referrer);
  item->SetTransitionType(transition);
  item->SetIsOverridingUserAgent(useDesktopUserAgent);
  item->set_is_renderer_initiated(rendererInitiated);
  return [
      [[CRWSessionEntry alloc] initWithNavigationItem:item.Pass()] autorelease];
}

@end
