// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_cache_factory.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_tab_model_list_observer.h"
#import "ios/chrome/browser/snapshots/snapshot_cache_web_state_list_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// C++ wrapper around SnapshotCache, owning the SnapshotCache and allowing it
// bind it to an ios::ChromeBrowserState as a KeyedService.
class SnapshotCacheWrapper : public KeyedService {
 public:
  explicit SnapshotCacheWrapper(ios::ChromeBrowserState* browser_state,
                                SnapshotCache* snapshot_cache);
  ~SnapshotCacheWrapper() override;

  SnapshotCache* snapshot_cache() { return snapshot_cache_; }

  // KeyedService implementation.
  void Shutdown() override;

 private:
  __strong SnapshotCache* snapshot_cache_;
  std::unique_ptr<SnapshotCacheTabModelListObserver> tab_model_list_observer_;

  DISALLOW_COPY_AND_ASSIGN(SnapshotCacheWrapper);
};

SnapshotCacheWrapper::SnapshotCacheWrapper(
    ios::ChromeBrowserState* browser_state,
    SnapshotCache* snapshot_cache)
    : snapshot_cache_(snapshot_cache) {
  DCHECK(snapshot_cache);
  tab_model_list_observer_ =
      std::make_unique<SnapshotCacheTabModelListObserver>(
          browser_state,
          std::make_unique<SnapshotCacheWebStateListObserver>(snapshot_cache));
}

SnapshotCacheWrapper::~SnapshotCacheWrapper() {
  DCHECK(!snapshot_cache_);
}

void SnapshotCacheWrapper::Shutdown() {
  tab_model_list_observer_.reset();
  [snapshot_cache_ shutdown];
  snapshot_cache_ = nil;
}

std::unique_ptr<KeyedService> BuildSnapshotCacheWrapper(
    web::BrowserState* context) {
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  return std::make_unique<SnapshotCacheWrapper>(chrome_browser_state,
                                                [[SnapshotCache alloc] init]);
}
}  // namespace

// static
SnapshotCache* SnapshotCacheFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  SnapshotCacheWrapper* wrapper = static_cast<SnapshotCacheWrapper*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
  return wrapper ? wrapper->snapshot_cache() : nil;
}

// static
SnapshotCacheFactory* SnapshotCacheFactory::GetInstance() {
  static base::NoDestructor<SnapshotCacheFactory> instance;
  return instance.get();
}

// static
BrowserStateKeyedServiceFactory::TestingFactory
SnapshotCacheFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildSnapshotCacheWrapper);
}

SnapshotCacheFactory::SnapshotCacheFactory()
    : BrowserStateKeyedServiceFactory(
          "SnapshotCache",
          BrowserStateDependencyManager::GetInstance()) {}

SnapshotCacheFactory::~SnapshotCacheFactory() = default;

std::unique_ptr<KeyedService> SnapshotCacheFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  return BuildSnapshotCacheWrapper(context);
}

web::BrowserState* SnapshotCacheFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}

bool SnapshotCacheFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
