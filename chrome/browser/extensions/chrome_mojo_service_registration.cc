// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_mojo_service_registration.h"

#include "base/logging.h"

namespace extensions {

void RegisterChromeServicesForFrame(content::RenderFrameHost* render_frame_host,
                                    const Extension* extension) {
  DCHECK(render_frame_host);
  DCHECK(extension);

  // TODO(kmarshall): Add Mojo service registration.
}

}  // namespace extensions
