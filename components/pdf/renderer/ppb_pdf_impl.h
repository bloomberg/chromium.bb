// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENT_PDF_RENDERER_PPB_PDF_IMPL_H_
#define COMPONENT_PDF_RENDERER_PPB_PDF_IMPL_H_

#include "ppapi/c/pp_instance.h"

struct PPB_PDF;

namespace pdf {

class PPB_PDF_Impl {
 public:
  class PrintClient {
   public:
    virtual ~PrintClient() {}

    // Returns whether printing is enabled for the plugin instance identified by
    // |instance_id|.
    virtual bool IsPrintingEnabled(PP_Instance instance_id) = 0;

    // Invokes the "Print" command for the plugin instance identified by
    // |instance_id|. Returns whether the "Print" command was issued or not.
    virtual bool Print(PP_Instance instance_id) = 0;
  };

  // Returns a pointer to the interface implementing PPB_PDF that is exposed
  // to the plugin.
  static const PPB_PDF* GetInterface();

  // Invokes the "Print" command for the given instance as if the user right
  // clicked on it and selected "Print". Returns if the "Print" command was
  // issued or not.
  static bool InvokePrintingForInstance(PP_Instance instance);

  // The caller retains the ownership of |print_client|. The client is
  // allowed to be set only once, and when set, the client must outlive the
  // PPB_PDF_Impl instance.
  static void SetPrintClient(PrintClient* print_client);
};

}  // namespace pdf

#endif  // COMPONENT_PDF_RENDERER_PPB_PDF_IMPL_H_
