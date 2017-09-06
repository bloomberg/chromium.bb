// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/instant_controller.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/ui/browser_instant_controller.h"
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
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (navigation_handle->HasCommitted() &&
        navigation_handle->IsInMainFrame()) {
      callback_.Run();
    }
  }

  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(TabObserver);
};

InstantController::InstantController(BrowserInstantController* browser)
    : browser_(browser), search_origin_(SearchModel::Origin::DEFAULT) {}

InstantController::~InstantController() = default;

void InstantController::SearchModeChanged(SearchModel::Origin old_origin,
                                          SearchModel::Origin new_origin) {
  LogDebugEvent(base::StringPrintf("SearchModeChanged: %d to %d", old_origin,
                                   new_origin));

  search_origin_ = new_origin;
  ResetInstantTab();
}

void InstantController::ActiveTabChanged() {
  LogDebugEvent("ActiveTabChanged");
  ResetInstantTab();
}

void InstantController::LogDebugEvent(const std::string& info) const {
  DVLOG(1) << info;

  debug_events_.push_front(std::make_pair(
      base::Time::Now().ToInternalValue(), info));
  static const size_t kMaxDebugEventSize = 2000;
  if (debug_events_.size() > kMaxDebugEventSize)
    debug_events_.pop_back();
}

void InstantController::ClearDebugEvents() {
  debug_events_.clear();
}

void InstantController::InstantTabAboutToNavigateMainFrame() {
  DCHECK(instant_tab_observer_);
  // The Instant tab navigated (which means it had instant support both before
  // and after the navigation). This may cause it to be assigned to a new
  // renderer process, which doesn't have the most visited/theme data yet, so
  // send it now.
  // TODO(treib): This seems unnecessarily convoluted and fragile. Can't we just
  // send this when the Instant process is created?
  UpdateInfoForInstantTab();
}

void InstantController::ResetInstantTab() {
  if (search_origin_ == SearchModel::Origin::NTP) {
    content::WebContents* active_tab = browser_->GetActiveWebContents();
    if (!instant_tab_observer_ ||
        active_tab != instant_tab_observer_->web_contents()) {
      instant_tab_observer_ = base::MakeUnique<TabObserver>(
          active_tab,
          base::Bind(&InstantController::InstantTabAboutToNavigateMainFrame,
                     base::Unretained(this)));
      UpdateInfoForInstantTab();
    }
  } else {
    instant_tab_observer_.reset();
  }
}

void InstantController::UpdateInfoForInstantTab() {
  DCHECK(instant_tab_observer_);
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(browser_->profile());
  if (instant_service) {
    instant_service->UpdateThemeInfo();
    instant_service->UpdateMostVisitedItemsInfo();
  }
}
