// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/renderer_host/browser_render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"

namespace chrome {

void ChromeContentBrowserClient::OnRenderViewCreation(
    RenderViewHost* render_view_host,
    Profile* profile,
    const GURL& url) {
  // Tell the RenderViewHost whether it will be used for an extension process.
  ExtensionService* service = profile->GetExtensionService();
  if (service) {
    bool is_extension_process = service->ExtensionBindingsAllowed(url);
    render_view_host->set_is_extension_process(is_extension_process);

    const Extension* installed_app = service->GetInstalledApp(url);
    static_cast<BrowserRenderProcessHost*>(render_view_host->process())->
        set_installed_app(installed_app);
  }
}

}  // namespace chrome
