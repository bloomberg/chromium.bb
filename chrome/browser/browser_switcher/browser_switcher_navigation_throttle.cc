// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/browser_switcher_navigation_throttle.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_switcher/alternative_browser_driver.h"
#include "chrome/browser/browser_switcher/browser_switcher_service.h"
#include "chrome/browser/browser_switcher/browser_switcher_service_factory.h"
#include "chrome/browser/browser_switcher/browser_switcher_sitelist.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/guest_mode.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace browser_switcher {

namespace {

// Returns true if there's only 1 tab left open in this profile. Incognito
// window tabs count as the same profile.
bool IsLastTab(const Profile* profile) {
  profile = profile->GetOriginalProfile();
  int tab_count = 0;
  for (const Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile()->GetOriginalProfile() != profile)
      continue;
    tab_count += browser->tab_strip_model()->count();
    if (tab_count > 1)
      return false;
  }
  return true;
}

bool MaybeLaunchAlternativeBrowser(
    content::WebContents* web_contents,
    const navigation_interception::NavigationParams& params) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  BrowserSwitcherService* service =
      BrowserSwitcherServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  const GURL& url = params.url();
  bool should_switch = service->sitelist()->ShouldSwitch(url);

  if (!should_switch)
    return false;

  // Redirect top-level navigations only. This excludes iframes and webviews
  // in particular.
  if (content::GuestMode::IsCrossProcessFrameGuest(web_contents))
    return false;

  // If prerendering, don't launch the alternative browser but abort the
  // navigation.
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(web_contents);
  if (prerender_contents) {
    prerender_contents->Destroy(prerender::FINAL_STATUS_BROWSER_SWITCH);
    return true;
  }

  // TODO(nicolaso): Once the chrome://browserswitch page is implemented, open
  // that instead of immediately launching the browser/closing the tab.
  bool success;
  {
    SCOPED_UMA_HISTOGRAM_TIMER("BrowserSwitcher.LaunchTime");
    success = service->driver()->TryLaunch(url);
    UMA_HISTOGRAM_BOOLEAN("BrowserSwitcher.LaunchSuccess", success);
  }
  if (success) {
    const Profile* profile =
        Profile::FromBrowserContext(web_contents->GetBrowserContext());
    if (service->prefs().KeepLastTab() && IsLastTab(profile)) {
      // TODO(nicolaso): Show the NTP after cancelling the navigation.
    } else {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&content::WebContents::ClosePage,
                                    base::Unretained(web_contents)));
    }
    return true;
  }

  // Launching the browser failed, fall back to loading the page normally.
  //
  // TODO(nicolaso): Show an error page instead.
  return false;
}

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
BrowserSwitcherNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::BrowserContext* browser_context =
      navigation->GetWebContents()->GetBrowserContext();

  if (browser_context->IsOffTheRecord())
    return nullptr;

  if (!navigation->IsInMainFrame())
    return nullptr;

  return std::make_unique<navigation_interception::InterceptNavigationThrottle>(
      navigation, base::BindRepeating(&MaybeLaunchAlternativeBrowser));
}

}  // namespace browser_switcher
