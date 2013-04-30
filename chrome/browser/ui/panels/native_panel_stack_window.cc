// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/native_panel_stack_window.h"

#include "base/logging.h"

// static
#if !defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
NativePanelStackWindow* NativePanelStackWindow::Create(
    NativePanelStackWindowDelegate* delegate) {
  NOTIMPLEMENTED();
  return NULL;
}
#endif
