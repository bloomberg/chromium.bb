// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autocheckout_whitelist_manager_factory.h"

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "components/autofill/browser/autocheckout/whitelist_manager.h"

namespace autofill {
namespace autocheckout {

class WhitelistManagerServiceImpl
    : public WhitelistManagerService {
 public:
  explicit WhitelistManagerServiceImpl(Profile* profile);
  virtual ~WhitelistManagerServiceImpl();

  // WhitelistManagerService:
  virtual void Shutdown() OVERRIDE;
  virtual WhitelistManager* GetWhitelistManager() OVERRIDE;

 private:
  scoped_ptr<WhitelistManager> whitelist_manager_;
};

WhitelistManagerServiceImpl
    ::WhitelistManagerServiceImpl(Profile* profile) {
  whitelist_manager_.reset(new WhitelistManager());
  whitelist_manager_->Init(profile->GetRequestContext());
}

WhitelistManagerServiceImpl
    ::~WhitelistManagerServiceImpl() {}

void WhitelistManagerServiceImpl::Shutdown() {
  whitelist_manager_.reset();
}

WhitelistManager*
WhitelistManagerServiceImpl::GetWhitelistManager() {
  return whitelist_manager_.get();
}

// static
WhitelistManager*
WhitelistManagerFactory::GetForProfile(Profile* profile) {
  WhitelistManagerService* service =
      static_cast<WhitelistManagerService*>(
          GetInstance()->GetServiceForProfile(profile, true));
  // service can be NULL for tests.
  return service ? service->GetWhitelistManager() : NULL;
}

// static
WhitelistManagerFactory*
WhitelistManagerFactory::GetInstance() {
  return Singleton<WhitelistManagerFactory>::get();
}

WhitelistManagerFactory::WhitelistManagerFactory()
    : ProfileKeyedServiceFactory("AutocheckoutWhitelistManager",
                                 ProfileDependencyManager::GetInstance()) {
}

WhitelistManagerFactory::~WhitelistManagerFactory() {
}

ProfileKeyedService*
WhitelistManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  WhitelistManagerService* service =
      new WhitelistManagerServiceImpl(static_cast<Profile*>(profile));
  return service;
}

content::BrowserContext* WhitelistManagerFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

bool WhitelistManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace autocheckout
}  // namespace autofill
