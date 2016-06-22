// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/app/blimp_content_browser_client.h"
#include "blimp/engine/app/blimp_browser_main_parts.h"
#include "blimp/engine/app/settings_manager.h"
#include "blimp/engine/mojo/blob_channel_service.h"
#include "content/public/common/service_registry.h"

namespace blimp {
namespace engine {

BlimpContentBrowserClient::BlimpContentBrowserClient() {}

BlimpContentBrowserClient::~BlimpContentBrowserClient() {}

content::BrowserMainParts* BlimpContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  blimp_browser_main_parts_ = new BlimpBrowserMainParts(parameters);
  // BrowserMainLoop takes ownership of the returned BrowserMainParts.
  return blimp_browser_main_parts_;
}

void BlimpContentBrowserClient::OverrideWebkitPrefs(
    content::RenderViewHost* render_view_host,
    content::WebPreferences* prefs) {
  if (!blimp_browser_main_parts_)
    return;

  if (!blimp_browser_main_parts_->GetSettingsManager())
    return;

  blimp_browser_main_parts_->GetSettingsManager()->UpdateWebkitPreferences(
      prefs);
}

BlimpBrowserContext* BlimpContentBrowserClient::GetBrowserContext() {
  return blimp_browser_main_parts_->GetBrowserContext();
}

content::LocationProvider*
BlimpContentBrowserClient::OverrideSystemLocationProvider() {
  if (!location_provider_) {
    location_provider_ = base::WrapUnique(new BlimpLocationProvider());
  }
  return location_provider_.get();
}

bool BlimpContentBrowserClient::UseNetworkLocationProviders() {
  return false;
}

void BlimpContentBrowserClient::RegisterRenderProcessMojoServices(
    content::ServiceRegistry* registry,
    content::RenderProcessHost* render_process_host) {
  registry->AddService<mojom::BlobChannel>(
      base::Bind(&BlobChannelService::Create,
                 blimp_browser_main_parts_->GetBlobChannelSender()));
}

}  // namespace engine
}  // namespace blimp
