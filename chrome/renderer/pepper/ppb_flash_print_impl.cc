// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/pepper/ppb_flash_print_impl.h"

#include "chrome/renderer/pepper/ppb_pdf_impl.h"
#include "ppapi/c/private/ppb_flash_print.h"

namespace {

const PPB_Flash_Print flash_print_interface = {
  &PPB_PDF_Impl::InvokePrintingForInstance
};

}  // namespace

// static
const PPB_Flash_Print_1_0* PPB_Flash_Print_Impl::GetInterface() {
  return &flash_print_interface;
}
