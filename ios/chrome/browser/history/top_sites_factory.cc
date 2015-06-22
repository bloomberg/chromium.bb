// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/history/top_sites_factory.h"

#include "base/memory/singleton.h"
#include "components/dom_distiller/core/url_constants.h"
#include "components/history/core/browser/history_constants.h"
#include "components/history/core/browser/top_sites_impl.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "ios/public/provider/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "ios/public/provider/chrome/browser/keyed_service_provider.h"
#include "ios/web/public/web_thread.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace ios {

namespace {
// Returns true if this looks like the type of URL that should be added to the
// history. This filters out URLs such a JavaScript.
bool CanAddURLToHistory(const GURL& url) {
  if (!url.is_valid())
    return false;

  // TODO: We should allow ChromeUIScheme URLs if they have been explicitly
  // typed.  Right now, however, these are marked as typed even when triggered
  // by a shortcut or menu action.
  if (url.SchemeIs(url::kJavaScriptScheme) ||
      url.SchemeIs(dom_distiller::kDomDistillerScheme) ||
      url.SchemeIs(ios::GetChromeBrowserProvider()->GetChromeUIScheme()))
    return false;

  // Allow all about: and chrome: URLs except about:blank, since the user may
  // like to see "chrome://memory/", etc. in their history and autocomplete.
  if (url == GURL(url::kAboutBlankURL))
    return false;

  return true;
}
}  // namespace

// static
scoped_refptr<history::TopSites> TopSitesFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return make_scoped_refptr(static_cast<history::TopSites*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true).get()));
}

// static
TopSitesFactory* TopSitesFactory::GetInstance() {
  return Singleton<TopSitesFactory>::get();
}

TopSitesFactory::TopSitesFactory()
    : RefcountedBrowserStateKeyedServiceFactory(
          "TopSites",
          BrowserStateDependencyManager::GetInstance()) {
  DependsOn(ios::GetKeyedServiceProvider()->GetHistoryServiceFactory());
}

TopSitesFactory::~TopSitesFactory() {
}

scoped_refptr<RefcountedKeyedService> TopSitesFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  scoped_refptr<history::TopSitesImpl> top_sites(new history::TopSitesImpl(
      browser_state->GetPrefs(),
      ios::GetKeyedServiceProvider()->GetHistoryServiceForBrowserState(
          browser_state, ServiceAccessType::EXPLICIT_ACCESS),
      history::PrepopulatedPageList(), base::Bind(CanAddURLToHistory)));
  top_sites->Init(
      browser_state->GetStatePath().Append(history::kTopSitesFilename),
      web::WebThread::GetTaskRunnerForThread(web::WebThread::DB));
  return top_sites;
}

void TopSitesFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  history::TopSitesImpl::RegisterPrefs(registry);
}

bool TopSitesFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace ios
