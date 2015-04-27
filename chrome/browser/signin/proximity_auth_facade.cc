// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/proximity_auth_facade.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/proximity_auth/proximity_auth_client.h"
#include "components/proximity_auth/screenlock_bridge.h"
#include "components/signin/core/browser/signin_manager_base.h"

namespace {

// A Chrome-specific implementation of the ProximityAuthClient.
class ChromeProximityAuthClient : public proximity_auth::ProximityAuthClient {
 public:
  ChromeProximityAuthClient() {}
  ~ChromeProximityAuthClient() override {}

  // proximity_auth::ProximityAuthClient implementation:
  std::string GetAuthenticatedUsername(
      content::BrowserContext* browser_context) const override;
  void Lock(content::BrowserContext* browser_context) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeProximityAuthClient);
};

std::string ChromeProximityAuthClient::GetAuthenticatedUsername(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfileIfExists(profile);
  // |profile| has to be a signed-in profile with SigninManager already
  // created. Otherwise, just crash to collect stack.
  DCHECK(signin_manager);
  return signin_manager->GetAuthenticatedUsername();
}

void ChromeProximityAuthClient::Lock(content::BrowserContext* browser_context) {
  profiles::LockProfile(Profile::FromBrowserContext(browser_context));
}

// A facade class that is the glue required to initialize and manage the
// lifecycle of various objects of the Proximity Auth component.
class ProximityAuthFacade {
 public:
  proximity_auth::ScreenlockBridge* GetScreenlockBridge() {
    return &screenlock_bridge_;
  }

 private:
  friend struct base::DefaultLazyInstanceTraits<ProximityAuthFacade>;
  friend struct base::DefaultDeleter<ProximityAuthFacade>;

  ProximityAuthFacade() : screenlock_bridge_(&proximity_auth_client_) {}
  ~ProximityAuthFacade() {}

  ChromeProximityAuthClient proximity_auth_client_;
  proximity_auth::ScreenlockBridge screenlock_bridge_;

  DISALLOW_COPY_AND_ASSIGN(ProximityAuthFacade);
};

base::LazyInstance<ProximityAuthFacade> g_proximity_auth_facade_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

proximity_auth::ScreenlockBridge* GetScreenlockBridgeInstance() {
  return g_proximity_auth_facade_instance.Pointer()->GetScreenlockBridge();
}
