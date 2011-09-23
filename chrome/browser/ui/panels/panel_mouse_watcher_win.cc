// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"

namespace {

HMODULE GetModuleHandleFromAddress(void *address) {
  MEMORY_BASIC_INFORMATION mbi;
  SIZE_T result = ::VirtualQuery(address, &mbi, sizeof(mbi));
  return static_cast<HMODULE>(mbi.AllocationBase);
}


// Gets the handle to the currently executing module.
HMODULE GetCurrentModuleHandle() {
  return ::GetModuleHandleFromAddress(GetCurrentModuleHandle);
}

}  // namespace

class PanelMouseWatcherWin : public PanelMouseWatcher {
 public:
  PanelMouseWatcherWin();
  virtual ~PanelMouseWatcherWin();

  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;

 private:
  static LRESULT CALLBACK MouseHookProc(int code, WPARAM wparam, LPARAM lparam);

  HHOOK mouse_hook_;

  DISALLOW_COPY_AND_ASSIGN(PanelMouseWatcherWin);
};

scoped_ptr<PanelMouseWatcherWin> mouse_watcher;

// static
PanelMouseWatcher* PanelMouseWatcher::GetInstance() {
  if (!mouse_watcher.get())
    mouse_watcher.reset(new PanelMouseWatcherWin());

  return mouse_watcher.get();
}

PanelMouseWatcherWin::PanelMouseWatcherWin()
    : mouse_hook_(NULL) {
}

PanelMouseWatcherWin::~PanelMouseWatcherWin() {
  DCHECK(!mouse_hook_);
}

void PanelMouseWatcherWin::Start() {
  DCHECK(!mouse_hook_);
  mouse_hook_ = ::SetWindowsHookEx(
      WH_MOUSE_LL, MouseHookProc, GetCurrentModuleHandle(), 0);
  DCHECK(mouse_hook_);
}

void PanelMouseWatcherWin::Stop() {
  DCHECK(mouse_hook_);
  ::UnhookWindowsHookEx(mouse_hook_);
  mouse_hook_ = NULL;
}

LRESULT CALLBACK PanelMouseWatcherWin::MouseHookProc(int code,
                                                     WPARAM wparam,
                                                     LPARAM lparam) {
  if (code == HC_ACTION) {
    MOUSEHOOKSTRUCT* hook_struct = reinterpret_cast<MOUSEHOOKSTRUCT*>(lparam);
    if (hook_struct) {
      mouse_watcher->HandleMouseMovement(
          gfx::Point(hook_struct->pt.x, hook_struct->pt.y));
    }
  }
  return ::CallNextHookEx(NULL, code, wparam, lparam);
}
