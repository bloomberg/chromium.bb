// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/ksv/keyboard_shortcut_viewer_util.h"

#include "ash/components/shortcut_viewer/public/mojom/constants.mojom.h"
#include "ash/components/shortcut_viewer/views/keyboard_shortcut_view.h"
#include "ash/public/cpp/ash_switches.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "content/public/common/service_manager_connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace keyboard_shortcut_viewer_util {

void ShowKeyboardShortcutViewer() {
  base::TimeTicks user_gesture_time = base::TimeTicks::Now();
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kKeyboardShortcutViewerApp)) {
    service_manager::Connector* connector =
        content::ServiceManagerConnection::GetForProcess()->GetConnector();
    // TODO(jamescook): Pass |user_gesture_time| via a mojo Show() method.
    connector->StartService(shortcut_viewer::mojom::kServiceName);
  } else {
    keyboard_shortcut_viewer::KeyboardShortcutView::Toggle(user_gesture_time);
  }
}

}  // namespace keyboard_shortcut_viewer_util
