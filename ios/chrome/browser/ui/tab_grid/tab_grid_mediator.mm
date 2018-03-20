// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_mediator.h"

#include <memory>

#include "base/scoped_observer.h"
#include "base/strings/sys_string_conversions.h"
#include "components/favicon/ios/web_favicon_driver.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_consumer.h"
#import "ios/chrome/browser/ui/tab_grid/grid/grid_item.h"
#import "ios/chrome/browser/web/tab_id_tab_helper.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"
#include "ios/chrome/browser/web_state_list/web_state_opener.h"
#include "ios/web/public/web_state/web_state.h"
#import "ios/web/public/web_state/web_state_observer_bridge.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Constructs a GridItem from a |webState|.
GridItem* CreateItem(web::WebState* webState) {
  TabIdTabHelper* tabHelper = TabIdTabHelper::FromWebState(webState);
  GridItem* item = [[GridItem alloc] init];
  item.identifier = tabHelper->tab_id();
  item.title = base::SysUTF16ToNSString(webState->GetTitle());
  return item;
}

// Constructs an array of GridItems from a |webStateList|.
NSArray* CreateItems(WebStateList* webStateList) {
  NSMutableArray* items = [[NSMutableArray alloc] init];
  for (int i = 0; i < webStateList->count(); i++) {
    web::WebState* webState = webStateList->GetWebStateAt(i);
    [items addObject:CreateItem(webState)];
  }
  return [items copy];
}
}  // namespace

@interface TabGridMediator ()<CRWWebStateObserver, WebStateListObserving>
// The list from the tab model.
@property(nonatomic, assign) WebStateList* webStateList;
// The UI consumer to which updates are made.
@property(nonatomic, weak) id<GridConsumer> consumer;
@end

@implementation TabGridMediator {
  // Observers for WebStateList.
  std::unique_ptr<WebStateListObserverBridge> _webStateListObserverBridge;
  std::unique_ptr<ScopedObserver<WebStateList, WebStateListObserver>>
      _scopedWebStateListObserver;
  // Observer for WebStates.
  std::unique_ptr<web::WebStateObserverBridge> _webStateObserverBridge;
  std::unique_ptr<ScopedObserver<web::WebState, web::WebStateObserver>>
      _scopedWebStateObserver;
}

// Public properties.
@synthesize tabModel = _tabModel;
// Private properties.
@synthesize webStateList = _webStateList;
@synthesize consumer = _consumer;

- (instancetype)initWithConsumer:(id<GridConsumer>)consumer {
  if (self = [super init]) {
    _consumer = consumer;
    _webStateListObserverBridge =
        std::make_unique<WebStateListObserverBridge>(self);
    _scopedWebStateListObserver =
        std::make_unique<ScopedObserver<WebStateList, WebStateListObserver>>(
            _webStateListObserverBridge.get());
    _webStateObserverBridge =
        std::make_unique<web::WebStateObserverBridge>(self);
    _scopedWebStateObserver =
        std::make_unique<ScopedObserver<web::WebState, web::WebStateObserver>>(
            _webStateObserverBridge.get());
  }
  return self;
}

#pragma mark - Public properties

- (void)setTabModel:(TabModel*)tabModel {
  _scopedWebStateListObserver->RemoveAll();
  _scopedWebStateObserver->RemoveAll();
  _tabModel = tabModel;
  _webStateList = tabModel.webStateList;
  if (_webStateList) {
    _scopedWebStateListObserver->Add(_webStateList);
    for (int i = 0; i < self.webStateList->count(); i++) {
      web::WebState* webState = self.webStateList->GetWebStateAt(i);
      _scopedWebStateObserver->Add(webState);
    }
    [self populateConsumerItems];
  }
}

#pragma mark - WebStateListObserving

- (void)webStateList:(WebStateList*)webStateList
    didInsertWebState:(web::WebState*)webState
              atIndex:(int)index
           activating:(BOOL)activating {
  [self.consumer insertItem:CreateItem(webState)
                    atIndex:index
              selectedIndex:webStateList->active_index()];
  _scopedWebStateObserver->Add(webState);
}

