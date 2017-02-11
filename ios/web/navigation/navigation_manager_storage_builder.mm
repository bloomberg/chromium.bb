// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_manager_storage_builder.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/crw_session_entry.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_item_storage_builder.h"
#include "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/crw_navigation_manager_storage.h"

// CRWSessionController's readonly properties redefined as readwrite.  These
// will be removed and NavigationManagerImpl's ivars will be written directly
// as this functionality moves from CRWSessionController to
// NavigationManagerImpl;
@interface CRWSessionController (ExposedForSerialization)
@property(nonatomic, readwrite, retain) NSString* tabId;
@property(nonatomic, readwrite, copy) NSString* openerId;
@property(nonatomic, readwrite, getter=isOpenedByDOM) BOOL openedByDOM;
@property(nonatomic, readwrite, assign) NSInteger openerNavigationIndex;
@property(nonatomic, readwrite, assign) NSInteger previousNavigationIndex;
@property(nonatomic, readwrite, retain)
    CRWSessionCertificatePolicyManager* sessionCertificatePolicyManager;
@end

namespace web {

CRWNavigationManagerStorage* NavigationManagerStorageBuilder::BuildStorage(
    NavigationManagerImpl* navigation_manager) const {
  DCHECK(navigation_manager);
  CRWNavigationManagerStorage* serialized_navigation_manager =
      [[CRWNavigationManagerStorage alloc] init];
  CRWSessionController* session_controller =
      navigation_manager->GetSessionController();
  serialized_navigation_manager.tabID = session_controller.tabId;
  serialized_navigation_manager.openerID = session_controller.openerId;
  serialized_navigation_manager.openedByDOM = session_controller.openedByDOM;
  serialized_navigation_manager.openerNavigationIndex =
      session_controller.openerNavigationIndex;
  serialized_navigation_manager.windowName = session_controller.windowName;
  serialized_navigation_manager.currentNavigationIndex =
      session_controller.currentNavigationIndex;
  serialized_navigation_manager.previousNavigationIndex =
      session_controller.previousNavigationIndex;
  serialized_navigation_manager.lastVisitedTimestamp =
      session_controller.lastVisitedTimestamp;
  serialized_navigation_manager.sessionCertificatePolicyManager =
      session_controller.sessionCertificatePolicyManager;
  NSMutableArray* item_storages = [[NSMutableArray alloc] init];
  NavigationItemStorageBuilder item_storage_builder;
  for (size_t index = 0; index < session_controller.items.size(); ++index) {
    web::NavigationItemImpl* item = session_controller.items[index].get();
    [item_storages addObject:item_storage_builder.BuildStorage(item)];
  }
  serialized_navigation_manager.itemStorages = item_storages;
  return serialized_navigation_manager;
}

std::unique_ptr<NavigationManagerImpl>
NavigationManagerStorageBuilder::BuildNavigationManagerImpl(
    CRWNavigationManagerStorage* navigation_manager_serialization) const {
  DCHECK(navigation_manager_serialization);
  NSArray* item_storages = navigation_manager_serialization.itemStorages;
  web::ScopedNavigationItemList items(item_storages.count);
  NavigationItemStorageBuilder item_storage_builder;
  for (size_t index = 0; index < item_storages.count; ++index) {
    std::unique_ptr<NavigationItemImpl> item_impl =
        item_storage_builder.BuildNavigationItemImpl(item_storages[index]);
    items[index] = std::move(item_impl);
  }
  NSUInteger current_index =
      navigation_manager_serialization.currentNavigationIndex;
  base::scoped_nsobject<CRWSessionController> session_controller(
      [[CRWSessionController alloc] initWithNavigationItems:std::move(items)
                                               currentIndex:current_index
                                               browserState:nullptr]);
  [session_controller setTabId:navigation_manager_serialization.tabID];
  [session_controller setOpenerId:navigation_manager_serialization.openerID];
  [session_controller
      setOpenedByDOM:navigation_manager_serialization.openedByDOM];
  [session_controller setOpenerNavigationIndex:navigation_manager_serialization
                                                   .openerNavigationIndex];
  [session_controller
      setWindowName:navigation_manager_serialization.windowName];
  [session_controller
      setPreviousNavigationIndex:navigation_manager_serialization
                                     .previousNavigationIndex];
  [session_controller setLastVisitedTimestamp:navigation_manager_serialization
                                                  .lastVisitedTimestamp];
  [session_controller
      setSessionCertificatePolicyManager:navigation_manager_serialization
                                             .sessionCertificatePolicyManager];
  std::unique_ptr<NavigationManagerImpl> navigation_manager(
      new NavigationManagerImpl());
  navigation_manager->SetSessionController(session_controller);
  return navigation_manager;
}

}  // namespace web
