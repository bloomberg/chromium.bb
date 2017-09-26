// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_controller.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser_instant_controller.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

class InstantController::TabObserver : public content::WebContentsObserver {
 public:
  TabObserver(content::WebContents* web_contents, const base::Closure& callback)
      : content::WebContentsObserver(web_contents), callback_(callback) {}
  ~TabObserver() override = default;

 private:
  // Overridden from content::WebContentsObserver:
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override {
    if (load_details.is_main_frame && search::IsInstantNTP(web_contents())) {
      callback_.Run();
    }
  }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    // TODO(treib): Verify if this is necessary - NavigationEntryCommitted
    // should already cover all cases.
    if (navigation_handle->HasCommitted() &&
        navigation_handle->IsInMainFrame() &&
        search::IsInstantNTP(web_contents())) {
      callback_.Run();
    }
  }

  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(TabObserver);
};

InstantController::InstantController(
    BrowserInstantController* browser_instant_controller)
    : browser_instant_controller_(browser_instant_controller) {}

InstantController::~InstantController() = default;

void InstantController::OnTabActivated(content::WebContents* web_contents) {
  if (!tab_observer_ || tab_observer_->web_contents() != web_contents) {
    tab_observer_ = std::make_unique<TabObserver>(
        web_contents, base::Bind(&InstantController::UpdateInfoForInstantTab,
                                 base::Unretained(this)));
    // If this tab is an NTP, immediately send it the required info.
    if (search::IsInstantNTP(web_contents)) {
      UpdateInfoForInstantTab();
    }
  }
}

void InstantController::OnTabDeactivated(content::WebContents* web_contents) {
  if (tab_observer_ && tab_observer_->web_contents() == web_contents) {
    tab_observer_ = nullptr;
  }
}

void InstantController::OnTabDetached(content::WebContents* web_contents) {
  OnTabDeactivated(web_contents);
}

void InstantController::UpdateInfoForInstantTab() {
  InstantService* instant_service = InstantServiceFactory::GetForProfile(
      browser_instant_controller_->profile());
  if (instant_service) {
    instant_service->UpdateThemeInfo();
    instant_service->UpdateMostVisitedItemsInfo();
  }
}
