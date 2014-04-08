// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PEPPER_PPB_PDF_IMPL_H_
#define CHROME_RENDERER_PEPPER_PPB_PDF_IMPL_H_

#include "ppapi/c/pp_instance.h"

struct PPB_PDF;

class PPB_PDF_Impl {
 public:
  // Returns a pointer to the interface implementing PPB_PDF that is exposed
  // to the plugin.
  static const PPB_PDF* GetInterface();

  // Invokes the "Print" command for the given instance as if the user right
  // clicked on it and selected "Print".
  static void InvokePrintingForInstance(PP_Instance instance);
};

#endif  // CHROME_RENDERER_PEPPER_PPB_PDF_IMPL_H_
