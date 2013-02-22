// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/host_desktop.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "ash/shell.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/aura/active_desktop_monitor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"

namespace chrome {

namespace {

bool g_force_ = false;
HostDesktopType g_force_type_ = HOST_DESKTOP_TYPE_COUNT;

}  // namespace

ScopedForceDesktopType::ScopedForceDesktopType(HostDesktopType type)
    : previous_type_(g_force_type_),
      previous_force_(g_force_) {
  g_force_type_ = type;
  g_force_ = true;
}

ScopedForceDesktopType::~ScopedForceDesktopType() {
  g_force_type_ = previous_type_;
  g_force_ = previous_force_;
}

HostDesktopType GetHostDesktopTypeForNativeView(gfx::NativeView native_view) {
  if (g_force_)
    return g_force_type_;
#if defined(USE_ASH)
  // TODO(ananta)
  // Once we've threaded creation context to wherever needed, we should remove
  // this check here.
#if defined(OS_WIN)
  if (!native_view)
    return GetActiveDesktop();
#endif
  return IsNativeViewInAsh(native_view) ?
      HOST_DESKTOP_TYPE_ASH :
      HOST_DESKTOP_TYPE_NATIVE;
#else
  return HOST_DESKTOP_TYPE_NATIVE;
#endif
}

HostDesktopType GetHostDesktopTypeForNativeWindow(
    gfx::NativeWindow native_window) {
  if (g_force_)
    return g_force_type_;
#if defined(USE_ASH)
  // TODO(ananta)
  // Once we've threaded creation context to wherever needed, we should remove
  // this check here.
#if defined(OS_WIN)
  if (!native_window)
    return GetActiveDesktop();
#endif
  return IsNativeWindowInAsh(native_window) ?
      HOST_DESKTOP_TYPE_ASH :
      HOST_DESKTOP_TYPE_NATIVE;
#else
  return HOST_DESKTOP_TYPE_NATIVE;
#endif
}

HostDesktopType GetHostDesktopTypeForBrowser(const Browser* browser) {
  if (g_force_)
    return g_force_type_;
  return browser->host_desktop_type();
}

HostDesktopType GetActiveDesktop() {
#if defined(USE_ASH) && !defined(OS_CHROMEOS)
  // The Ash desktop is considered active if a non-desktop RootWindow was last
  // activated and the Ash desktop is still open.  As it is, the Ash desktop
  // will be considered the last active if a user switches from metro Chrome to
  // the Windows desktop but doesn't activate any Chrome windows there (e.g.,
  // by clicking on one or otherwise giving one focus).  Consider finding a way
  // to detect that the Windows desktop has been activated so that the native
  // desktop can be considered active once the user switches to it if its
  // BrowserList isn't empty.
  if ((ActiveDesktopMonitor::GetLastActivatedDesktopType() ==
       chrome::HOST_DESKTOP_TYPE_ASH) &&
      ash::Shell::HasInstance()) {
    return HOST_DESKTOP_TYPE_ASH;
  }
#endif
  return HOST_DESKTOP_TYPE_NATIVE;
}

}  // namespace chrome
