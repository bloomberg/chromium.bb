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
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chrome/browser/ui/extensions/application_launch.h"
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
  return CheckNavigation();
}

content::NavigationThrottle::ThrottleCheckResult
BookmarkAppNavigationThrottle::WillRedirectRequest() {
  return CheckNavigation();
}

content::NavigationThrottle::ThrottleCheckResult
BookmarkAppNavigationThrottle::CheckNavigation() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::WebContents* source = navigation_handle()->GetWebContents();
  ui::PageTransition transition_type = navigation_handle()->GetPageTransition();
  if (!(PageTransitionCoreTypeIs(transition_type, ui::PAGE_TRANSITION_LINK))) {
    DVLOG(1) << "Don't intercept: Transition type is "
             << PageTransitionGetCoreTransitionString(transition_type);
    return content::NavigationThrottle::PROCEED;
  }

  if (!navigation_handle()->GetURL().SchemeIsHTTPOrHTTPS()) {
    DVLOG(1) << "Don't intercept: scheme is not HTTP or HTTPS.";
    return content::NavigationThrottle::PROCEED;
  }

  DCHECK(navigation_handle()->IsInMainFrame());
  // Don't redirect same origin navigations. This matches what is done on
  // Android.
  if (source->GetMainFrame()->GetLastCommittedOrigin().IsSameOriginWith(
          url::Origin::Create(navigation_handle()->GetURL()))) {
    DVLOG(1) << "Don't intercept: Same origin navigation.";
    return content::NavigationThrottle::PROCEED;
  }

  content::BrowserContext* browser_context = source->GetBrowserContext();
  scoped_refptr<const extensions::Extension> matching_app;
  for (scoped_refptr<const extensions::Extension> app :
       ExtensionRegistry::Get(browser_context)->enabled_extensions()) {
    if (!app->from_bookmark())
      continue;

    const UrlHandlerInfo* url_handler = UrlHandlers::FindMatchingUrlHandler(
        app.get(), navigation_handle()->GetURL());
    if (!url_handler)
      continue;

    matching_app = app;
    break;
  }

  if (!matching_app) {
    DVLOG(1) << "No matching Bookmark App for URL: "
             << navigation_handle()->GetURL();
    return NavigationThrottle::PROCEED;
  } else {
    DVLOG(1) << "Found matching Bookmark App: " << matching_app->name() << "("
             << matching_app->id() << ")";
  }

  // If prerendering, don't launch the app but abort the navigation.
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(source);
  if (prerender_contents) {
    prerender_contents->Destroy(prerender::FINAL_STATUS_NAVIGATION_INTERCEPTED);
    return content::NavigationThrottle::CANCEL_AND_IGNORE;
  }

  if (source->GetController().IsInitialNavigation()) {
    // When a new WebContents has no opener, the first navigation will happen
    // synchronously. This could result in us opening the app and then focusing
    // the original WebContents. To avoid this we open the app asynchronously.
    if (!source->HasOpener()) {
      DVLOG(1) << "Deferring opening app.";
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&BookmarkAppNavigationThrottle::OpenBookmarkApp,
                                weak_ptr_factory_.GetWeakPtr(), matching_app));
    } else {
      OpenBookmarkApp(matching_app);
    }

    // According to NavigationThrottle::WillStartRequest's documentation closing
    // a WebContents should be done asynchronously to avoid UAFs. Closing the
    // WebContents will cancel the navigation.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&BookmarkAppNavigationThrottle::CloseWebContents,
                              weak_ptr_factory_.GetWeakPtr()));
    return content::NavigationThrottle::DEFER;
  }

  OpenBookmarkApp(matching_app);
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

}  // namespace extensions
