// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bookmark_app_navigation_throttle.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace extensions {

// static
std::unique_ptr<content::NavigationThrottle>
BookmarkAppNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DVLOG(1) << "Considering URL for interception: "
           << navigation_handle->GetURL().spec();
  if (!navigation_handle->IsInMainFrame()) {
    DVLOG(1) << "Don't intercept: Navigation is not in main frame.";
    return nullptr;
  }

  if (navigation_handle->IsPost()) {
    DVLOG(1) << "Don't intercept: Method is POST.";
    return nullptr;
  }

  content::BrowserContext* browser_context =
      navigation_handle->GetWebContents()->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile->GetProfileType() == Profile::INCOGNITO_PROFILE) {
    DVLOG(1) << "Don't intercept: Navigation is in incognito.";
    return nullptr;
  }

  DVLOG(1) << "Attaching Bookmark App Navigation Throttle.";
  return std::make_unique<extensions::BookmarkAppNavigationThrottle>(
      navigation_handle);
}

BookmarkAppNavigationThrottle::BookmarkAppNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle), weak_ptr_factory_(this) {}

BookmarkAppNavigationThrottle::~BookmarkAppNavigationThrottle() {}

const char* BookmarkAppNavigationThrottle::GetNameForLogging() {
  return "BookmarkAppNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
BookmarkAppNavigationThrottle::WillStartRequest() {
  return ProcessNavigation();
}

content::NavigationThrottle::ThrottleCheckResult
BookmarkAppNavigationThrottle::WillRedirectRequest() {
  return ProcessNavigation();
}

content::NavigationThrottle::ThrottleCheckResult
BookmarkAppNavigationThrottle::ProcessNavigation() {
  ui::PageTransition transition_type = navigation_handle()->GetPageTransition();
  if (!(PageTransitionCoreTypeIs(transition_type, ui::PAGE_TRANSITION_LINK))) {
    DVLOG(1) << "Don't intercept: Transition type is "
             << PageTransitionGetCoreTransitionString(transition_type);
    return content::NavigationThrottle::PROCEED;
  }

  auto app_for_window_ref = GetAppForWindow();
  auto target_app_ref = GetTargetApp();

  if (app_for_window_ref == target_app_ref) {
    DVLOG(1) << "Don't intercept: The target URL is in the same scope as the "
             << "current app.";
    return content::NavigationThrottle::PROCEED;
  }

  // This results in a navigation from google.com/ to google.com/maps to just
  // navigate the tab.
  // TODO(crbug.com/783487): Get the app that corresponds to the current URL
  // and, if there is none or it's not the same as the target app, then open
  // a new app window for the target app.
  if (!app_for_window_ref &&
      HasSameOriginAsCurrentSite(navigation_handle()->GetURL())) {
    DVLOG(1) << "Don't intercept: Keep same origin navigations in the browser.";
    return content::NavigationThrottle::PROCEED;
  }

  if (target_app_ref) {
    auto* prerender_contents = prerender::PrerenderContents::FromWebContents(
        navigation_handle()->GetWebContents());
    if (prerender_contents) {
      // If prerendering, don't launch the app but abort the navigation.
      prerender_contents->Destroy(
          prerender::FINAL_STATUS_NAVIGATION_INTERCEPTED);
      return content::NavigationThrottle::CANCEL_AND_IGNORE;
    }

    return OpenInAppWindowAndCloseTabIfNecessary(target_app_ref);
  }

  if (app_for_window_ref) {
    // The experience when navigating to an out-of-scope website inside an app
    // window is not great, so we bounce these navigations back to the browser.
    // TODO(crbug.com/774895): Stop bouncing back to the browser once the
    // experience for out-of-scope navigations improves.
    DVLOG(1) << "Open in new tab.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&BookmarkAppNavigationThrottle::OpenInNewTab,
                              weak_ptr_factory_.GetWeakPtr()));
    return content::NavigationThrottle::DEFER;
  }

  DVLOG(1) << "No matching Bookmark App for URL: "
           << navigation_handle()->GetURL();
  return content::NavigationThrottle::PROCEED;
}

bool BookmarkAppNavigationThrottle::HasSameOriginAsCurrentSite(
    const GURL& target_url) {
  DCHECK(navigation_handle()->IsInMainFrame());
  return url::Origin::Create(target_url)
      .IsSameOriginWith(navigation_handle()
                            ->GetWebContents()
                            ->GetMainFrame()
                            ->GetLastCommittedOrigin());
}

