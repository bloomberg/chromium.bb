// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/session_storage_builder.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_item_storage_builder.h"
#include "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/crw_session_storage.h"
#import "ios/web/public/serializable_user_data_manager.h"
#import "ios/web/web_state/web_state_impl.h"

// CRWSessionController's readonly properties redefined as readwrite.  These
// will be removed and NavigationManagerImpl's ivars will be written directly
// as this functionality moves from CRWSessionController to
// NavigationManagerImpl;
@interface CRWSessionController (ExposedForSerialization)
@property(nonatomic, readwrite, assign) NSInteger previousItemIndex;
@property(nonatomic, readwrite, retain)
    CRWSessionCertificatePolicyManager* sessionCertificatePolicyManager;
@end

namespace web {

CRWSessionStorage* SessionStorageBuilder::BuildStorage(
    WebStateImpl* web_state) const {
  DCHECK(web_state);
  web::NavigationManagerImpl* navigation_manager =
      web_state->navigation_manager_.get();
  DCHECK(navigation_manager);
  CRWSessionStorage* session_storage = [[CRWSessionStorage alloc] init];
  session_storage.hasOpener = web_state->HasOpener();
  CRWSessionController* session_controller =
      navigation_manager->GetSessionController();
  session_storage.lastCommittedItemIndex =
      session_controller.lastCommittedItemIndex;
  session_storage.previousItemIndex = session_controller.previousItemIndex;
  session_storage.sessionCertificatePolicyManager =
      session_controller.sessionCertificatePolicyManager;
  NSMutableArray* item_storages = [[NSMutableArray alloc] init];
  NavigationItemStorageBuilder item_storage_builder;
  for (size_t index = 0; index < session_controller.items.size(); ++index) {
    web::NavigationItemImpl* item = session_controller.items[index].get();
    [item_storages addObject:item_storage_builder.BuildStorage(item)];
  }
  session_storage.itemStorages = item_storages;
  web::SerializableUserDataManager* user_data_manager =
      web::SerializableUserDataManager::FromWebState(web_state);
  [session_storage
      setSerializableUserData:user_data_manager->CreateSerializableUserData()];
  return session_storage;
}

void SessionStorageBuilder::ExtractSessionState(
    WebStateImpl* web_state,
    CRWSessionStorage* storage) const {
  DCHECK(web_state);
  DCHECK(storage);
  web_state->created_with_opener_ = storage.hasOpener;
  NSArray* item_storages = storage.itemStorages;
  web::ScopedNavigationItemList items(item_storages.count);
  NavigationItemStorageBuilder item_storage_builder;
  for (size_t index = 0; index < item_storages.count; ++index) {
    std::unique_ptr<NavigationItemImpl> item_impl =
        item_storage_builder.BuildNavigationItemImpl(item_storages[index]);
    items[index] = std::move(item_impl);
  }
  NSUInteger last_committed_item_index = storage.lastCommittedItemIndex;
  base::scoped_nsobject<CRWSessionController> session_controller(
      [[CRWSessionController alloc]
            initWithBrowserState:nullptr
                 navigationItems:std::move(items)
          lastCommittedItemIndex:last_committed_item_index]);
  [session_controller setPreviousItemIndex:storage.previousItemIndex];
  [session_controller
      setSessionCertificatePolicyManager:storage
                                             .sessionCertificatePolicyManager];
  web_state->navigation_manager_.reset(new NavigationManagerImpl());
  web_state->navigation_manager_->SetSessionController(session_controller);
  web::SerializableUserDataManager::FromWebState(web_state)
      ->AddSerializableUserData(storage.userData);
}

}  // namespace web
