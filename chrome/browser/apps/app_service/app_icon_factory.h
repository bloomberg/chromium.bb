// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_FACTORY_H_

#include <string>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class BrowserContext;
}

namespace apps {

void LoadIconFromExtension(apps::mojom::IconCompression icon_compression,
                           int size_hint_in_dip,
                           content::BrowserContext* context,
                           const std::string& extension_id,
                           apps::mojom::Publisher::LoadIconCallback callback);

// The file named by |path| might be empty, not found or otherwise unreadable.
// If so, "fallback(callback)" is run. If the file is non-empty and readable,
// just "callback" is run, even if that file doesn't contain a valid image.
void LoadIconFromFileWithFallback(
    apps::mojom::IconCompression icon_compression,
    int size_hint_in_dip,
    const base::FilePath& path,
    apps::mojom::Publisher::LoadIconCallback callback,
    base::OnceCallback<void(apps::mojom::Publisher::LoadIconCallback)>
        fallback);

void LoadIconFromResource(apps::mojom::IconCompression icon_compression,
                          int size_hint_in_dip,
                          int resource_id,
                          bool is_placeholder_icon,
                          apps::mojom::Publisher::LoadIconCallback callback);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_FACTORY_H_
