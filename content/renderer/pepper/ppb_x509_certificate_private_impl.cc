// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_x509_certificate_private_impl.h"

#include "content/renderer/render_thread_impl.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace content {

PPB_X509Certificate_Private_Impl::PPB_X509Certificate_Private_Impl(
    PP_Instance instance) :
  PPB_X509Certificate_Private_Shared(ppapi::OBJECT_IS_IMPL, instance) {
}

// static
PP_Resource PPB_X509Certificate_Private_Impl::CreateResource(
    PP_Instance instance) {
  return (new PPB_X509Certificate_Private_Impl(instance))->GetReference();
}

bool PPB_X509Certificate_Private_Impl::ParseDER(
    const std::vector<char>& der,
    ppapi::PPB_X509Certificate_Fields* result) {
  bool succeeded = false;
  RenderThreadImpl::current()->Send(
      new PpapiHostMsg_PPBX509Certificate_ParseDER(der, &succeeded, result));
  return succeeded;
}

PPB_X509Certificate_Private_Impl::~PPB_X509Certificate_Private_Impl() {
}

}  // namespace content
