// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mojo/service_registration_manager.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "device/serial/serial_service_impl.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/process_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/switches.h"

namespace extensions {
namespace {

base::LazyInstance<ServiceRegistrationManager> g_lazy_instance =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ServiceRegistrationManager::ServiceRegistrationManager() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableMojoSerialService)) {
    AddServiceFactory(
        "serial",
        base::Bind(device::SerialServiceImpl::CreateOnMessageLoop,
                   content::BrowserThread::GetMessageLoopProxyForThread(
                       content::BrowserThread::FILE),
                   content::BrowserThread::GetMessageLoopProxyForThread(
                       content::BrowserThread::IO),
                   content::BrowserThread::GetMessageLoopProxyForThread(
                       content::BrowserThread::UI)));
  }
}

ServiceRegistrationManager::~ServiceRegistrationManager() {
}

ServiceRegistrationManager* ServiceRegistrationManager::GetSharedInstance() {
  return g_lazy_instance.Pointer();
}

void ServiceRegistrationManager::AddServicesToRenderFrame(
    content::RenderFrameHost* render_frame_host) {
  content::BrowserContext* context =
      render_frame_host->GetProcess()->GetBrowserContext();
  content::SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  GURL extension_url = site_instance->GetSiteURL();
  ExtensionRegistry* registry = ExtensionRegistry::Get(context);

  // TODO(sammc): Handle content scripts and web pages that have access to some
  // extension APIs.
  if (!extension_url.SchemeIs(kExtensionScheme)) {
    return;
  }

  const Extension* extension =
      registry->enabled_extensions().GetByID(extension_url.host());
  if (!extension)
    return;

  Feature::Context context_type =
      ProcessMap::Get(context)->GetMostLikelyContextType(
          extension, render_frame_host->GetProcess()->GetID());
  for (const auto& factory : factories_) {
    auto availability = ExtensionAPI::GetSharedInstance()->IsAvailable(
        factory.first, extension, context_type, extension_url);
    if (availability.is_available())
      factory.second->Register(render_frame_host->GetServiceRegistry());
  }
}

}  // namespace extensions
