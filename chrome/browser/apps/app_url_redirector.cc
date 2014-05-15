// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_url_redirector.h"

#include "apps/launcher.h"
#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "components/navigation_interception/intercept_navigation_resource_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/extension_set.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::ResourceRequestInfo;
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

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
  DCHECK(!params.is_post());
  DCHECK(UrlHandlers::CanExtensionHandleUrl(app, params.url()));

  Profile* profile =
      Profile::FromBrowserContext(source->GetBrowserContext());

  DVLOG(1) << "Launching app handler with URL: "
           << params.url().spec() << " -> "
           << app->name() << "(" << app->id() << "):" << handler_id;
  apps::LaunchPlatformAppWithUrl(
      profile, app, handler_id, params.url(), params.referrer().url);

  return true;
}

}  // namespace

// static
content::ResourceThrottle*
AppUrlRedirector::MaybeCreateThrottleFor(net::URLRequest* request,
                                         ProfileIOData* profile_io_data) {
  DVLOG(1) << "Considering URL for redirection: "
           << request->method() << " " << request->url().spec();

  // Support only GET for now.
  if (request->method() != "GET") {
    DVLOG(1) << "Skip redirection: method is not GET";
    return NULL;
  }

  if (!request->url().SchemeIsHTTPOrHTTPS()) {
    DVLOG(1) << "Skip redirection: scheme is not HTTP or HTTPS";
    return NULL;
  }

  // The user has indicated that a URL should be force downloaded. Turn off
  // URL redirection in this case.
  if (ResourceRequestInfo::ForRequest(request)->IsDownload()) {
    DVLOG(1) << "Skip redirection: request is a forced download";
    return NULL;
  }

  // Never redirect URLs to apps in incognito. Technically, apps are not
  // supported in incognito, but that may change in future.
  // See crbug.com/240879, which tracks incognito support for v2 apps.
  if (profile_io_data->IsOffTheRecord()) {
    DVLOG(1) << "Skip redirection: unsupported in incognito";
    return NULL;
  }

  const extensions::ExtensionSet& extensions =
      profile_io_data->GetExtensionInfoMap()->extensions();
  for (extensions::ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end();
       ++iter) {
    const UrlHandlerInfo* handler =
        UrlHandlers::FindMatchingUrlHandler(*iter, request->url());
    if (handler) {
      DVLOG(1) << "Found matching app handler for redirection: "
               << (*iter)->name() << "(" << (*iter)->id() << "):"
               << handler->id;
      return new navigation_interception::InterceptNavigationResourceThrottle(
          request,
          base::Bind(&LaunchAppWithUrl,
                     scoped_refptr<const Extension>(*iter),
                     handler->id));
    }
  }

  DVLOG(1) << "Skipping redirection: no matching app handler found";
  return NULL;
}
