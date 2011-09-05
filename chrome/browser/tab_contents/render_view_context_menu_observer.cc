// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/render_view_context_menu_observer.h"

void RenderViewContextMenuObserver::InitMenu(const ContextMenuParams& params) {
}

bool RenderViewContextMenuObserver::IsCommandIdSupported(int command_id) {
  return false;
}

bool RenderViewContextMenuObserver::IsCommandIdEnabled(int command_id) {
  return false;
}

void RenderViewContextMenuObserver::ExecuteCommand(int command_id) {
}
