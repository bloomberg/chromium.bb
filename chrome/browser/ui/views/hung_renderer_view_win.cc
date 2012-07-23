// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/hung_renderer_view_win.h"

// static
HungRendererDialogView* HungRendererDialogView::Create() {
  if (!g_instance_)
    g_instance_ = new HungRendererDialogViewWin();
  return g_instance_;
}

void HungRendererDialogViewWin::ShowForWebContents(WebContents* contents) {
  HungRendererDialogView::ShowForWebContents(contents);
}

void HungRendererDialogViewWin::EndForWebContents(WebContents* contents) {
  HungRendererDialogView::EndForWebContents(contents);
}
