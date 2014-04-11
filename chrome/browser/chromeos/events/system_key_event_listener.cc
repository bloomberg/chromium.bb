// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/events/system_key_event_listener.h"

#define XK_MISCELLANY 1
#include <X11/keysymdef.h>
#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>
#undef Status

#include "chromeos/ime/ime_keyboard.h"
#include "chromeos/ime/input_method_manager.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_source.h"

namespace chromeos {

namespace {
static SystemKeyEventListener* g_system_key_event_listener = NULL;
}  // namespace

// static
void SystemKeyEventListener::Initialize() {
  CHECK(!g_system_key_event_listener);
  g_system_key_event_listener = new SystemKeyEventListener();
}

// static
void SystemKeyEventListener::Shutdown() {
  // We may call Shutdown without calling Initialize, e.g. if we exit early.
  if (g_system_key_event_listener) {
    delete g_system_key_event_listener;
    g_system_key_event_listener = NULL;
  }
}

// static
SystemKeyEventListener* SystemKeyEventListener::GetInstance() {
  return g_system_key_event_listener;
}

SystemKeyEventListener::SystemKeyEventListener()
    : stopped_(false),
      xkb_event_base_(0) {
  XDisplay* display = gfx::GetXDisplay();
  int xkb_major_version = XkbMajorVersion;
  int xkb_minor_version = XkbMinorVersion;
  if (!XkbQueryExtension(display,
                         NULL,  // opcode_return
                         &xkb_event_base_,
                         NULL,  // error_return
                         &xkb_major_version,
                         &xkb_minor_version)) {
    LOG(WARNING) << "Could not query Xkb extension";
  }

  if (!XkbSelectEvents(display, XkbUseCoreKbd,
                       XkbStateNotifyMask,
                       XkbStateNotifyMask)) {
    LOG(WARNING) << "Could not install Xkb Indicator observer";
  }

  ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
}

SystemKeyEventListener::~SystemKeyEventListener() {
  Stop();
}

void SystemKeyEventListener::Stop() {
  if (stopped_)
    return;
  ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
  stopped_ = true;
}

void SystemKeyEventListener::WillProcessEvent(const base::NativeEvent& event) {
  ProcessedXEvent(event);
}

void SystemKeyEventListener::DidProcessEvent(const base::NativeEvent& event) {
}

void SystemKeyEventListener::ProcessedXEvent(XEvent* xevent) {
  input_method::InputMethodManager* input_method_manager =
      input_method::InputMethodManager::Get();

  if (xevent->type == xkb_event_base_) {
    // TODO(yusukes): Move this part to aura::WindowTreeHost.
    XkbEvent* xkey_event = reinterpret_cast<XkbEvent*>(xevent);
    if (xkey_event->any.xkb_type == XkbStateNotify) {
      if (xkey_event->state.mods) {
        // TODO(yusukes,adlr): Let the user know that num lock is unsupported.
        // Force turning off Num Lock (crosbug.com/29169)
        input_method_manager->GetImeKeyboard()->DisableNumLock();
      }
    }
  }
}

}  // namespace chromeos
