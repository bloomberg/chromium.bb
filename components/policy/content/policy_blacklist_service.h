// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CONTENT_POLICY_BLACKLIST_SERVICE_H_
#define COMPONENTS_POLICY_CONTENT_POLICY_BLACKLIST_SERVICE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/policy/core/browser/url_blacklist_manager.h"

namespace safe_search_api {
class URLChecker;
}  // namespace safe_search_api

// PolicyBlacklistService and PolicyBlacklistFactory provide a way for
// us to access URLBlacklistManager, a policy block list service based on
// the Preference Service. The URLBlacklistManager responds to permission
// changes and is per-Profile.
class PolicyBlacklistService : public KeyedService {
 public:
  using CheckSafeSearchCallback = base::OnceCallback<void(bool is_safe)>;

  PolicyBlacklistService(
      content::BrowserContext* browser_context,
      std::unique_ptr<policy::URLBlacklistManager> url_blacklist_manager);
  ~PolicyBlacklistService() override;

  policy::URLBlacklist::URLBlacklistState GetURLBlacklistState(
      const GURL& url) const;

  // Starts a call to the Safe Search API for the given URL to determine whether
  // the URL is "safe" (not porn). Returns whether |callback| was run
  // synchronously.
  bool CheckSafeSearchURL(const GURL& url, CheckSafeSearchCallback callback);

  // Creates a SafeSearch URLChecker using a given URLLoaderFactory for testing.
  void SetSafeSearchURLCheckerForTest(
      std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker);

 private:
  content::BrowserContext* const browser_context_;
  std::unique_ptr<policy::URLBlacklistManager> url_blacklist_manager_;
  std::unique_ptr<safe_search_api::URLChecker> safe_search_url_checker_;

  DISALLOW_COPY_AND_ASSIGN(PolicyBlacklistService);
};

class PolicyBlacklistFactory : public BrowserContextKeyedServiceFactory {
 public:
  static PolicyBlacklistFactory* GetInstance();
  static PolicyBlacklistService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  PolicyBlacklistFactory();
  ~PolicyBlacklistFactory() override;
  friend struct base::DefaultSingletonTraits<PolicyBlacklistFactory>;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  // Finds which browser context (if any) to use.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(PolicyBlacklistFactory);
};

#endif  // COMPONENTS_POLICY_CONTENT_POLICY_BLACKLIST_SERVICE_H_
