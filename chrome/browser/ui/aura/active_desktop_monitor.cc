// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/aura/active_desktop_monitor.h"

#include "ui/aura/env.h"
#include "ui/aura/root_window.h"

#if defined(USE_X11)
#include "ui/views/widget/desktop_aura/desktop_root_window_host_x11.h"
#elif defined(OS_WIN)
#include "ui/views/widget/desktop_aura/desktop_root_window_host_win.h"
#endif

// static
ActiveDesktopMonitor* ActiveDesktopMonitor::g_instance_ = NULL;

ActiveDesktopMonitor::ActiveDesktopMonitor()
    : last_activated_desktop_(chrome::HOST_DESKTOP_TYPE_NATIVE) {
  DCHECK(!g_instance_);
  g_instance_ = this;
  aura::Env::GetInstance()->AddObserver(this);
}

ActiveDesktopMonitor::~ActiveDesktopMonitor() {
  DCHECK_EQ(g_instance_, this);
  aura::Env::GetInstance()->RemoveObserver(this);
  g_instance_ = NULL;
}

// static
chrome::HostDesktopType ActiveDesktopMonitor::GetLastActivatedDesktopType() {
  // Tests may not have created the monitor.
  return g_instance_ ? g_instance_->last_activated_desktop_ :
      chrome::HOST_DESKTOP_TYPE_NATIVE;
}

// static
bool ActiveDesktopMonitor::IsDesktopWindow(aura::RootWindow* root_window) {
  // Only windows hosted by a DesktopRootWindowHost implementation can be mapped
  // back to a content Window. All others, therefore, must be the root window
  // for an Ash display.
#if defined(OS_WIN)
  return views::DesktopRootWindowHostWin::GetContentWindowForHWND(
      root_window->GetAcceleratedWidget()) != NULL;
#elif defined(OS_LINUX)
  return views::DesktopRootWindowHostX11::GetContentWindowForXID(
      root_window->GetAcceleratedWidget()) != NULL;
#else
  NOTREACHED();
  return true;
#endif
}

void ActiveDesktopMonitor::OnWindowInitialized(aura::Window* window) {}

void ActiveDesktopMonitor::OnRootWindowActivated(
    aura::RootWindow* root_window) {
  if (IsDesktopWindow(root_window))
    last_activated_desktop_ = chrome::HOST_DESKTOP_TYPE_NATIVE;
  else
    last_activated_desktop_ = chrome::HOST_DESKTOP_TYPE_ASH;
  DVLOG(1) << __FUNCTION__
           << (last_activated_desktop_ == chrome::HOST_DESKTOP_TYPE_NATIVE ?
               " native" : " ash") << " desktop activated.";
}
