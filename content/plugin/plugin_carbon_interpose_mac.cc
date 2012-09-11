// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !defined(__LP64__)

#include <Carbon/Carbon.h>

#include "content/plugin/plugin_interpose_util_mac.h"
#include "ui/gfx/rect.h"

#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#if defined(MAC_OS_X_VERSION_10_7) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
// QuickdrawAPI.h is no longer included in the 10.7 SDK, but the symbols are
// still exported by QD.framework (a subframework of ApplicationServices).
// http://developer.apple.com/legacy/mac/library/documentation/Carbon/reference/QuickDraw_Ref/QuickDraw_Ref.pdf
extern "C" {
void SetCursor(const Cursor* crsr);
}
#endif  // 10.7+ SDK

// Returns true if the given window is modal.
static bool IsModalWindow(WindowRef window) {
  WindowModality modality = kWindowModalityNone;
  WindowRef modal_target = NULL;
  OSStatus status = GetWindowModality(window, &modality, &modal_target);
  return (status == noErr) && (modality != kWindowModalityNone);
}

static CGRect CGRectForWindow(WindowRef window) {
  CGRect bounds = { { 0, 0 }, { 0, 0 } };
  HIWindowGetBounds(window, kWindowContentRgn, kHICoordSpace72DPIGlobal,
                    &bounds);
  return bounds;
}

struct WindowInfo {
  uint32 window_id;
  CGRect bounds;
  WindowInfo(WindowRef window) {
    window_id = HIWindowGetCGWindowID(window);
    bounds = CGRectForWindow(window);
  }
};

static void OnPluginWindowClosed(const WindowInfo& window_info) {
  mac_plugin_interposing::NotifyBrowserOfPluginHideWindow(window_info.window_id,
                                                          window_info.bounds);
}

static void OnPluginWindowShown(WindowRef window) {
  mac_plugin_interposing::NotifyBrowserOfPluginShowWindow(
      HIWindowGetCGWindowID(window), CGRectForWindow(window),
      IsModalWindow(window));
}

static void OnPluginWindowSelected(WindowRef window) {
  mac_plugin_interposing::NotifyBrowserOfPluginSelectWindow(
      HIWindowGetCGWindowID(window), CGRectForWindow(window),
      IsModalWindow(window));
}

#pragma mark -

static void ChromePluginSelectWindow(WindowRef window) {
  mac_plugin_interposing::SwitchToPluginProcess();
  SelectWindow(window);
  OnPluginWindowSelected(window);
}

static void ChromePluginShowWindow(WindowRef window) {
  mac_plugin_interposing::SwitchToPluginProcess();
  ShowWindow(window);
  OnPluginWindowShown(window);
}

static void ChromePluginDisposeWindow(WindowRef window) {
  WindowInfo window_info(window);
  DisposeWindow(window);
  OnPluginWindowClosed(window_info);
}

static void ChromePluginHideWindow(WindowRef window) {
  WindowInfo window_info(window);
  HideWindow(window);
  OnPluginWindowClosed(window_info);
}

static void ChromePluginShowHide(WindowRef window, Boolean show) {
  if (show) {
    mac_plugin_interposing::SwitchToPluginProcess();
    ShowHide(window, show);
    OnPluginWindowShown(window);
  } else {
    WindowInfo window_info(window);
    ShowHide(window, show);
    OnPluginWindowClosed(window_info);
  }
}

static void ChromePluginReleaseWindow(WindowRef window) {
  WindowInfo window_info(window);
  ReleaseWindow(window);
  OnPluginWindowClosed(window_info);
}

static void ChromePluginDisposeDialog(DialogRef dialog) {
  WindowRef window = GetDialogWindow(dialog);
  WindowInfo window_info(window);
  DisposeDialog(dialog);
  OnPluginWindowClosed(window_info);
}

static OSStatus ChromePluginSetThemeCursor(ThemeCursor cursor) {
  OpaquePluginRef delegate = mac_plugin_interposing::GetActiveDelegate();
  if (delegate) {
    mac_plugin_interposing::NotifyPluginOfSetThemeCursor(delegate, cursor);
    return noErr;
  }
  return SetThemeCursor(cursor);
}

static void ChromePluginSetCursor(const Cursor* cursor) {
  OpaquePluginRef delegate = mac_plugin_interposing::GetActiveDelegate();
  if (delegate) {
    mac_plugin_interposing::NotifyPluginOfSetCursor(delegate, cursor);
    return;
  }
  return SetCursor(cursor);
}

#pragma mark -

struct interpose_substitution {
  const void* replacement;
  const void* original;
};

#define INTERPOSE_FUNCTION(function) \
    { reinterpret_cast<const void*>(ChromePlugin##function), \
      reinterpret_cast<const void*>(function) }

__attribute__((used)) static const interpose_substitution substitutions[]
    __attribute__((section("__DATA, __interpose"))) = {
  INTERPOSE_FUNCTION(SelectWindow),
  INTERPOSE_FUNCTION(ShowWindow),
  INTERPOSE_FUNCTION(ShowHide),
  INTERPOSE_FUNCTION(DisposeWindow),
  INTERPOSE_FUNCTION(HideWindow),
  INTERPOSE_FUNCTION(ReleaseWindow),
  INTERPOSE_FUNCTION(DisposeDialog),
  INTERPOSE_FUNCTION(SetThemeCursor),
  INTERPOSE_FUNCTION(SetCursor),
};

#endif  // !__LP64__
