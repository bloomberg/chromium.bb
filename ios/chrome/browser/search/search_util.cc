// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/search/search_util.h"

#include "components/search/search.h"
#include "components/search_engines/template_url_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/web_state/web_state.h"
#include "url/gurl.h"

namespace {

TemplateURL* GetDefaultSearchProviderTemplateURL(
    ios::ChromeBrowserState* browser_state) {
  if (!browser_state)
    return nullptr;

  TemplateURLService* template_url_service =
      ios::TemplateURLServiceFactory::GetForBrowserState(browser_state);
  return template_url_service ? template_url_service->GetDefaultSearchProvider()
                              : nullptr;
}

}  // namespace

base::string16 ExtractSearchTermsFromURL(ios::ChromeBrowserState* browser_state,
                                         const GURL& url) {
  base::string16 search_terms;
  TemplateURL* template_url =
      GetDefaultSearchProviderTemplateURL(browser_state);
  if (template_url) {
    template_url->ExtractSearchTermsFromURL(
        url, ios::UIThreadSearchTermsData(browser_state), &search_terms);
  }
  return search_terms;
}

bool IsQueryExtractionAllowedForURL(ios::ChromeBrowserState* browser_state,
                                    const GURL& url) {
  TemplateURL* template_url =
      GetDefaultSearchProviderTemplateURL(browser_state);
  return template_url && search::IsSuitableURLForInstant(url, template_url);
}

base::string16 GetSearchTerms(web::WebState* web_state) {
  if (!web_state)
    return base::string16();

  web::NavigationItem* item =
      web_state->GetNavigationManager()->GetVisibleItem();
  if (!item)
    return base::string16();

  if (!search::IsQueryExtractionEnabled())
    return base::string16();

  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(web_state->GetBrowserState());
  if (!IsQueryExtractionAllowedForURL(browser_state, item->GetVirtualURL()))
    return base::string16();

  return ExtractSearchTermsFromURL(browser_state, item->GetVirtualURL());
}
