// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mojo/service_registration.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/service_registry.h"
#include "extensions/browser/api/serial/serial_service_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/mojo/keep_alive_impl.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/switches.h"

namespace extensions {
namespace {

const Extension* GetExtension(content::RenderFrameHost* render_frame_host) {
  content::SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  GURL url = render_frame_host->GetLastCommittedURL();
  if (!url.is_empty()) {
    if (site_instance->GetSiteURL().GetOrigin() != url.GetOrigin())
      return nullptr;
  } else {
    url = site_instance->GetSiteURL();
  }
  content::BrowserContext* browser_context = site_instance->GetBrowserContext();
  if (!url.SchemeIs(kExtensionScheme))
    return nullptr;

  return ExtensionRegistry::Get(browser_context)
      ->enabled_extensions()
      .GetExtensionOrAppByURL(url);
}

bool ExtensionHasPermission(const Extension* extension,
                            content::RenderProcessHost* render_process_host,
                            const std::string& permission_name) {
  Feature::Context context =
      ProcessMap::Get(render_process_host->GetBrowserContext())
          ->GetMostLikelyContextType(extension, render_process_host->GetID());

  return ExtensionAPI::GetSharedInstance()
      ->IsAvailable(permission_name, extension, context, extension->url())
      .is_available();
}

}  // namespace

void RegisterCoreExtensionServices(
    content::RenderFrameHost* render_frame_host) {
  const Extension* extension = GetExtension(render_frame_host);
  if (!extension)
    return;

  content::ServiceRegistry* service_registry =
      render_frame_host->GetServiceRegistry();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableMojoSerialService)) {
    if (ExtensionHasPermission(extension, render_frame_host->GetProcess(),
                               "serial")) {
      service_registry->AddService(base::Bind(&BindToSerialServiceRequest));
    }
  }
  service_registry->AddService(base::Bind(
      KeepAliveImpl::Create,
      render_frame_host->GetProcess()->GetBrowserContext(), extension));
}

}  // namespace extensions
