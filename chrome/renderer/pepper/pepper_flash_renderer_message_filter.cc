// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/pepper_flash_renderer_message_filter.h"

#include "chrome/renderer/pepper/ppb_pdf_impl.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace chrome {

PepperFlashRendererMessageFilter::PepperFlashRendererMessageFilter(
    content::RendererPpapiHost* host)
    : InstanceMessageFilter(host->GetPpapiHost()),
      host_(host) {
}

PepperFlashRendererMessageFilter::~PepperFlashRendererMessageFilter() {
}

bool PepperFlashRendererMessageFilter::OnInstanceMessageReceived(
    const IPC::Message& msg) {
  return false;
}

}  // namespace chrome
