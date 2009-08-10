// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#include "base/logging.h"

using WebKit::WebFrame;

void PrintWebViewHelper::Print(WebFrame* frame, bool script_initiated) {
  // TODO(port): print not implemented
  NOTIMPLEMENTED();
}

void PrintWebViewHelper::PrintPage(const ViewMsg_PrintPage_Params& params,
                                   const gfx::Size& canvas_size,
                                   WebFrame* frame) {
  // TODO(port) implement printing
  NOTIMPLEMENTED();
}

