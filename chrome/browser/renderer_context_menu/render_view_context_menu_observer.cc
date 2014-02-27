// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/render_view_context_menu_observer.h"

void RenderViewContextMenuObserver::InitMenu(
    const content::ContextMenuParams& params) {
}

bool RenderViewContextMenuObserver::IsCommandIdSupported(int command_id) {
  return false;
}

bool RenderViewContextMenuObserver::IsCommandIdChecked(int command_id) {
  return false;
}

bool RenderViewContextMenuObserver::IsCommandIdEnabled(int command_id) {
  return false;
}

void RenderViewContextMenuObserver::ExecuteCommand(int command_id) {
}

void RenderViewContextMenuObserver::OnMenuCancel() {
}
