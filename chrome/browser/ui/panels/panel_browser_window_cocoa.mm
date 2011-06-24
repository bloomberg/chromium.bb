// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_window_cocoa.h"

#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/panels/panel.h"
#import "chrome/browser/ui/panels/panel_window_controller_cocoa.h"

namespace {

// Use this instead of 0 for minimum size of a window when doing opening and
// closing animations, since OSX window manager does not like 0-sized windows
// (according to avi@).
const int kMinimumWindowSize = 1;

// TODO(dcheng): Move elsewhere so BrowserWindowCocoa can use them too.
NSRect ConvertCoordinatesToCocoa(const gfx::Rect& bounds) {
  // Flip coordinates based on the primary screen.
  NSScreen* screen = [[NSScreen screens] objectAtIndex:0];

  return NSMakeRect(
      bounds.x(), NSHeight([screen frame]) - bounds.height() - bounds.y(),
      bounds.width(), bounds.height());
}

}  // namespace

// This creates a shim window class, which in turn creates a Cocoa window
// controller which in turn creates actual NSWindow by loading a nib.
// Overall chain of ownership is:
// PanelWindowControllerCocoa -> PanelBrowserWindowCocoa -> Panel.
NativePanel* Panel::CreateNativePanel(Browser* browser, Panel* panel,
                                      const gfx::Rect& bounds) {
  return new PanelBrowserWindowCocoa(browser, panel, bounds);
}

PanelBrowserWindowCocoa::PanelBrowserWindowCocoa(Browser* browser,
                                                 Panel* panel,
                                                 const gfx::Rect& bounds)
  : browser_(browser),
    panel_(panel),
    bounds_(bounds) {
  controller_ = [[PanelWindowControllerCocoa alloc] initWithBrowserWindow:this];
}

PanelBrowserWindowCocoa::~PanelBrowserWindowCocoa() {
}

bool PanelBrowserWindowCocoa::isClosed() {
  return !controller_;
}

void PanelBrowserWindowCocoa::ShowPanel() {
  if (isClosed())
    return;

  NSRect finalFrame = ConvertCoordinatesToCocoa(GetPanelBounds());
  NSRect startFrame = NSMakeRect(NSMinX(finalFrame), NSMinY(finalFrame),
                                 NSWidth(finalFrame), kMinimumWindowSize);
  NSWindow* window = [controller_ window];
  // Show the window, using OS-specific animation.
  [window setFrame:startFrame display:NO animate:NO];
  // Shows the window without making it key, on top of its layer, even if
  // Chromium is not an active app.
  [window orderFrontRegardless];
  [window setFrame:finalFrame display:YES animate:YES];
}

void PanelBrowserWindowCocoa::ShowPanelInactive() {
  // TODO(dimich): to be implemented.
  ShowPanel();
}

gfx::Rect PanelBrowserWindowCocoa::GetPanelBounds() const {
  return bounds_;
}

void PanelBrowserWindowCocoa::SetPanelBounds(const gfx::Rect& bounds) {
  bounds_ = bounds;
  NSRect frame = ConvertCoordinatesToCocoa(bounds);
  [[controller_ window] setFrame:frame display:YES animate:YES];
}

void PanelBrowserWindowCocoa::MinimizePanel() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::RestorePanel() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ClosePanel() {
  if (isClosed())
      return;

  NSWindow* window = [controller_ window];
  NSRect frame = [window frame];
  frame.size.height = kMinimumWindowSize;
  [window setFrame:frame display:YES animate:YES];
  browser_->OnWindowClosing();
  DestroyPanelBrowser();  // not immediately, though.
}

void PanelBrowserWindowCocoa::ActivatePanel() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::DeactivatePanel() {
  NOTIMPLEMENTED();
}

bool PanelBrowserWindowCocoa::IsPanelActive() const {
  NOTIMPLEMENTED();
  return false;
}

gfx::NativeWindow PanelBrowserWindowCocoa::GetNativePanelHandle() {
  return [controller_ window];
}

void PanelBrowserWindowCocoa::UpdatePanelTitleBar() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::ShowTaskManagerForPanel() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::NotifyPanelOnUserChangedTheme() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::FlashPanelFrame() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::DestroyPanelBrowser() {
  [controller_ close];
  controller_ = NULL;
}

