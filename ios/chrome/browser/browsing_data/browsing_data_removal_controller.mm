// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/browsing_data_removal_controller.h"

#import <WebKit/WebKit.h>

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/hash_tables.h"
#include "base/ios/callback_counter.h"
#include "base/logging.h"
#import "base/mac/bind_objc_block.h"
#include "base/memory/ref_counted.h"
#include "components/open_from_clipboard/clipboard_recent_content.h"
#include "components/signin/ios/browser/account_consistency_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#include "ios/chrome/browser/browsing_data/ios_chrome_browsing_data_remover.h"
#include "ios/chrome/browser/sessions/session_util.h"
#include "ios/chrome/browser/signin/account_consistency_service_factory.h"
#import "ios/chrome/browser/snapshots/snapshots_util.h"
#import "ios/chrome/browser/ui/external_file_remover.h"
#import "ios/chrome/browser/ui/external_file_remover_factory.h"
#include "ios/web/public/web_thread.h"
#import "ios/web/public/web_view_creation_util.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#include "net/cookies/cookie_store.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Empty callback used by DeleteAllCreatedBetweenAsync below.
void DoNothing(uint32_t n) {}
}

@interface BrowsingDataRemovalController ()
// Removes browsing data that is created by web views associated with
// |browserState|. |mask| is obtained from BrowsingDataRemoveMask. |deleteBegin|
// defines the begin time from which the data has to be removed, up to the
// present time. |completionHandler| is called when this operation finishes.
// This method finishes removal of the browsing data even if |browserState| is
// destroyed after this method call.
- (void)
removeWebViewCreatedBrowsingDataFromBrowserState:
    (ios::ChromeBrowserState*)browserState
                                            mask:(BrowsingDataRemoveMask)mask
                                     deleteBegin:(base::Time)deleteBegin
                               completionHandler:
                                   (ProceduralBlock)completionHandler;
// Removes browsing data that is created by WKWebViews associated with
// |browserState|. |browserState| must not be off the record. |mask| is obtained
// from BrowsingDataRemoveMask. |deleteBegin| defines the begin time from which
// the data has to be removed, up to the present time. |completionHandler| is
// called when this operation finishes. This method finishes removal of the
// browsing data even if |browserState| is destroyed after this method call.
- (void)
removeWKWebViewCreatedBrowsingDataFromBrowserState:
    (ios::ChromeBrowserState*)browserState
                                              mask:(BrowsingDataRemoveMask)mask
                                       deleteBegin:(base::Time)deleteBegin
                                 completionHandler:
                                     (ProceduralBlock)completionHandler;

// Removes browsing data associated with |browserState| that is specific to iOS
// and not removed when the |browserState| is destroyed. |mask| is obtained from
// BrowsingDataRemoveMask |deleteBegin| defines the begin time from which the
// data has to be removed. |browserState| cannot be  null. |completionHandler|
// is called when this operation finishes. This method finishes removal of the
// browsing data even if |browserState| is destroyed after this method call.
- (void)
removeIOSSpecificBrowsingDataFromBrowserState:
    (ios::ChromeBrowserState*)browserState
                                         mask:(BrowsingDataRemoveMask)mask
                                  deleteBegin:(base::Time)deleteBegin
                            completionHandler:
                                (ProceduralBlock)completionHandler;
// Removes browsing data from |browserState| that is persisted on disk. |mask|
// is obtained from BrowsingDataRemoveMask. |browserState| cannot be null and
// must be off the record. This method finishes removal of the browsing data
// even if |browserState| is destroyed after this method call.
- (void)
removeIOSSpecificPersistentIncognitoDataFromBrowserState:
    (ios::ChromeBrowserState*)browserState
                                                    mask:
                                                        (BrowsingDataRemoveMask)
                                                            mask;

// Increments the count of pending removal operations for |browserState|.
// Called when any removal operation for |browserState| starts.
- (void)incrementPendingRemovalCountForBrowserState:
    (ios::ChromeBrowserState*)browserState;
// Decrements the count of pending removal operations for |browserState|.
// Called when a removal operation for |browserState| finishes.
- (void)decrementPendingRemovalCountForBrowserState:
    (ios::ChromeBrowserState*)browserState;

// Removes files received from other applications by |browserState|.
// |completionHandler| is called when the files have been removed.
- (void)removeExternalFilesForBrowserState:
            (ios::ChromeBrowserState*)browserState
                         completionHandler:(ProceduralBlock)completionHandler;
@end

