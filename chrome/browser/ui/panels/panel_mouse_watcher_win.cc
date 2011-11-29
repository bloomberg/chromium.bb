// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/panel_mouse_watcher.h"
#include "ui/gfx/point.h"

namespace {

HMODULE GetModuleHandleFromAddress(void *address) {
  MEMORY_BASIC_INFORMATION mbi;
  SIZE_T result = ::VirtualQuery(address, &mbi, sizeof(mbi));
  return static_cast<HMODULE>(mbi.AllocationBase);
}


// Gets the handle to the currently executing module.
HMODULE GetCurrentModuleHandle() {
  return GetModuleHandleFromAddress(GetCurrentModuleHandle);
}

}  // namespace

class PanelMouseWatcherWin : public PanelMouseWatcher {
 public:
  PanelMouseWatcherWin();
  virtual ~PanelMouseWatcherWin();

 private:
  virtual void Start() OVERRIDE;
  virtual void Stop() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;

  static LRESULT CALLBACK MouseHookProc(int code, WPARAM wparam, LPARAM lparam);

  static PanelMouseWatcherWin* instance_;  // singleton instance
  HHOOK mouse_hook_;

  DISALLOW_COPY_AND_ASSIGN(PanelMouseWatcherWin);
};

PanelMouseWatcherWin* PanelMouseWatcherWin::instance_ = NULL;

// static
PanelMouseWatcher* PanelMouseWatcher::Create() {
  return new PanelMouseWatcherWin();
}

PanelMouseWatcherWin::PanelMouseWatcherWin()
    : mouse_hook_(NULL) {
  DCHECK(!instance_);  // Only one instance ever used.
  instance_ = this;
}

PanelMouseWatcherWin::~PanelMouseWatcherWin() {
  DCHECK(!IsActive());
}

void PanelMouseWatcherWin::Start() {
  DCHECK(!IsActive());
  mouse_hook_ = ::SetWindowsHookEx(
      WH_MOUSE_LL, MouseHookProc, GetCurrentModuleHandle(), 0);
  DCHECK(mouse_hook_);
}

void PanelMouseWatcherWin::Stop() {
  DCHECK(IsActive());
  ::UnhookWindowsHookEx(mouse_hook_);
  mouse_hook_ = NULL;
}

bool PanelMouseWatcherWin::IsActive() const {
  return mouse_hook_ != NULL;
}

LRESULT CALLBACK PanelMouseWatcherWin::MouseHookProc(int code,
                                                     WPARAM wparam,
                                                     LPARAM lparam) {
  DCHECK(instance_);
  if (code == HC_ACTION) {
    MOUSEHOOKSTRUCT* hook_struct = reinterpret_cast<MOUSEHOOKSTRUCT*>(lparam);
    if (hook_struct)
      instance_->NotifyMouseMovement(gfx::Point(hook_struct->pt.x,
                                                hook_struct->pt.y));
  }
  return ::CallNextHookEx(NULL, code, wparam, lparam);
}