- (void)webStateList:(WebStateList*)webStateList
     didMoveWebState:(web::WebState*)webState
           fromIndex:(int)fromIndex
             toIndex:(int)toIndex {
  [self.consumer moveItemFromIndex:fromIndex
                           toIndex:toIndex
                     selectedIndex:webStateList->active_index()];
}

- (void)webStateList:(WebStateList*)webStateList
    didReplaceWebState:(web::WebState*)oldWebState
          withWebState:(web::WebState*)newWebState
               atIndex:(int)index {
  [self.consumer replaceItemAtIndex:index withItem:CreateItem(newWebState)];
  _scopedWebStateObserver->Remove(oldWebState);
  _scopedWebStateObserver->Add(newWebState);
}

- (void)webStateList:(WebStateList*)webStateList
    didDetachWebState:(web::WebState*)webState
              atIndex:(int)index {
  [self.consumer removeItemAtIndex:index
                     selectedIndex:webStateList->active_index()];
  _scopedWebStateObserver->Remove(webState);
}

- (void)webStateList:(WebStateList*)webStateList
    didChangeActiveWebState:(web::WebState*)newWebState
                oldWebState:(web::WebState*)oldWebState
                    atIndex:(int)atIndex
                     reason:(int)reason {
  [self.consumer selectItemAtIndex:atIndex];
}

#pragma mark - CRWWebStateObserver

- (void)webState:(web::WebState*)webState didLoadPageWithSuccess:(BOOL)success {
  int index = self.webStateList->GetIndexOfWebState(webState);
  [self.consumer replaceItemAtIndex:index withItem:CreateItem(webState)];
}

#pragma mark - GridCommands

- (void)addNewItem {
  [self insertNewItemAtIndex:self.webStateList->count()];
}

- (void)insertNewItemAtIndex:(NSUInteger)index {
  web::WebState::CreateParams params(self.tabModel.browserState);
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);
  self.webStateList->InsertWebState(
      base::checked_cast<int>(index), std::move(webState),
      (WebStateList::INSERT_FORCE_INDEX | WebStateList::INSERT_ACTIVATE),
      WebStateOpener());
  web::WebState::OpenURLParams openParams(
      GURL(kChromeUINewTabURL), web::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false);
  self.webStateList->GetWebStateAt(index)->OpenURL(openParams);
}

- (void)selectItemAtIndex:(NSUInteger)index {
  self.webStateList->ActivateWebStateAt(index);
}

- (void)closeItemAtIndex:(NSUInteger)index {
  self.webStateList->CloseWebStateAt(index, WebStateList::CLOSE_USER_ACTION);
}

- (void)closeAllItems {
  self.webStateList->CloseAllWebStates(WebStateList::CLOSE_USER_ACTION);
}

#pragma mark - GridImageDataSource

- (void)snapshotForIdentifier:(NSString*)identifier
                   completion:(void (^)(UIImage*))completion {
  web::WebState* webState = [self webStateForIdentifier:identifier];
  if (webState) {
    SnapshotTabHelper::FromWebState(webState)->RetrieveColorSnapshot(
        ^(UIImage* image) {
          if (image)
            completion(image);
        });
  }
}

- (void)faviconForIdentifier:(NSString*)identifier
                  completion:(void (^)(UIImage*))completion {
  web::WebState* webState = [self webStateForIdentifier:identifier];
  if (webState) {
    favicon::FaviconDriver* faviconDriver =
        favicon::WebFaviconDriver::FromWebState(webState);
    if (faviconDriver) {
      gfx::Image favicon = faviconDriver->GetFavicon();
      if (!favicon.IsEmpty())
        completion(favicon.ToUIImage());
    }
  }
}

#pragma mark - Private

- (web::WebState*)webStateForIdentifier:(NSString*)identifier {
  for (int i = 0; i < self.webStateList->count(); i++) {
    web::WebState* webState = self.webStateList->GetWebStateAt(i);
    TabIdTabHelper* tabHelper = TabIdTabHelper::FromWebState(webState);
    if ([identifier isEqualToString:tabHelper->tab_id()]) {
      return webState;
    }
  }
  return nullptr;
}

- (void)populateConsumerItems {
  if (self.webStateList->count() > 0) {
    [self.consumer populateItems:CreateItems(self.webStateList)
                   selectedIndex:self.webStateList->active_index()];
  }
}

@end