@implementation BrowsingDataRemovalController {
  // Mapping from ChromeBrowserState to the IOSChromeBrowsingDataRemover used
  // for removal of the data on that instance.
  base::flat_map<ios::ChromeBrowserState*,
                 std::unique_ptr<IOSChromeBrowsingDataRemover>>
      _browingDataRemovers;
  // A map that tracks the number of pending removals for a given
  // ChromeBrowserState.
  base::flat_map<ios::ChromeBrowserState*, int> _pendingRemovalCount;
}

- (void)removeBrowsingDataFromBrowserState:
            (ios::ChromeBrowserState*)browserState
                                      mask:(BrowsingDataRemoveMask)mask
                                timePeriod:(browsing_data::TimePeriod)timePeriod
                         completionHandler:(ProceduralBlock)completionHandler {
  DCHECK(browserState);
  DLOG_IF(WARNING, mask == BrowsingDataRemoveMask::REMOVE_NOTHING)
      << "Nothing to remove!";
  // Cookies and server bound certificates should have the same lifetime.
  DCHECK_EQ(
      IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES),
      IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_CHANNEL_IDS));

  [self incrementPendingRemovalCountForBrowserState:browserState];

  ProceduralBlock browsingDataCleared = ^{
    [self decrementPendingRemovalCountForBrowserState:browserState];
    if (AccountConsistencyService* accountConsistencyService =
            ios::AccountConsistencyServiceFactory::GetForBrowserState(
                browserState)) {
      accountConsistencyService->OnBrowsingDataRemoved();
    }
    if (completionHandler) {
      completionHandler();
    }
  };

  scoped_refptr<CallbackCounter> callbackCounter =
      new CallbackCounter(base::BindBlockArc(browsingDataCleared));
  ProceduralBlock decrementCallbackCounterCount = ^{
    callbackCounter->DecrementCount();
  };

  callbackCounter->IncrementCount();
  base::Time beginDeleteTime =
      browsing_data::CalculateBeginDeleteTime(timePeriod);
  [self removeIOSSpecificBrowsingDataFromBrowserState:browserState
                                                 mask:mask
                                          deleteBegin:beginDeleteTime
                                    completionHandler:
                                        decrementCallbackCounterCount];

  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_DOWNLOADS)) {
    DCHECK_EQ(browsing_data::TimePeriod::ALL_TIME, timePeriod)
        << "Partial clearing not supported";
    if (!browserState->IsOffTheRecord()) {
      callbackCounter->IncrementCount();
      [self removeExternalFilesForBrowserState:browserState
                             completionHandler:decrementCallbackCounterCount];
    }
  }

  if (!browserState->IsOffTheRecord()) {
    callbackCounter->IncrementCount();
    auto iterator = _browingDataRemovers.find(browserState);
    if (iterator == _browingDataRemovers.end()) {
      iterator =
          _browingDataRemovers
              .emplace(
                  browserState,
                  std::make_unique<IOSChromeBrowsingDataRemover>(browserState))
              .first;
      DCHECK(iterator != _browingDataRemovers.end());
    }
    iterator->second->Remove(timePeriod, mask, base::BindBlockArc(^{
                               callbackCounter->DecrementCount();
                             }));
  }
}

- (void)
removeIOSSpecificIncognitoBrowsingDataFromBrowserState:
    (ios::ChromeBrowserState*)browserState
                                                  mask:(BrowsingDataRemoveMask)
                                                           mask
                                     completionHandler:
                                         (ProceduralBlock)completionHandler {
  DCHECK(browserState && browserState->IsOffTheRecord());
  [self removeIOSSpecificBrowsingDataFromBrowserState:browserState
                                                 mask:mask
                                          deleteBegin:base::Time()
                                    completionHandler:completionHandler];
}

- (void)
removeIOSSpecificBrowsingDataFromBrowserState:
    (ios::ChromeBrowserState*)browserState
                                         mask:(BrowsingDataRemoveMask)mask
                                  deleteBegin:(base::Time)deleteBegin
                            completionHandler:
                                (ProceduralBlock)completionHandler {
  DCHECK(browserState);
  [self incrementPendingRemovalCountForBrowserState:browserState];

  ProceduralBlock browsingDataCleared = ^{
    [self decrementPendingRemovalCountForBrowserState:browserState];
    if (completionHandler) {
      completionHandler();
    }
  };

  // Note: Before adding any method below, make sure that it can finish clearing
  // browsing data even when |browserState| is destroyed after this method call.

  // If deleting history, clear visited links.
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_HISTORY)) {
    if (!browserState->IsOffTheRecord()) {
      ClipboardRecentContent::GetInstance()->SuppressClipboardContent();
      session_util::DeleteLastSession(browserState);
    }
    // Remove the screenshots taken by the system when backgrounding the
    // application. Partial removal based on timePeriod is not required.
    ClearIOSSnapshots();
  }

  if (browserState->IsOffTheRecord()) {
    // In incognito, only data removal for all time is currently supported.
    DCHECK_EQ(base::Time(), deleteBegin);
    [self removeIOSSpecificPersistentIncognitoDataFromBrowserState:browserState
                                                              mask:mask];
  }

  [self removeWebViewCreatedBrowsingDataFromBrowserState:browserState
                                                    mask:mask
                                             deleteBegin:deleteBegin
                                       completionHandler:browsingDataCleared];
}

