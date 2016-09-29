// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ntp_tiles/ios_most_visited_sites_factory.h"

#include "base/memory/ptr_util.h"
#include "components/history/core/browser/top_sites.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/top_sites_factory.h"
#include "ios/chrome/browser/ntp_tiles/ios_popular_sites_factory.h"
#include "ios/chrome/browser/suggestions/suggestions_service_factory.h"

std::unique_ptr<ntp_tiles::MostVisitedSites>
IOSMostVisitedSitesFactory::NewForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return base::MakeUnique<ntp_tiles::MostVisitedSites>(
      browser_state->GetPrefs(),
      ios::TopSitesFactory::GetForBrowserState(browser_state),
      suggestions::SuggestionsServiceFactory::GetForBrowserState(browser_state),
      IOSPopularSitesFactory::NewForBrowserState(browser_state), nil);
}
