// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/chrome_ppapi_interfaces.h"

#include "chrome/renderer/pepper/ppb_nacl_private_impl.h"
#include "chrome/renderer/pepper/ppb_pdf_impl.h"
#include "ppapi/c/private/ppb_nacl_private.h"
#include "ppapi/c/private/ppb_pdf.h"
#include "webkit/plugins/ppapi/ppapi_interface_factory.h"

namespace chrome {

const void* ChromePPAPIInterfaceFactory(const std::string& interface_name) {
#if !defined(DISABLE_NACL)
  if (interface_name == PPB_NACL_PRIVATE_INTERFACE)
    return PPB_NaCl_Private_Impl::GetInterface();
#endif  // DISABLE_NACL
  if (interface_name == PPB_PDF_INTERFACE)
    return PPB_PDF_Impl::GetInterface();
  return NULL;
}

}  // namespace chrome