- (void)
removeWebViewCreatedBrowsingDataFromBrowserState:
    (ios::ChromeBrowserState*)browserState
                                            mask:(BrowsingDataRemoveMask)mask
                                     deleteBegin:(base::Time)deleteBegin
                               completionHandler:
                                   (ProceduralBlock)completionHandler {
  // TODO(crbug.com/480654): Remove this check once browsing data partitioning
  // between BrowserStates is achieved.
  if (browserState->IsOffTheRecord()) {
    if (completionHandler) {
      dispatch_async(dispatch_get_main_queue(), completionHandler);
    }
    return;
  }
  scoped_refptr<CallbackCounter> callbackCounter = new CallbackCounter(
      base::BindBlockArc(completionHandler ? completionHandler
                                           : ^{
                                             }));

  // Note: Before adding any method below, make sure that it can finish clearing
  // browsing data even when |browserState| is destroyed after this method call.

  callbackCounter->IncrementCount();
  [self removeWKWebViewCreatedBrowsingDataFromBrowserState:browserState
                                                      mask:mask
                                               deleteBegin:deleteBegin
                                         completionHandler:^{
                                           callbackCounter->DecrementCount();
                                         }];
}

- (void)
removeWKWebViewCreatedBrowsingDataFromBrowserState:
    (ios::ChromeBrowserState*)browserState
                                              mask:(BrowsingDataRemoveMask)mask
                                       deleteBegin:(base::Time)deleteBegin
                                 completionHandler:
                                     (ProceduralBlock)completionHandler {
  scoped_refptr<CallbackCounter> callbackCounter = new CallbackCounter(
      base::BindBlockArc(completionHandler ? completionHandler
                                           : ^{
                                             }));
  ProceduralBlock decrementCallbackCounterCount = ^{
    callbackCounter->DecrementCount();
  };

  // Converts browsing data types from BrowsingDataRemoveMask to
  // WKWebsiteDataStore strings.
  NSMutableSet* dataTypesToRemove = [[NSMutableSet alloc] init];
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_CACHE_STORAGE)) {
    [dataTypesToRemove addObject:WKWebsiteDataTypeDiskCache];
    [dataTypesToRemove addObject:WKWebsiteDataTypeMemoryCache];
  }
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_APPCACHE)) {
    [dataTypesToRemove addObject:WKWebsiteDataTypeOfflineWebApplicationCache];
  }
  WKWebView* markerWKWebView = nil;
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES)) {
    // TODO(crbug.com/661630): This approach of creating a WKWebView to clear
    // cookies is a workaround for
    // https://bugs.webkit.org/show_bug.cgi?id=149078. Remove this, when that
    // bug is fixed. Note: This WKWebView will be released when cookies have
    // been cleared.
    markerWKWebView = web::BuildWKWebView(CGRectZero, browserState);
    [dataTypesToRemove addObject:WKWebsiteDataTypeCookies];
  }
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_LOCAL_STORAGE)) {
    [dataTypesToRemove addObject:WKWebsiteDataTypeSessionStorage];
    [dataTypesToRemove addObject:WKWebsiteDataTypeLocalStorage];
  }
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_WEBSQL)) {
    [dataTypesToRemove addObject:WKWebsiteDataTypeWebSQLDatabases];
  }
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_INDEXEDDB)) {
    [dataTypesToRemove addObject:WKWebsiteDataTypeIndexedDBDatabases];
  }

  ProceduralBlock afterRemovalFromWKWebsiteDataStore = ^{
    if (IsRemoveDataMaskSet(mask,
                            BrowsingDataRemoveMask::REMOVE_VISITED_LINKS)) {
      // TODO(crbug.com/557963): Purging the WKProcessPool is a workaround for
      // the fact that there is no public API to clear visited links in
      // WKWebView. Remove this workaround if/when that API is made public.
      // Note: Purging the WKProcessPool for clearing visisted links does have
      // the side-effect of also losing the in-memory cookies of WKWebView but
      // it is not a problem in practice since there is no UI to only have
      // visited links be removed but not cookies.
      DCHECK(IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES));
      web::WKWebViewConfigurationProvider::FromBrowserState(browserState)
          .Purge();
    }

    decrementCallbackCounterCount();
  };

  if ([dataTypesToRemove count]) {
    callbackCounter->IncrementCount();
    ProceduralBlock removeFromWKWebsiteDataStore = ^{
      NSDate* beginDeleteDate =
          [NSDate dateWithTimeIntervalSince1970:deleteBegin.ToDoubleT()];
      [[WKWebsiteDataStore defaultDataStore]
          removeDataOfTypes:dataTypesToRemove
              modifiedSince:beginDeleteDate
          completionHandler:afterRemovalFromWKWebsiteDataStore];
    };

    if (markerWKWebView) {
      // TODO(crbug.com/661630): Executing JS enables the markerWKWebView to
      // connect to the Networking process. This is so that the
      // -[WKWebsiteDataStore removeDataOfTypes:] API is able to send an IPC
      // message to the Networking process to clear cookies. This has been
      // reverse-engineered by code inspection on the WebKit2 source code and is
      // an undocumented workaround for
      // https://bugs.webkit.org/show_bug.cgi?id=149078. Remove it, when that
      // bug is fixed.
      [markerWKWebView evaluateJavaScript:@""
                        completionHandler:^(id, NSError*) {
                          removeFromWKWebsiteDataStore();
                        }];
    } else {
      removeFromWKWebsiteDataStore();
    }
  }

  // This is to ensure that the caller of this API still gets a callback even
  // when none of the masks matched.
  callbackCounter->IncrementCount();
  dispatch_async(dispatch_get_main_queue(), decrementCallbackCounterCount);
}

