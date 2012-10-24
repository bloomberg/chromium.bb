// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/host_desktop.h"

#include  "chrome/browser/ui/ash/ash_util.h"
#include  "chrome/browser/ui/browser_list_impl.h"

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
  for (HostDesktopType type = HOST_DESKTOP_TYPE_FIRST;
      type < HOST_DESKTOP_TYPE_COUNT;
      type = static_cast<HostDesktopType>(type + 1)) {
    BrowserListImpl::const_iterator begin =
        BrowserListImpl::GetInstance(type)->begin();
    BrowserListImpl::const_iterator end =
        BrowserListImpl::GetInstance(type)->end();
    if (std::find(begin, end, browser) != end)
      return type;
  }
  return HOST_DESKTOP_TYPE_NATIVE;
}

}  // namespace chrome
