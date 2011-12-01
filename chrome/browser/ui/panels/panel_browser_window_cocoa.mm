// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_window_cocoa.h"

#include "base/logging.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#import "chrome/browser/ui/panels/panel_titlebar_view_cocoa.h"
#import "chrome/browser/ui/panels/panel_window_controller_cocoa.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/public/browser/native_web_keyboard_event.h"

namespace {

// Use this instead of 0 for minimum size of a window when doing opening and
// closing animations, since OSX window manager does not like 0-sized windows
// (according to avi@).
const int kMinimumWindowSize = 1;

// TODO(dcheng): Move elsewhere so BrowserWindowCocoa can use them too.
// Converts global screen coordinates in platfrom-independent coordinates
// (with the (0,0) in the top-left corner of the primary screen) to the Cocoa
// screen coordinates (with (0,0) in the low-left corner).
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
  browser_->tabstrip_model()->AddObserver(this);
}

PanelBrowserWindowCocoa::~PanelBrowserWindowCocoa() {
  browser_->tabstrip_model()->RemoveObserver(this);
}

bool PanelBrowserWindowCocoa::isClosed() {
  return !controller_;
}

void PanelBrowserWindowCocoa::ShowPanel() {
  // The Browser associated with this browser window must become the active
  // browser at the time |Show()| is called. This is the natural behaviour under
  // Windows, but |-makeKeyAndOrderFront:| won't send |-windowDidBecomeKey:|
  // until we return to the runloop. Therefore any calls to
  // |BrowserList::GetLastActive()| (for example, in bookmark_util), will return
  // the previous browser instead if we don't explicitly set it here.
  BrowserList::SetLastActive(browser());

  ShowPanelInactive();
  ActivatePanel();
}

void PanelBrowserWindowCocoa::ShowPanelInactive() {
  if (isClosed())
    return;

  // Browser calls this several times, meaning 'ensure it's shown'.
  // Animations don't look good when repeated - hence this flag is needed.
  if (is_shown_) {
    return;
  }
  is_shown_ = true;

  NSRect finalFrame = ConvertCoordinatesToCocoa(bounds_);
  [controller_ revealAnimatedWithFrame:finalFrame];
}

gfx::Rect PanelBrowserWindowCocoa::GetPanelBounds() const {
  return bounds_;
}

// |bounds| is the platform-independent screen coordinates, with (0,0) at
// top-left of the primary screen.
void PanelBrowserWindowCocoa::SetPanelBounds(const gfx::Rect& bounds) {
  if (bounds_ == bounds)
    return;

  bounds_ = bounds;

  NSRect frame = ConvertCoordinatesToCocoa(bounds);
  [controller_ setPanelFrame:frame];
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
  [controller_ deactivate];
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

void PanelBrowserWindowCocoa::UpdatePanelLoadingAnimations(
    bool should_animate) {
  [controller_ updateThrobber:should_animate];
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

void PanelBrowserWindowCocoa::PanelTabContentsFocused(
    TabContents* tab_contents) {
  // TODO(jianli): to be implemented.
}

void PanelBrowserWindowCocoa::PanelCut() {
  // Nothing to do since we do not have panel-specific system menu on Mac.
}

void PanelBrowserWindowCocoa::PanelCopy() {
  // Nothing to do since we do not have panel-specific system menu on Mac.
}

void PanelBrowserWindowCocoa::PanelPaste() {
  // Nothing to do since we do not have panel-specific system menu on Mac.
}

// TODO(dimich) the code here looks very platform-independent. Move it to Panel.
void PanelBrowserWindowCocoa::DrawAttention() {
  // Don't draw attention for active panel.
  if ([[controller_ window] isMainWindow])
    return;

  if (IsDrawingAttention())
    return;

  // Bring up the titlebar if minimized so the user can see it.
  if (panel_->expansion_state() == Panel::MINIMIZED)
    panel_->SetExpansionState(Panel::TITLE_ONLY);

  PanelTitlebarViewCocoa* titlebar = [controller_ titlebarView];
  [titlebar drawAttention];
}

bool PanelBrowserWindowCocoa::IsDrawingAttention() const {
  PanelTitlebarViewCocoa* titlebar = [controller_ titlebarView];
  return [titlebar isDrawingAttention];
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

gfx::Size PanelBrowserWindowCocoa::IconOnlySize() const {
  // TODO(dimich): to be implemented.
  return gfx::Size();
}

void PanelBrowserWindowCocoa::EnsurePanelFullyVisible() {
  // TODO(dimich): to be implemented.
}

void PanelBrowserWindowCocoa::DidCloseNativeWindow() {
  DCHECK(!isClosed());
  panel_->OnNativePanelClosed();
  controller_ = NULL;
}

gfx::Size PanelBrowserWindowCocoa::WindowSizeFromContentSize(
    const gfx::Size& content_size) const {
  NSWindow* window = [controller_ window];
  NSRect content = NSMakeRect(0, 0,
                              content_size.width(), content_size.height());
  NSRect frame = [window frameRectForContentRect:content];
  return gfx::Size(NSWidth(frame), NSHeight(frame));
}

gfx::Size PanelBrowserWindowCocoa::ContentSizeFromWindowSize(
    const gfx::Size& window_size) const {
  NSWindow* window = [controller_ window];
  NSRect frame = NSMakeRect(0, 0, window_size.width(), window_size.height());
  NSRect content = [window contentRectForFrameRect:frame];
  return gfx::Size(NSWidth(content), NSHeight(content));
}

int PanelBrowserWindowCocoa::TitleOnlyHeight() const {
  return [controller_ titlebarHeightInScreenCoordinates];
}

void PanelBrowserWindowCocoa::TabInsertedAt(TabContentsWrapper* contents,
                                            int index,
                                            bool foreground) {
  [controller_ tabInserted:contents->tab_contents()];
}

void PanelBrowserWindowCocoa::TabDetachedAt(TabContentsWrapper* contents,
                                            int index) {
  [controller_ tabDetached:contents->tab_contents()];
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
  virtual bool VerifyDrawingAttention() const OVERRIDE;
  virtual bool VerifyActiveState(bool is_active) OVERRIDE;
  virtual bool IsWindowSizeKnown() const OVERRIDE;
  virtual bool IsAnimatingBounds() const OVERRIDE;

 private:
  PanelTitlebarViewCocoa* titlebar() const;
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

PanelTitlebarViewCocoa* NativePanelTestingCocoa::titlebar() const {
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

bool NativePanelTestingCocoa::VerifyDrawingAttention() const {
  return [titlebar() isDrawingAttention];
}

bool NativePanelTestingCocoa::VerifyActiveState(bool is_active) {
  // TODO(jianli): to be implemented.
  return false;
}

bool NativePanelTestingCocoa::IsWindowSizeKnown() const {
  return true;
}

bool NativePanelTestingCocoa::IsAnimatingBounds() const {
  return [native_panel_window_->controller_ isAnimatingBounds];
}
