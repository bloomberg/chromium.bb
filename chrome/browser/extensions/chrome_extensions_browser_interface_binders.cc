// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extensions_browser_interface_binders.h"

#include "base/bind.h"
#include "chrome/browser/media/router/media_router_feature.h"       // nogncheck
#include "chrome/browser/media/router/mojo/media_router_desktop.h"  // nogncheck
#include "chrome/common/media_router/mojom/media_router.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"

namespace extensions {

void PopulateChromeFrameBindersForExtension(
    service_manager::BinderMapWithContext<content::RenderFrameHost*>*
        binder_map,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) {
  DCHECK(extension);
  auto* context = render_frame_host->GetProcess()->GetBrowserContext();
  if (media_router::MediaRouterEnabled(context) &&
      extension->permissions_data()->HasAPIPermission(
          APIPermission::kMediaRouterPrivate)) {
    binder_map->Add<media_router::mojom::MediaRouter>(
        base::BindRepeating(&media_router::MediaRouterDesktop::BindToReceiver,
                            base::RetainedRef(extension), context));
  }
}

}  // namespace extensions
