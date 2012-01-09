// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_aura.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "chrome/browser/ui/panels/panel_browser_frame_view.h"
#include "chrome/browser/ui/panels/panel_browser_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/compact_browser_frame_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#include "chrome/browser/ui/views/frame/popup_non_client_frame_view.h"

namespace browser {

BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame, BrowserView* browser_view) {
  bool create_panel_view = false;
#if defined(OS_CHROMEOS)
  // For R18, panels and popups in Aura ChromeOS use the PanelManager.
  // This logic must match the logic in Browser::CreateBrowserWindow().
  // TODO(stevenjb): Clean this up after R18.
  if (browser_view->IsBrowserTypePanel() || browser_view->IsBrowserTypePopup())
    create_panel_view = true;
#else
  if (browser_view->IsBrowserTypePanel())
    create_panel_view = true;
#endif
  if (create_panel_view) {
    return new PanelBrowserFrameView(
        frame, static_cast<PanelBrowserView*>(browser_view));
  }
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kAuraTranslucentFrames))
    return new BrowserNonClientFrameViewAura(frame, browser_view);

  if (ash::Shell::GetInstance()->IsWindowModeCompact())
    return new CompactBrowserFrameView(frame, browser_view);

  return new OpaqueBrowserFrameView(frame, browser_view);
}

}  // namespace browser
