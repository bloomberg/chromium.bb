// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/shortcut_viewer/shortcut_viewer_application.h"

#include "ash/components/shortcut_viewer/last_window_closed_observer.h"
#include "ash/components/shortcut_viewer/views/keyboard_shortcut_view.h"
#include "base/trace_event/trace_event.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/views/mus/aura_init.h"

namespace keyboard_shortcut_viewer {

ShortcutViewerApplication::ShortcutViewerApplication() = default;
ShortcutViewerApplication::~ShortcutViewerApplication() = default;

// static
void ShortcutViewerApplication::RegisterForTraceEvents() {
  TRACE_EVENT0("shortcut_viewer", "ignored");
}

void ShortcutViewerApplication::OnStart() {
  aura_init_ = views::AuraInit::Create(
      context()->connector(), context()->identity(), "views_mus_resources.pak",
      std::string(), nullptr, views::AuraInit::Mode::AURA_MUS2,
      false /*register_path_provider*/);
  if (!aura_init_) {
    context()->QuitNow();
    return;
  }

  // Quit the application when the window is closed.
  last_window_closed_observer_ = std::make_unique<LastWindowClosedObserver>(
      context()->CreateQuitClosure());

  // This app needs InputDeviceManager information that loads asynchronously via
  // InputDeviceClient. If the device list is incomplete, wait for it to load.
  DCHECK(ui::InputDeviceManager::HasInstance());
  if (ui::InputDeviceManager::GetInstance()->AreDeviceListsComplete()) {
    KeyboardShortcutView::Toggle();
  } else {
    ui::InputDeviceManager::GetInstance()->AddObserver(this);
  }
}

void ShortcutViewerApplication::OnDeviceListsComplete() {
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
  KeyboardShortcutView::Toggle();
}

}  // namespace keyboard_shortcut_viewer
