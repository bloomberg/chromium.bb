// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_url_redirector.h"

#include "apps/launcher.h"
#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "components/navigation_interception/intercept_navigation_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::WebContents;
using extensions::Extension;
using extensions::UrlHandlers;
using extensions::UrlHandlerInfo;

namespace {

bool LaunchAppWithUrl(
    const scoped_refptr<const Extension> app,
    const std::string& handler_id,
    content::WebContents* source,
    const navigation_interception::NavigationParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Redirect top-level navigations only. This excludes iframes and webviews
  // in particular.
  if (source->IsSubframe()) {
    DVLOG(1) << "Cancel redirection: source is a subframe";
    return false;
  }

  // If prerendering, don't launch the app but abort the navigation.
  prerender::PrerenderContents* prerender_contents =
      prerender::PrerenderContents::FromWebContents(source);
  if (prerender_contents) {
    prerender_contents->Destroy(prerender::FINAL_STATUS_NAVIGATION_INTERCEPTED);
    return true;
  }

  // These are guaranteed by CreateThrottleFor below.
  DCHECK(UrlHandlers::CanExtensionHandleUrl(app.get(), params.url()));
  DCHECK(!params.is_post());

  Profile* profile =
      Profile::FromBrowserContext(source->GetBrowserContext());

  DVLOG(1) << "Launching app handler with URL: "
           << params.url().spec() << " -> "
           << app->name() << "(" << app->id() << "):" << handler_id;
  apps::LaunchPlatformAppWithUrl(
      profile, app.get(), handler_id, params.url(), params.referrer().url);

  return true;
}

}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
AppUrlRedirector::MaybeCreateThrottleFor(content::NavigationHandle* handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DVLOG(1) << "Considering URL for redirection: " << handle->GetURL().spec();

  content::BrowserContext* browser_context =
      handle->GetWebContents()->GetBrowserContext();
  DCHECK(browser_context);

  // Support only GET for now.
  if (handle->IsPost()) {
    DVLOG(1) << "Skip redirection: method is not GET";
    return nullptr;
  }

  if (!handle->GetURL().SchemeIsHTTPOrHTTPS()) {
    DVLOG(1) << "Skip redirection: scheme is not HTTP or HTTPS";
    return nullptr;
  }

  // Never redirect URLs to apps in incognito. Technically, apps are not
  // supported in incognito, but that may change in future.
  // See crbug.com/240879, which tracks incognito support for v2 apps.
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (profile->GetProfileType() == Profile::INCOGNITO_PROFILE) {
    DVLOG(1) << "Skip redirection: unsupported in incognito";
    return nullptr;
  }

  const extensions::ExtensionSet& enabled_extensions =
      extensions::ExtensionRegistry::Get(browser_context)->enabled_extensions();
  for (extensions::ExtensionSet::const_iterator iter =
           enabled_extensions.begin();
       iter != enabled_extensions.end(); ++iter) {
    const UrlHandlerInfo* handler =
        UrlHandlers::FindMatchingUrlHandler(iter->get(), handle->GetURL());
    if (handler) {
      DVLOG(1) << "Found matching app handler for redirection: "
               << (*iter)->name() << "(" << (*iter)->id() << "):"
               << handler->id;
      return std::unique_ptr<content::NavigationThrottle>(
          new navigation_interception::InterceptNavigationThrottle(
              handle,
              base::Bind(&LaunchAppWithUrl,
                         scoped_refptr<const Extension>(*iter), handler->id),
              true));
    }
  }

  DVLOG(1) << "Skipping redirection: no matching app handler found";
  return nullptr;
}
