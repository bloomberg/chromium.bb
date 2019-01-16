// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_STORE_FACTORY_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_STORE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "components/keyed_service/ios/refcounted_browser_state_keyed_service_factory.h"

enum class ServiceAccessType;

namespace password_manager {
class PasswordStore;
}

namespace ios_web_view {

class WebViewBrowserState;

// Singleton that owns all PasswordStores and associates them with
// WebViewBrowserState.
class WebViewPasswordStoreFactory
    : public RefcountedBrowserStateKeyedServiceFactory {
 public:
  static scoped_refptr<password_manager::PasswordStore> GetForBrowserState(
      WebViewBrowserState* browser_state,
      ServiceAccessType access_type);

  static WebViewPasswordStoreFactory* GetInstance();

  // Called by the PasswordDataTypeController whenever there is a possibility
  // that syncing passwords has just started or ended for |browser_state|.
  static void OnPasswordsSyncedStatePotentiallyChanged(
      WebViewBrowserState* browser_state);

 private:
  friend class base::NoDestructor<WebViewPasswordStoreFactory>;

  WebViewPasswordStoreFactory();
  ~WebViewPasswordStoreFactory() override;

  // BrowserStateKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;
  web::BrowserState* GetBrowserStateToUse(
      web::BrowserState* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(WebViewPasswordStoreFactory);
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_STORE_FACTORY_H_
