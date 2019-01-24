// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_PROFILE_SYNC_SERVICE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_PROFILE_SYNC_SERVICE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace syncer {
class SyncService;
}  // namespace syncer

namespace ios_web_view {
class WebViewBrowserState;

// Singleton that owns all ProfileSyncService and associates them with
// WebViewBrowserState.
class WebViewProfileSyncServiceFactory
    : public BrowserStateKeyedServiceFactory {
 public:
  static syncer::SyncService* GetForBrowserState(
      WebViewBrowserState* browser_state);

  static WebViewProfileSyncServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<WebViewProfileSyncServiceFactory>;

  WebViewProfileSyncServiceFactory();
  ~WebViewProfileSyncServiceFactory() override;

  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

  DISALLOW_COPY_AND_ASSIGN(WebViewProfileSyncServiceFactory);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_SYNC_WEB_VIEW_PROFILE_SYNC_SERVICE_FACTORY_H_