content::NavigationThrottle::ThrottleCheckResult
BookmarkAppNavigationThrottle::OpenInAppWindowAndCloseTabIfNecessary(
    scoped_refptr<const Extension> target_app) {
  content::WebContents* source = navigation_handle()->GetWebContents();
  if (source->GetController().IsInitialNavigation()) {
    // When a new WebContents has no opener, the first navigation will happen
    // synchronously. This could result in us opening the app and then focusing
    // the original WebContents. To avoid this we open the app asynchronously.
    if (!source->HasOpener()) {
      DVLOG(1) << "Deferring opening app.";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&BookmarkAppNavigationThrottle::OpenBookmarkApp,
                                weak_ptr_factory_.GetWeakPtr(), target_app));
    } else {
      OpenBookmarkApp(target_app);
    }

    // According to NavigationThrottle::WillStartRequest's documentation closing
    // a WebContents should be done asynchronously to avoid UAFs. Closing the
    // WebContents will cancel the navigation.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&BookmarkAppNavigationThrottle::CloseWebContents,
                              weak_ptr_factory_.GetWeakPtr()));
    return content::NavigationThrottle::DEFER;
  }

  OpenBookmarkApp(target_app);
  return content::NavigationThrottle::CANCEL_AND_IGNORE;
}

void BookmarkAppNavigationThrottle::OpenBookmarkApp(
    scoped_refptr<const Extension> bookmark_app) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::WebContents* source = navigation_handle()->GetWebContents();
  content::BrowserContext* browser_context = source->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(browser_context);
  AppLaunchParams launch_params(
      profile, bookmark_app.get(), extensions::LAUNCH_CONTAINER_WINDOW,
      WindowOpenDisposition::CURRENT_TAB, extensions::SOURCE_URL_HANDLER);
  launch_params.override_url = navigation_handle()->GetURL();

  DVLOG(1) << "Opening app.";
  OpenApplication(launch_params);
}

void BookmarkAppNavigationThrottle::CloseWebContents() {
  DVLOG(1) << "Closing empty tab.";
  navigation_handle()->GetWebContents()->Close();
}

void BookmarkAppNavigationThrottle::OpenInNewTab() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::WebContents* source = navigation_handle()->GetWebContents();
  content::OpenURLParams url_params(navigation_handle()->GetURL(),
                                    navigation_handle()->GetReferrer(),
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                    navigation_handle()->GetPageTransition(),
                                    navigation_handle()->IsRendererInitiated());
  url_params.redirect_chain = navigation_handle()->GetRedirectChain();
  url_params.frame_tree_node_id = navigation_handle()->GetFrameTreeNodeId();
  url_params.user_gesture = navigation_handle()->HasUserGesture();
  url_params.started_from_context_menu =
      navigation_handle()->WasStartedFromContextMenu();

  source->OpenURL(url_params);
  CancelDeferredNavigation(content::NavigationThrottle::CANCEL_AND_IGNORE);
}

scoped_refptr<const Extension>
BookmarkAppNavigationThrottle::GetAppForWindow() {
  content::WebContents* source = navigation_handle()->GetWebContents();
  Browser* browser = chrome::FindBrowserWithWebContents(source);
  if (!browser || !browser->is_app())
    return nullptr;

  const Extension* app =
      ExtensionRegistry::Get(source->GetBrowserContext())
          ->GetExtensionById(
              web_app::GetExtensionIdFromApplicationName(browser->app_name()),
              extensions::ExtensionRegistry::ENABLED);
  if (!app || !app->from_bookmark())
    return nullptr;

  // Bookmark Apps for installable websites have scope.
  // TODO(crbug.com/774918): Replace once there is a more explicit indicator
  // of a Bookmark App for an installable website.
  if (UrlHandlers::GetUrlHandlers(app) == nullptr)
    return nullptr;

  return app;
}

scoped_refptr<const Extension> BookmarkAppNavigationThrottle::GetTargetApp() {
  const GURL& target_url = navigation_handle()->GetURL();

  content::BrowserContext* browser_context =
      navigation_handle()->GetWebContents()->GetBrowserContext();
  for (scoped_refptr<const extensions::Extension> app :
       ExtensionRegistry::Get(browser_context)->enabled_extensions()) {
    if (!app->from_bookmark())
      continue;

    const UrlHandlerInfo* url_handler =
        UrlHandlers::FindMatchingUrlHandler(app.get(), target_url);
    if (!url_handler)
      continue;

    return app;
  }

  return nullptr;
}

}  // namespace extensions
