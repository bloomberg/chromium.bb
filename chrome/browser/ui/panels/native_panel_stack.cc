// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/native_panel_stack.h"

#include "base/logging.h"

// static
#if !defined(TOOLKIT_VIEWS)
NativePanelStack* NativePanelStack::Create(
    scoped_ptr<StackedPanelCollection> stack) {
  NOTIMPLEMENTED();
  return NULL;
}
#endif
