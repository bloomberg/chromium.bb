// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_window_cocoa.h"

#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#import "chrome/browser/ui/panels/panel_titlebar_view_cocoa.h"
#import "chrome/browser/ui/panels/panel_window_controller_cocoa.h"
#include "content/common/native_web_keyboard_event.h"

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
    bounds_(bounds),
    is_shown_(false),
    has_find_bar_(false) {
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

  // Browser calls this several times, meaning 'ensure it's shown'.
  // Animations don't look good when repeated - hence this flag is needed.
  if (is_shown_) {
    return;
  }
  is_shown_ = true;

  NSRect finalFrame = ConvertCoordinatesToCocoa(GetPanelBounds());
  [controller_ revealAnimatedWithFrame:finalFrame];
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
  [controller_ setPanelFrame:frame];
}

void PanelBrowserWindowCocoa::OnPanelExpansionStateChanged(
    Panel::ExpansionState expansion_state) {
  NOTIMPLEMENTED();
}

bool PanelBrowserWindowCocoa::ShouldBringUpPanelTitlebar(int mouse_x,
                                                         int mouse_y) const {
  NOTIMPLEMENTED();
  return false;
}

void PanelBrowserWindowCocoa::ClosePanel() {
  if (isClosed())
      return;

  NSWindow* window = [controller_ window];
  [window performClose:controller_];
}

void PanelBrowserWindowCocoa::ActivatePanel() {
  if (!is_shown_)
    return;
  [BrowserWindowUtils activateWindowForController:controller_];
}

void PanelBrowserWindowCocoa::DeactivatePanel() {
  // TODO(dcheng): Implement. See crbug.com/51364 for more details.
  NOTIMPLEMENTED();
}

bool PanelBrowserWindowCocoa::IsPanelActive() const {
  // TODO(dcheng): It seems like a lot of these methods can be called before
  // our NSWindow is created. Do we really need to check in every one of these
  // methods if the NSWindow is created, or is there a better way to
  // gracefully handle this?
  if (!is_shown_)
    return false;
  return [[controller_ window] isMainWindow];
}

gfx::NativeWindow PanelBrowserWindowCocoa::GetNativePanelHandle() {
  return [controller_ window];
}

void PanelBrowserWindowCocoa::UpdatePanelTitleBar() {
  if (!is_shown_)
    return;
  [controller_ updateTitleBar];
}

void PanelBrowserWindowCocoa::ShowTaskManagerForPanel() {
  NOTIMPLEMENTED();
}

FindBar* PanelBrowserWindowCocoa::CreatePanelFindBar() {
  DCHECK(!has_find_bar_) << "find bar should only be created once";
  has_find_bar_ = true;

  FindBarBridge* bridge = new FindBarBridge();
  [controller_ addFindBar:bridge->find_bar_cocoa_controller()];
  return bridge;
}

void PanelBrowserWindowCocoa::NotifyPanelOnUserChangedTheme() {
  NOTIMPLEMENTED();
}

void PanelBrowserWindowCocoa::DrawAttention() {
  NOTIMPLEMENTED();
}

bool PanelBrowserWindowCocoa::IsDrawingAttention() const {
  NOTIMPLEMENTED();
  return false;
}

bool PanelBrowserWindowCocoa::PreHandlePanelKeyboardEvent(
    const NativeWebKeyboardEvent& event, bool* is_keyboard_shortcut) {
  if (![BrowserWindowUtils shouldHandleKeyboardEvent:event])
    return false;

  int id = [BrowserWindowUtils getCommandId:event];
  if (id == -1)
    return false;

  if (browser()->IsReservedCommandOrKey(id, event)) {
      return [BrowserWindowUtils handleKeyboardEvent:event.os_event
                                 inWindow:GetNativePanelHandle()];
  }

  DCHECK(is_keyboard_shortcut);
  *is_keyboard_shortcut = true;
  return false;
}

void PanelBrowserWindowCocoa::HandlePanelKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  if ([BrowserWindowUtils shouldHandleKeyboardEvent:event]) {
    [BrowserWindowUtils handleKeyboardEvent:event.os_event
                                   inWindow:GetNativePanelHandle()];
  }
}

Browser* PanelBrowserWindowCocoa::GetPanelBrowser() const {
  return browser();
}

void PanelBrowserWindowCocoa::DestroyPanelBrowser() {
  [controller_ close];
}

void PanelBrowserWindowCocoa::didCloseNativeWindow() {
  DCHECK(!isClosed());
  panel_->manager()->Remove(panel_.get());
  controller_ = NULL;
}

gfx::Size PanelBrowserWindowCocoa::GetNonClientAreaExtent() const {
  NOTIMPLEMENTED();
  return gfx::Size();
}

int PanelBrowserWindowCocoa::GetRestoredHeight() const {
  NOTIMPLEMENTED();
  return 0;
}

void PanelBrowserWindowCocoa::SetRestoredHeight(int height) {
  NOTIMPLEMENTED();
}

// NativePanelTesting implementation.
class NativePanelTestingCocoa : public NativePanelTesting {
 public:
  NativePanelTestingCocoa(NativePanel* native_panel);
  virtual ~NativePanelTestingCocoa() { }
  // Overridden from NativePanelTesting
  virtual void PressLeftMouseButtonTitlebar(const gfx::Point& point) OVERRIDE;
  virtual void ReleaseMouseButtonTitlebar() OVERRIDE;
  virtual void DragTitlebar(int delta_x, int delta_y) OVERRIDE;
  virtual void CancelDragTitlebar() OVERRIDE;
  virtual void FinishDragTitlebar() OVERRIDE;
 private:
  PanelTitlebarViewCocoa* titlebar();
  // Weak, assumed always to outlive this test API object.
  PanelBrowserWindowCocoa* native_panel_window_;
};

// static
NativePanelTesting* NativePanelTesting::Create(NativePanel* native_panel) {
  return new NativePanelTestingCocoa(native_panel);
}

NativePanelTestingCocoa::NativePanelTestingCocoa(NativePanel* native_panel)
  : native_panel_window_(static_cast<PanelBrowserWindowCocoa*>(native_panel)) {
}

PanelTitlebarViewCocoa* NativePanelTestingCocoa::titlebar() {
  return [native_panel_window_->controller_ titlebarView];
}

void NativePanelTestingCocoa::PressLeftMouseButtonTitlebar(
  const gfx::Point& point) {
  [titlebar() pressLeftMouseButtonTitlebar];
}

void NativePanelTestingCocoa::ReleaseMouseButtonTitlebar() {
  [titlebar() releaseLeftMouseButtonTitlebar];
}

void NativePanelTestingCocoa::DragTitlebar(int delta_x, int delta_y) {
  [titlebar() dragTitlebarDeltaX:delta_x deltaY:delta_y];
}

void NativePanelTestingCocoa::CancelDragTitlebar() {
  [titlebar() cancelDragTitlebar];
}

void NativePanelTestingCocoa::FinishDragTitlebar() {
  [titlebar() finishDragTitlebar];
}

