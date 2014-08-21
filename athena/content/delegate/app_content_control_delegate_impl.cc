// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/public/app_content_control_delegate.h"

#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/app_runtime/app_runtime_api.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"

namespace athena {

class AppContentControlDelegateImpl : public AppContentControlDelegate {
 public:
  AppContentControlDelegateImpl() {}
  virtual ~AppContentControlDelegateImpl() {}

  // AppContentControlDelegate:
  virtual bool UnloadApplication(
      const std::string& app_id,
      content::BrowserContext* browser_context) OVERRIDE;
  virtual bool RestartApplication(
      const std::string& app_id,
      content::BrowserContext* browser_context) OVERRIDE;
  virtual std::string GetApplicationID(
      content::WebContents* web_contents) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppContentControlDelegateImpl);
};

bool AppContentControlDelegateImpl::UnloadApplication(
    const std::string& app_id,
    content::BrowserContext* browser_context) {
  // TODO(skuhne): Use the extension system to unload
  // (|ExtensionService::TerminateExtension|) once it becomes available in
  // Athena.
  return false;
}

bool AppContentControlDelegateImpl::RestartApplication(
    const std::string& app_id,
    content::BrowserContext* browser_context) {
  // TODO(skuhne): As soon as the ExtensionSystem can be used, we should use the
  // proper commands here for restarting.
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(browser_context)->GetExtensionById(
          app_id, extensions::ExtensionRegistry::EVERYTHING);
  DCHECK(extension);
  extensions::AppRuntimeEventRouter::DispatchOnLaunchedEvent(browser_context,
                                                             extension);
  return true;
}

// Get the extension Id from a given |web_contents|.
std::string AppContentControlDelegateImpl::GetApplicationID(
    content::WebContents* web_contents) {
  content::RenderViewHost* render_view_host = web_contents->GetRenderViewHost();
  // This works for both apps and extensions because the site has been
  // normalized to the extension URL for hosted apps.
  content::SiteInstance* site_instance = render_view_host->GetSiteInstance();
  if (!site_instance)
    return std::string();

  const GURL& site_url = site_instance->GetSiteURL();

  if (!site_url.SchemeIs(extensions::kExtensionScheme))
    return std::string();

  return site_url.host();
}

// static
AppContentControlDelegate*
AppContentControlDelegate::CreateAppContentControlDelegate() {
  return new AppContentControlDelegateImpl;
}

}  // namespace athena
