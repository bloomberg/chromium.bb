// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/ephemeral_app_throttle.h"

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/webstore_ephemeral_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_io_data.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/navigation_interception/intercept_navigation_resource_throttle.h"
#include "components/navigation_interception/navigation_params.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;
using content::WebContents;
using extensions::Extension;
using extensions::WebstoreEphemeralInstaller;

namespace {

bool LaunchEphemeralApp(
    const std::string& app_id,
    content::RenderViewHost* source,
    const navigation_interception::NavigationParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Redirect top-level navigations only.
  if (source->IsSubframe())
    return false;

  WebContents* web_contents = WebContents::FromRenderViewHost(source);
  if (!web_contents)
    return false;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile)
    return false;

  ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  DCHECK(service);
  const Extension* extension = service->GetExtensionById(app_id, false);

  if (extension && extension->is_app()) {
    // If the app is already installed, launch it.
    AppLaunchParams params(profile, extension, NEW_FOREGROUND_TAB);
    params.desktop_type = chrome::GetHostDesktopTypeForNativeView(
        web_contents->GetView()->GetNativeView());
    OpenApplication(params);
    return true;
  }

  if (!extension) {
    // Install ephemeral app and launch.
    scoped_refptr<WebstoreEphemeralInstaller> installer =
        WebstoreEphemeralInstaller::CreateForLink(app_id, web_contents);
    installer->BeginInstall();
    return true;
  }

  return false;
}

}  // namespace

// static
content::ResourceThrottle*
EphemeralAppThrottle::MaybeCreateThrottleForLaunch(
    net::URLRequest* request,
    ProfileIOData* profile_io_data) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableEphemeralApps))
    return NULL;

  if (request->method() != "GET" || !request->url().SchemeIsHTTPOrHTTPS())
    return NULL;

  // Not supported for incognito profiles.
  if (profile_io_data->is_incognito())
    return NULL;

  // Crudely watch for links to Chrome Web Store detail pages and assume that
  // the app ID will be after the last slash of the URL. We cannot even
  // differentiate between apps and extensions, so attempt to launch both.
  // This is obviously for experimental purposes only and will be implemented
  // properly in production code - for example, links to ephemeral apps could
  // have a new scheme.
  if (request->url().spec().find(
          extension_urls::GetWebstoreItemDetailURLPrefix()) != 0)
    return NULL;

  std::string app_id(request->url().ExtractFileName());
  if (!Extension::IdIsValid(app_id))
    return NULL;

  return new navigation_interception::InterceptNavigationResourceThrottle(
      request,
      base::Bind(&LaunchEphemeralApp, app_id));
}
