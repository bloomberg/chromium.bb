// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_mouse_watcher_win.h"

#include <windows.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"

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

class PanelMouseWatcherWin {
 public:
  PanelMouseWatcherWin();
  ~PanelMouseWatcherWin();

 private:
  static LRESULT CALLBACK MouseHookProc(int code, WPARAM wparam, LPARAM lparam);

  void OnMouseAction(int mouse_x, int mouse_y);

  HHOOK mouse_hook_;
  bool bring_up_titlebars_;

  DISALLOW_COPY_AND_ASSIGN(PanelMouseWatcherWin);
};

scoped_ptr<PanelMouseWatcherWin> mouse_watcher;

PanelMouseWatcherWin::PanelMouseWatcherWin()
    : mouse_hook_(NULL),
      bring_up_titlebars_(false) {
  mouse_hook_ = ::SetWindowsHookEx(
      WH_MOUSE_LL, MouseHookProc, GetCurrentModuleHandle(), 0);
  DCHECK(mouse_hook_);
}

PanelMouseWatcherWin::~PanelMouseWatcherWin() {
  ::UnhookWindowsHookEx(mouse_hook_);
}

LRESULT CALLBACK PanelMouseWatcherWin::MouseHookProc(int code,
                                                     WPARAM wparam,
                                                     LPARAM lparam) {
  if (code == HC_ACTION) {
    MOUSEHOOKSTRUCT* hook_struct = reinterpret_cast<MOUSEHOOKSTRUCT*>(lparam);
    if (hook_struct)
      mouse_watcher->OnMouseAction(hook_struct->pt.x, hook_struct->pt.y);
  }
  return ::CallNextHookEx(NULL, code, wparam, lparam);
}

void PanelMouseWatcherWin::OnMouseAction(int mouse_x, int mouse_y) {
  PanelManager* panel_manager = PanelManager::GetInstance();

  bool bring_up_titlebars = panel_manager->ShouldBringUpTitlebars(
      mouse_x, mouse_y);
  if (bring_up_titlebars == bring_up_titlebars_)
    return;
  bring_up_titlebars_ = bring_up_titlebars;

  panel_manager->BringUpOrDownTitlebars(bring_up_titlebars);
}

}  // namespace

void EnsureMouseWatcherStarted() {
  if (!mouse_watcher.get())
    mouse_watcher.reset(new PanelMouseWatcherWin());
}

void StopMouseWatcher() {
  mouse_watcher.reset(NULL);
}

bool IsMouseWatcherStarted() {
  return mouse_watcher.get() != NULL;
}
