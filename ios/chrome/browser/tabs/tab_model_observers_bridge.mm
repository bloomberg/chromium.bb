// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_model_observers_bridge.h"

#include <map>
#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_observers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabModelObserversBridge ()

// Called when the given WebState favicon has been changed.
- (void)webStateFaviconChanged:(web::WebState*)webState;

@end

namespace {

// Listen to multiple WebFaviconDriver for notification that their WebState's
// favicon has changed and forward the notification to TabModelObserversBridge.
class FaviconDriverObserverBridge : public favicon::FaviconDriverObserver {
 public:
  explicit FaviconDriverObserverBridge(TabModelObserversBridge* bridge);
  ~FaviconDriverObserverBridge() override;

  // Starts/stops listening to a given WebFaviconDriver notifications. The
  // |source| can be null (methods are no-op in that case).
  void Add(favicon::WebFaviconDriver* source);
  void Remove(favicon::WebFaviconDriver* source);

  // favicon::FaviconDriverObserver implementation.
  void OnFaviconUpdated(favicon::FaviconDriver* favicon_driver,
                        NotificationIconType notification_icon_type,
                        const GURL& icon_url,
                        bool icon_url_changed,
                        const gfx::Image& image) override;

 private:
  // The owning TabModelObserversBridge. Notifications will be forwarded to
  // this object.
  __weak TabModelObserversBridge* bridge_;

  // Scoped observer to track the WebFaviconDriver that are listened to.
  ScopedObserver<favicon::FaviconDriver, favicon::FaviconDriverObserver>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(FaviconDriverObserverBridge);
};

FaviconDriverObserverBridge::FaviconDriverObserverBridge(
    TabModelObserversBridge* bridge)
    : bridge_(bridge), scoped_observer_(this) {}

FaviconDriverObserverBridge::~FaviconDriverObserverBridge() = default;

void FaviconDriverObserverBridge::Add(favicon::WebFaviconDriver* source) {
  if (source) {
    DCHECK(!scoped_observer_.IsObserving(source));
    scoped_observer_.Add(source);
  }
}

void FaviconDriverObserverBridge::Remove(favicon::WebFaviconDriver* source) {
  if (source) {
    DCHECK(scoped_observer_.IsObserving(source));
    scoped_observer_.Remove(source);
  }
}

void FaviconDriverObserverBridge::OnFaviconUpdated(
    favicon::FaviconDriver* favicon_driver,
    favicon::FaviconDriverObserver::NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  // It is safe to cast the driver to favicon::WebFaviconDriver* if it is
  // observed as only favicon::WebFaviconDrivers can be added to the
  // ScopedObserver.
  DCHECK(scoped_observer_.IsObserving(favicon_driver));
  web::WebState* web_state =
      static_cast<favicon::WebFaviconDriver*>(favicon_driver)->web_state();
  [bridge_ webStateFaviconChanged:web_state];
}

}  // namespace

@implementation TabModelObserversBridge {
  // The TabModel owning self.
  __weak TabModel* _tabModel;

  // The TabModelObservers that forward events to TabModelObserver instances
  // registered with owning TabModel.
  __weak TabModelObservers* _tabModelObservers;

  // The FaviconDriverObserverBridge used to listen to all the WebState's
  // favicon changed notifications.
  std::unique_ptr<FaviconDriverObserverBridge> _faviconObserver;
}

- (instancetype)initWithTabModel:(TabModel*)tabModel
               tabModelObservers:(TabModelObservers*)tabModelObservers {
  DCHECK(tabModel);
  DCHECK(tabModelObservers);
  if ((self = [super init])) {
    _tabModel = tabModel;
    _tabModelObservers = tabModelObservers;
    _faviconObserver = std::make_unique<FaviconDriverObserverBridge>(self);
  }
  return self;
}

#pragma mark WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)atIndex
           activating:(BOOL)activating {
  DCHECK_GE(atIndex, 0);
  [self webStateInserted:webState];
  [_tabModelObservers tabModel:_tabModel
                  didInsertTab:LegacyTabHelper::GetTabForWebState(webState)
                       atIndex:static_cast<NSUInteger>(atIndex)
                  inForeground:activating];
  [_tabModelObservers tabModelDidChangeTabCount:_tabModel];
}

- (void)webStateList:(WebStateList*)webStateList
     didMoveWebState:(web::WebState*)webState
           fromIndex:(int)fromIndex
             toIndex:(int)toIndex {
  DCHECK_GE(fromIndex, 0);
  DCHECK_GE(toIndex, 0);
  [_tabModelObservers tabModel:_tabModel
                    didMoveTab:LegacyTabHelper::GetTabForWebState(webState)
                     fromIndex:static_cast<NSUInteger>(fromIndex)
                       toIndex:static_cast<NSUInteger>(toIndex)];
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)atIndex {
  DCHECK_GE(atIndex, 0);
  [self webStateInserted:newWebState];

  [_tabModelObservers tabModel:_tabModel
                 didReplaceTab:LegacyTabHelper::GetTabForWebState(oldWebState)
                       withTab:LegacyTabHelper::GetTabForWebState(newWebState)
                       atIndex:static_cast<NSUInteger>(atIndex)];

  [self webStateDetached:oldWebState];
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)atIndex {
  DCHECK_GE(atIndex, 0);
  [_tabModelObservers tabModel:_tabModel
                  didRemoveTab:LegacyTabHelper::GetTabForWebState(webState)
                       atIndex:static_cast<NSUInteger>(atIndex)];
  [_tabModelObservers tabModelDidChangeTabCount:_tabModel];
  [self webStateDetached:webState];
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                 userAction:(BOOL)userAction {
  if (!newWebState)
    return;

  // If there is no new active WebState, then it means that the atIndex will be
  // set to WebStateList::kInvalidIndex, so only check for a positive index if
  // there is a new WebState.
  DCHECK_GE(atIndex, 0);

  Tab* oldTab =
      oldWebState ? LegacyTabHelper::GetTabForWebState(oldWebState) : nil;
  [_tabModelObservers tabModel:_tabModel
            didChangeActiveTab:LegacyTabHelper::GetTabForWebState(newWebState)
                   previousTab:oldTab
                       atIndex:static_cast<NSUInteger>(atIndex)];
}

- (void)webStateList:(WebStateList*)webStateList
    willDetachWebState:(web::WebState*)webState
               atIndex:(int)atIndex {
  DCHECK_GE(atIndex, 0);
  [_tabModelObservers tabModel:_tabModel
                 willRemoveTab:LegacyTabHelper::GetTabForWebState(webState)];
}

#pragma mark Private methods

- (void)webStateFaviconChanged:(web::WebState*)webState {
  DCHECK(webState);
  [_tabModelObservers tabModel:_tabModel
                  didChangeTab:LegacyTabHelper::GetTabForWebState(webState)];
}

- (void)webStateInserted:(web::WebState*)webState {
  _faviconObserver->Add(favicon::WebFaviconDriver::FromWebState(webState));
}

- (void)webStateDetached:(web::WebState*)webState {
  _faviconObserver->Remove(favicon::WebFaviconDriver::FromWebState(webState));
}

@end
