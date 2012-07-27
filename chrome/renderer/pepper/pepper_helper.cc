// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/pepper_helper.h"

#include "chrome/renderer/pepper/chrome_renderer_pepper_host_factory.h"
#include "chrome/renderer/pepper/pepper_flash_renderer_message_filter.h"
#include "ppapi/host/ppapi_host.h"

namespace chrome {

PepperHelper::PepperHelper(content::RenderView* render_view)
    : RenderViewObserver(render_view) {
}

PepperHelper::~PepperHelper() {
}

void PepperHelper::DidCreatePepperPlugin(ppapi::host::PpapiHost* host) {
  // TODO(brettw) figure out how to hook up the host factory. It needs some
  // kind of filter-like system to allow dynamic additions.
  // new ChromeRendererPepperHostFactory(host);
  host->AddInstanceMessageFilter(
      scoped_ptr<ppapi::host::InstanceMessageFilter>(
          new PepperFlashRendererMessageFilter(host)));
}

}  // namespace chrome