- (void)
removeIOSSpecificPersistentIncognitoDataFromBrowserState:
    (ios::ChromeBrowserState*)browserState
                                                    mask:
                                                        (BrowsingDataRemoveMask)
                                                            mask {
  DCHECK(browserState && browserState->IsOffTheRecord());
  // Note: Before adding any method below, make sure that it can finish clearing
  // browsing data even when |browserState| is destroyed after this method call.
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_HISTORY)) {
    session_util::DeleteLastSession(browserState);
  }

  // Cookies and server bound certificates should have the same lifetime.
  DCHECK_EQ(
      IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES),
      IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_CHANNEL_IDS));
  if (IsRemoveDataMaskSet(mask, BrowsingDataRemoveMask::REMOVE_COOKIES)) {
    scoped_refptr<net::URLRequestContextGetter> contextGetter =
        browserState->GetRequestContext();
    base::Closure callback = base::BindBlockArc(^{
    });
    web::WebThread::PostTask(
        web::WebThread::IO, FROM_HERE, base::BindBlockArc(^{
          net::URLRequestContext* requestContext =
              contextGetter->GetURLRequestContext();
          net::ChannelIDService* channelIdService =
              requestContext->channel_id_service();
          DCHECK(channelIdService);
          DCHECK(channelIdService->GetChannelIDStore());
          channelIdService->GetChannelIDStore()->DeleteAll(callback);
          DCHECK(requestContext->cookie_store());
          requestContext->cookie_store()->DeleteAllCreatedBetweenAsync(
              base::Time(), base::Time(), base::Bind(&DoNothing));
        }));
  }
}

- (void)incrementPendingRemovalCountForBrowserState:
    (ios::ChromeBrowserState*)browserState {
  ++_pendingRemovalCount[browserState];
}

- (void)decrementPendingRemovalCountForBrowserState:
    (ios::ChromeBrowserState*)browserState {
  if (_pendingRemovalCount.find(browserState) != _pendingRemovalCount.end()) {
    --_pendingRemovalCount[browserState];
    if (!_pendingRemovalCount[browserState]) {
      _pendingRemovalCount.erase(browserState);
    }
  }
}

- (BOOL)hasPendingRemovalOperations:(ios::ChromeBrowserState*)browserState {
  return _pendingRemovalCount[browserState] != 0;
}

- (void)browserStateDestroyed:(ios::ChromeBrowserState*)browserState {
  _browingDataRemovers.erase(browserState);
  _pendingRemovalCount.erase(browserState);
}

- (void)removeExternalFilesForBrowserState:
            (ios::ChromeBrowserState*)browserState
                         completionHandler:(ProceduralBlock)completionHandler {
  DCHECK(!browserState->IsOffTheRecord());
  base::OnceClosure callback;
  if (completionHandler)
    callback = base::BindBlockArc(completionHandler);

  ExternalFileRemoverFactory::GetForBrowserState(browserState)
      ->RemoveAfterDelay(base::TimeDelta::FromSeconds(0), std::move(callback));
}

@end
