// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/window_finder_mus.h"

#include "content/public/common/service_manager_connection.h"  // nogncheck
#include "services/service_manager/runner/common/client_util.h"  // nogncheck
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/views/mus/mus_client.h"

bool GetLocalProcessWindowAtPointMus(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore,
    gfx::NativeWindow* mus_result) {
  *mus_result = nullptr;
  content::ServiceManagerConnection* service_manager_connection =
      content::ServiceManagerConnection::GetForProcess();
  if (!service_manager_connection || !service_manager::ServiceManagerIsRemote())
    return false;

  std::set<aura::Window*> root_windows =
      views::MusClient::Get()->window_tree_client()->GetRoots();
  // TODO(erg): Needs to deal with stacking order here.

  // For every mus window, look at the associated aura window and see if we're
  // in that.
  for (aura::Window* root : root_windows) {
    views::Widget* widget = views::Widget::GetWidgetForNativeView(root);
    if (widget && widget->GetWindowBoundsInScreen().Contains(screen_point)) {
      aura::Window* content_window = widget->GetNativeWindow();

      // If we were instructed to ignore this window, ignore it.
      if (base::ContainsKey(ignore, content_window))
        continue;

      *mus_result = content_window;
      return true;
    }
  }

  return true;
}
