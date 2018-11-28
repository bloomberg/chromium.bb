// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_FACTORY_H_

#include <string>

#include "base/callback_forward.h"
#include "chrome/services/app_service/public/mojom/app_service.mojom.h"
#include "chrome/services/app_service/public/mojom/types.mojom.h"
#include "ui/gfx/image/image_skia.h"

namespace content {
class BrowserContext;
}

namespace apps {

// TODO: delete this. See below.
using LoadIconRepeatingCallback =
    base::RepeatingCallback<void(apps::mojom::IconValuePtr icon_value)>;

void LoadIconFromExtension(
    apps::mojom::IconCompression icon_compression,
    int size_hint_in_dip,
    // TODO: the next line should be:
    //
    // apps::mojom::Publisher::LoadIconCallback callback,
    //
    // In other words, we should drop the "Repeating". This will require fixing
    // extensions::ImageLoader to take a base::OnceCallback instead of an
    // (implicitly repeating) base::Callback.
    LoadIconRepeatingCallback callback,
    content::BrowserContext* context,
    const std::string& extension_id);

void LoadIconFromResource(apps::mojom::IconCompression icon_compression,
                          int size_hint_in_dip,
                          apps::mojom::Publisher::LoadIconCallback callback,
                          int resource_id);

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_ICON_FACTORY_H_
