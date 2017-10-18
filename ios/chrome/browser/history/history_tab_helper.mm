// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/history/history_tab_helper.h"

#include "components/history/core/browser/history_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/history/history_service_factory.h"
#include "ios/web/public/navigation_item.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(HistoryTabHelper);

HistoryTabHelper::HistoryTabHelper(web::WebState* web_state)
    : web_state_(web_state) {}

HistoryTabHelper::~HistoryTabHelper() {}

void HistoryTabHelper::UpdateHistoryPageTitle(const web::NavigationItem& item) {
  history::HistoryService* service = GetHistoryService();
  if (service) {
    const base::string16& title = item.GetTitleForDisplay();
    // Don't update the history if current entry has no title.
    if (title.length() &&
        title != l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE)) {
      service->SetPageTitle(item.GetVirtualURL(), title);
    }
  }
}

history::HistoryService* HistoryTabHelper::GetHistoryService() {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(web_state_->GetBrowserState());

  if (browser_state->IsOffTheRecord()) {
    return nullptr;
  }

  return ios::HistoryServiceFactory::GetForBrowserState(
      browser_state, ServiceAccessType::IMPLICIT_ACCESS);
}
