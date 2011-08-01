// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/tab_util.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_web_ui_factory.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/renderer_host/render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/site_instance.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "googleurl/src/gurl.h"

namespace tab_util {

TabContents* GetTabContentsByID(int render_process_id, int render_view_id) {
  RenderViewHost* render_view_host =
      RenderViewHost::FromID(render_process_id, render_view_id);
  if (!render_view_host)
    return NULL;

  return render_view_host->delegate()->GetAsTabContents();
}

SiteInstance* GetSiteInstanceForNewTab(TabContents* source_contents,
                                       Profile* profile,
                                       const GURL& url) {
  // If url is a WebUI or extension, we need to be sure to use the right type
  // of renderer process up front.  Otherwise, we create a normal SiteInstance
  // as part of creating the tab.
  ExtensionService* service = profile->GetExtensionService();
  if (ChromeWebUIFactory::GetInstance()->UseWebUIForURL(profile, url) ||
      (service && service->GetExtensionByWebExtent(url))) {
    return SiteInstance::CreateSiteInstanceForURL(profile, url);
  }

  if (!source_contents)
    return NULL;

  // Don't use this logic when "--process-per-tab" is specified.
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kProcessPerTab) &&
      SiteInstance::IsSameWebSite(source_contents->browser_context(),
                                  source_contents->GetURL(),
                                  url)) {
    return source_contents->GetSiteInstance();
  }
  return NULL;
}

}  // namespace tab_util
