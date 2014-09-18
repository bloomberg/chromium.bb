// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/ephemeral_app_throttle.h"

#include "base/command_line.h"
#include "chrome/browser/apps/ephemeral_app_launcher.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/common/chrome_switches.h"
#include "components/crx_file/id_util.h"
#include "components/navigation_interception/intercept_navigation_resource_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_urls.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::WebContents;
using extensions::Extension;

namespace {

bool LaunchEphemeralApp(
    const std::string& app_id,
    content::WebContents* source,
    const navigation_interception::NavigationParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Redirect top-level navigations only.
  if (source->IsSubframe())
    return false;

  Profile* profile =
      Profile::FromBrowserContext(source->GetBrowserContext());
  if (!profile)
    return false;

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(service);
  const Extension* extension = service->GetInstalledExtension(app_id);

  // Only launch apps.
  if (extension && !extension->is_app())
    return false;

  // The EphemeralAppLauncher will handle launching of an existing app or
  // installing and launching a new ephemeral app.
  scoped_refptr<EphemeralAppLauncher> installer =
      EphemeralAppLauncher::CreateForWebContents(
          app_id, source, EphemeralAppLauncher::LaunchCallback());
  installer->Start();
  return true;
}

}  // namespace

// static
content::ResourceThrottle*
EphemeralAppThrottle::MaybeCreateThrottleForLaunch(
    net::URLRequest* request,
    ProfileIOData* profile_io_data) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableLinkableEphemeralApps))
    return NULL;

  if (request->method() != "GET" || !request->url().SchemeIsHTTPOrHTTPS())
    return NULL;

  // Not supported for incognito profiles.
  if (profile_io_data->IsOffTheRecord())
    return NULL;

  // Only watch for links in Google search results.
  if (request->referrer().find("https://www.google.com") == std::string::npos)
    return NULL;

  // Crudely watch for links to Chrome Web Store detail pages and assume that
  // the app ID will be after the last slash of the URL. We cannot even
  // differentiate between apps and extensions, so attempt to launch both.
  // This is obviously for demonstration purposes only and will be implemented
  // properly in production code - for example, links to ephemeral apps could
  // have a new scheme.
  if (request->url().spec().find(
          extension_urls::GetWebstoreItemDetailURLPrefix()) != 0)
    return NULL;

  std::string app_id(request->url().ExtractFileName());
  if (!crx_file::id_util::IdIsValid(app_id))
    return NULL;

  return new navigation_interception::InterceptNavigationResourceThrottle(
      request,
      base::Bind(&LaunchEphemeralApp, app_id));
}
