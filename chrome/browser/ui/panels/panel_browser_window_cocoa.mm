// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/panel_browser_window_cocoa.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#import "chrome/browser/ui/cocoa/browser_window_utils.h"
#include "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#include "chrome/browser/ui/cocoa/task_manager_mac.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#import "chrome/browser/ui/panels/panel_titlebar_view_cocoa.h"
#import "chrome/browser/ui/panels/panel_utils_cocoa.h"
#import "chrome/browser/ui/panels/panel_window_controller_cocoa.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/task_manager/task_manager_dialog.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/notification_source.h"

using content::WebContents;

namespace {

// Use this instead of 0 for minimum size of a window when doing opening and
// closing animations, since OSX window manager does not like 0-sized windows
// (according to avi@).
const int kMinimumWindowSize = 1;

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
    has_find_bar_(false),
    attention_request_id_(0),
    activation_requested_by_browser_(false) {
  controller_ = [[PanelWindowControllerCocoa alloc] initWithBrowserWindow:this];
  browser_->tabstrip_model()->AddObserver(this);
  registrar_.Add(
      this,
      chrome::NOTIFICATION_PANEL_CHANGED_EXPANSION_STATE,
      content::Source<Panel>(panel_.get()));
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
  // A call to SetPanelBounds() before showing it is required to set
  // the panel frame properly.
  SetPanelBoundsInstantly(bounds_);
  is_shown_ = true;

  NSRect finalFrame = cocoa_utils::ConvertRectToCocoaCoordinates(bounds_);
  [controller_ revealAnimatedWithFrame:finalFrame];
}

gfx::Rect PanelBrowserWindowCocoa::GetPanelBounds() const {
  return bounds_;
}

// |bounds| is the platform-independent screen coordinates, with (0,0) at
// top-left of the primary screen.
void PanelBrowserWindowCocoa::SetPanelBounds(const gfx::Rect& bounds) {
  setBoundsInternal(bounds, true);
}

void PanelBrowserWindowCocoa::SetPanelBoundsInstantly(const gfx::Rect& bounds) {
  setBoundsInternal(bounds, false);
}

void PanelBrowserWindowCocoa::setBoundsInternal(const gfx::Rect& bounds,
                                                bool animate) {
  // We will call SetPanelBoundsInstantly() once before showing the panel
  // and it should set the panel frame correctly.
  if (bounds_ == bounds && is_shown_)
    return;

  bounds_ = bounds;

  // Safely ignore calls to animate bounds before the panel is shown to
  // prevent the window from loading prematurely.
  if (animate && !is_shown_)
    return;

  NSRect frame = cocoa_utils::ConvertRectToCocoaCoordinates(bounds);
  [controller_ setPanelFrame:frame animate:animate];
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

  AutoReset<bool> pin(&activation_requested_by_browser_, true);
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

void PanelBrowserWindowCocoa::PreventActivationByOS(bool prevent_activation) {
  [controller_ preventBecomingKeyWindow:prevent_activation];
  return;
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
#if defined(WEBUI_TASK_MANAGER)
  TaskManagerDialog::Show();
#else
  // Uses WebUI TaskManager when swiches is set. It is beta feature.
  if (TaskManagerDialog::UseWebUITaskManager()) {
    TaskManagerDialog::Show();
  } else {
    TaskManagerMac::Show(false);
  }
#endif  // defined(WEBUI_TASK_MANAGER)
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

void PanelBrowserWindowCocoa::PanelWebContentsFocused(
    WebContents* contents) {
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

void PanelBrowserWindowCocoa::DrawAttention(bool draw_attention) {
  DCHECK((panel_->attention_mode() & Panel::USE_PANEL_ATTENTION) != 0);

  PanelTitlebarViewCocoa* titlebar = [controller_ titlebarView];
  if (draw_attention)
    [titlebar drawAttention];
  else
    [titlebar stopDrawingAttention];

  if ((panel_->attention_mode() & Panel::USE_SYSTEM_ATTENTION) != 0) {
    if (draw_attention) {
      DCHECK(!attention_request_id_);
      attention_request_id_ =
          [NSApp requestUserAttention:NSInformationalRequest];
    } else {
      [NSApp cancelUserAttentionRequest:attention_request_id_];
      attention_request_id_ = 0;
    }
  }
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

void PanelBrowserWindowCocoa::FullScreenModeChanged(
    bool is_full_screen) {
  [controller_ fullScreenModeChanged:is_full_screen];
}

Browser* PanelBrowserWindowCocoa::GetPanelBrowser() const {
  return browser();
}

void PanelBrowserWindowCocoa::DestroyPanelBrowser() {
  [controller_ close];
}

void PanelBrowserWindowCocoa::EnsurePanelFullyVisible() {
  [controller_ ensureFullyVisible];
}

void PanelBrowserWindowCocoa::SetPanelAlwaysOnTop(bool on_top) {
  [controller_ updateWindowLevel];
}

void PanelBrowserWindowCocoa::EnableResizeByMouse(bool enable) {
  [controller_ enableResizeByMouse:enable];
}

void PanelBrowserWindowCocoa::UpdatePanelMinimizeRestoreButtonVisibility() {
  [controller_ updateTitleBarMinimizeRestoreButtonVisibility];
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
  [controller_ tabInserted:contents->web_contents()];
}

void PanelBrowserWindowCocoa::TabDetachedAt(TabContentsWrapper* contents,
                                            int index) {
  [controller_ tabDetached:contents->web_contents()];
}

void PanelBrowserWindowCocoa::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_PANEL_CHANGED_EXPANSION_STATE, type);
  [controller_ updateWindowLevel];
}

// NativePanelTesting implementation.
class NativePanelTestingCocoa : public NativePanelTesting {
 public:
  NativePanelTestingCocoa(NativePanel* native_panel);
  virtual ~NativePanelTestingCocoa() { }
  // Overridden from NativePanelTesting
  virtual void PressLeftMouseButtonTitlebar(
      const gfx::Point& mouse_location, panel::ClickModifier modifier) OVERRIDE;
  virtual void ReleaseMouseButtonTitlebar(
      panel::ClickModifier modifier) OVERRIDE;
  virtual void DragTitlebar(const gfx::Point& mouse_location) OVERRIDE;
  virtual void CancelDragTitlebar() OVERRIDE;
  virtual void FinishDragTitlebar() OVERRIDE;
  virtual bool VerifyDrawingAttention() const OVERRIDE;
  virtual bool VerifyActiveState(bool is_active) OVERRIDE;
  virtual bool IsWindowSizeKnown() const OVERRIDE;
  virtual bool IsAnimatingBounds() const OVERRIDE;
  virtual bool IsButtonVisible(TitlebarButtonType button_type) const OVERRIDE;

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
    const gfx::Point& mouse_location, panel::ClickModifier modifier) {
  // Convert from platform-indepedent screen coordinates to Cocoa's screen
  // coordinates because PanelTitlebarViewCocoa method takes Cocoa's screen
  // coordinates.
  int modifierFlags =
      (modifier == panel::APPLY_TO_ALL ? NSShiftKeyMask : 0);
  [titlebar() pressLeftMouseButtonTitlebar:
      cocoa_utils::ConvertPointToCocoaCoordinates(mouse_location)
           modifiers:modifierFlags];
}

void NativePanelTestingCocoa::ReleaseMouseButtonTitlebar(
    panel::ClickModifier modifier) {
  int modifierFlags =
      (modifier == panel::APPLY_TO_ALL ? NSShiftKeyMask : 0);
  [titlebar() releaseLeftMouseButtonTitlebar:modifierFlags];
}

void NativePanelTestingCocoa::DragTitlebar(const gfx::Point& mouse_location) {
  // Convert from platform-indepedent screen coordinates to Cocoa's screen
  // coordinates because PanelTitlebarViewCocoa method takes Cocoa's screen
  // coordinates.
  [titlebar() dragTitlebar:
      cocoa_utils::ConvertPointToCocoaCoordinates(mouse_location)];
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

bool NativePanelTestingCocoa::IsButtonVisible(
    TitlebarButtonType button_type) const {
  switch (button_type) {
    case CLOSE_BUTTON:
      return ![[titlebar() closeButton] isHidden];
    case MINIMIZE_BUTTON:
      return ![[titlebar() minimizeButton] isHidden];
    case RESTORE_BUTTON:
      return ![[titlebar() restoreButton] isHidden];
    default:
      NOTREACHED();
  }
  return false;
}
