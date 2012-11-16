// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_flash_host.h"

#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/c/pp_errors.h"

namespace content {

PepperFlashHost::PepperFlashHost(
    RendererPpapiHost* host,
    PP_Instance instance,
    PP_Resource resource)
    : ResourceHost(host->GetPpapiHost(), instance, resource) {
}

PepperFlashHost::~PepperFlashHost() {
}

int32_t PepperFlashHost::OnResourceMessageReceived(
    const IPC::Message& msg,
    ppapi::host::HostMessageContext* context) {
  return PP_ERROR_FAILED;
}
}  // namespace content
