// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_mojo_service_registration.h"

#include "base/bind.h"
#include "base/logging.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "services/service_manager/public/cpp/interface_registry.h"

#if defined(ENABLE_MEDIA_ROUTER)
#include "chrome/browser/media/router/media_router_feature.h"  // nogncheck
#include "chrome/browser/media/router/mojo/media_router_mojo_impl.h"  // nogncheck
#endif

namespace extensions {

void RegisterChromeServicesForFrame(content::RenderFrameHost* render_frame_host,
                                    const Extension* extension) {
  DCHECK(render_frame_host);
  DCHECK(extension);

#if defined(ENABLE_MEDIA_ROUTER)
  content::BrowserContext* context =
      render_frame_host->GetProcess()->GetBrowserContext();
  if (media_router::MediaRouterEnabled(context)) {
    if (extension->permissions_data()->HasAPIPermission(
            APIPermission::kMediaRouterPrivate)) {
      render_frame_host->GetInterfaceRegistry()->AddInterface(
          base::Bind(media_router::MediaRouterMojoImpl::BindToRequest,
                     extension, context));
    }
  }
#endif  // defined(ENABLE_MEDIA_ROUTER)
}

}  // namespace extensions
