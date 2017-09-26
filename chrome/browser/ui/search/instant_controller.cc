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
#include "chrome/browser/ui/search/search_tab_helper.h"
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

InstantController::InstantController(
    BrowserInstantController* browser_instant_controller)
    : browser_instant_controller_(browser_instant_controller) {
  browser_instant_controller_->search_model()->AddObserver(this);
}

InstantController::~InstantController() {
  browser_instant_controller_->search_model()->RemoveObserver(this);
}

void InstantController::ModelChanged(SearchModel::Origin old_origin,
                                     SearchModel::Origin new_origin) {
  // The search mode in the active tab has changed. Bind |instant_tab_observer_|
  // if the |new_origin| reflects an Instant NTP.
  // Note: This can be called either because the SearchMode changed within the
  // current tab, or because the active tab changed. In the latter case, this
  // gets called before ActiveTabChanged().
  LogDebugEvent(
      base::StringPrintf("ModelChanged: %d to %d", old_origin, new_origin));

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
  content::WebContents* active_tab =
      browser_instant_controller_->GetActiveWebContents();
  if (active_tab &&
      SearchTabHelper::FromWebContents(active_tab)->model()->origin() ==
          SearchModel::Origin::NTP) {
    // The active tab is an NTP. If we're not already tracking it, do so and
    // also update the required info.
    if (!instant_tab_observer_ ||
        active_tab != instant_tab_observer_->web_contents()) {
      instant_tab_observer_ = base::MakeUnique<TabObserver>(
          active_tab,
          base::Bind(&InstantController::InstantTabAboutToNavigateMainFrame,
                     base::Unretained(this)));
      UpdateInfoForInstantTab();
    }
  } else {
    instant_tab_observer_ = nullptr;
  }
}

void InstantController::UpdateInfoForInstantTab() {
  DCHECK(instant_tab_observer_);
  InstantService* instant_service = InstantServiceFactory::GetForProfile(
      browser_instant_controller_->profile());
  if (instant_service) {
    instant_service->UpdateThemeInfo();
    instant_service->UpdateMostVisitedItemsInfo();
  }
}
