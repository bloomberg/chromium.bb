// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_url_redirector.h"

#include "apps/launcher.h"
#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_set.h"
#include "components/navigation_interception/intercept_navigation_resource_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/info_map.h"
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
    content::RenderViewHost* source,
    const navigation_interception::NavigationParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Redirect top-level navigations only. This excludes iframes and webviews
  // in particular.
  if (source->IsSubframe())
    return false;

  // These are guaranteed by CreateThrottleFor below.
  DCHECK(!params.is_post());
  DCHECK(UrlHandlers::CanExtensionHandleUrl(app, params.url()));

  WebContents* web_contents = WebContents::FromRenderViewHost(source);
  if (!web_contents)
    return false;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());

  apps::LaunchPlatformAppWithUrl(
      profile, app, handler_id, params.url(), params.referrer().url);

  return true;
}

}  // namespace

// static
content::ResourceThrottle*
AppUrlRedirector::MaybeCreateThrottleFor(net::URLRequest* request,
                                         ProfileIOData* profile_io_data) {
  // Support only GET for now.
  if (request->method() != "GET")
    return NULL;

  if (!request->url().SchemeIsHTTPOrHTTPS())
    return NULL;

  // Never redirect URLs to apps in incognito. Technically, apps are not
  // supported in incognito, but that may change in future.
  // See crbug.com/240879, which tracks incognito support for v2 apps.
  if (profile_io_data->is_incognito())
    return NULL;

  const ExtensionSet& extensions =
      profile_io_data->GetExtensionInfoMap()->extensions();
  for (ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end();
       ++iter) {
    const UrlHandlerInfo* handler =
        UrlHandlers::FindMatchingUrlHandler(*iter, request->url());
    if (handler) {
      return new navigation_interception::InterceptNavigationResourceThrottle(
          request,
          base::Bind(&LaunchAppWithUrl,
                     scoped_refptr<const Extension>(*iter),
                     handler->id));
    }
  }

  return NULL;
}
