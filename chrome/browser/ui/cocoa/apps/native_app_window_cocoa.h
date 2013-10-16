// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPS_NATIVE_APP_WINDOW_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_APPS_NATIVE_APP_WINDOW_COCOA_H_

#import <Cocoa/Cocoa.h>
#include <vector>

#include "apps/shell_window.h"
#include "apps/ui/native_app_window.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/browser_command_executor.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/common/draggable_region.h"
#include "ui/gfx/rect.h"

class ExtensionKeybindingRegistryCocoa;
class Profile;
class NativeAppWindowCocoa;
@class ShellNSWindow;
class SkRegion;

// A window controller for a minimal window to host a web app view. Passes
// Objective-C notifications to the C++ bridge.
@interface NativeAppWindowController : NSWindowController
                                      <NSWindowDelegate,
                                       BrowserCommandExecutor> {
 @private
  NativeAppWindowCocoa* appWindow_;  // Weak; owns self.
}

@property(assign, nonatomic) NativeAppWindowCocoa* appWindow;

// Consults the Command Registry to see if this |event| needs to be handled as
// an extension command and returns YES if so (NO otherwise).
- (BOOL)handledByExtensionCommand:(NSEvent*)event;

@end

// Cocoa bridge to AppWindow.
class NativeAppWindowCocoa : public apps::NativeAppWindow,
                             public content::WebContentsObserver {
 public:
  NativeAppWindowCocoa(apps::ShellWindow* shell_window,
                       const apps::ShellWindow::CreateParams& params);

  // ui::BaseWindow implementation.
  virtual bool IsActive() const OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual ui::WindowShowState GetRestoredState() const OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void ShowInactive() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void FlashFrame(bool flash) OVERRIDE;
  virtual bool IsAlwaysOnTop() const OVERRIDE;

  // Called when the window is about to be closed.
  void WindowWillClose();

  // Called when the window is focused.
  void WindowDidBecomeKey();

  // Called when the window is defocused.
  void WindowDidResignKey();

  // Called when the window finishes resizing, i.e. after zoom/unzoom, after
  // entering/leaving fullscreen, and after a user is done resizing.
  void WindowDidFinishResize();

  // Called when the window is resized. This is called repeatedly during a
  // zoom/unzoom, and while a user is resizing.
  void WindowDidResize();

  // Called when the window is moved.
  void WindowDidMove();

  // Called when the window is minimized.
  void WindowDidMiniaturize();

  // Called when the window is un-minimized.
  void WindowDidDeminiaturize();

  // Called when the window is zoomed (maximized or de-maximized).
  void WindowWillZoom();

  // Called to handle a key event.
  bool HandledByExtensionCommand(NSEvent* event);

  // Called to handle a mouse event.
  void HandleMouseEvent(NSEvent* event);

  // Returns true if |point| in local Cocoa coordinate system falls within
  // the draggable region.
  bool IsWithinDraggableRegion(NSPoint point) const;

  NSRect restored_bounds() const { return restored_bounds_; }
  bool use_system_drag() const { return use_system_drag_; }

 protected:
  // NativeAppWindow implementation.
  virtual void SetFullscreen(bool fullscreen) OVERRIDE;
  virtual bool IsFullscreenOrPending() const OVERRIDE;
  virtual bool IsDetached() const OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;
  virtual void UpdateWindowTitle() OVERRIDE;
  virtual void UpdateInputRegion(scoped_ptr<SkRegion> region) OVERRIDE;
  virtual void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions) OVERRIDE;
  virtual SkRegion* GetDraggableRegion() OVERRIDE;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual bool IsFrameless() const OVERRIDE;
  virtual gfx::Insets GetFrameInsets() const OVERRIDE;
  virtual bool IsVisible() const OVERRIDE;

  // WebContentsObserver implementation.
  virtual void RenderViewHostChanged(
      content::RenderViewHost* old_host,
      content::RenderViewHost* new_host) OVERRIDE;

  // These are used to simulate Mac-style hide/show. Since windows can be hidden
  // and shown using the app.window API, this sets is_hidden_with_app_ to
  // differentiate the reason a window was hidden.
  virtual void ShowWithApp() OVERRIDE;
  virtual void HideWithApp() OVERRIDE;

  virtual void SetAlwaysOnTop(bool always_on_top) OVERRIDE;

  // WebContentsModalDialogHost implementation.
  virtual gfx::NativeView GetHostView() const OVERRIDE;
  virtual gfx::Point GetDialogPosition(const gfx::Size& size) OVERRIDE;
  virtual gfx::Size GetMaximumDialogSize() OVERRIDE;
  virtual void AddObserver(
      web_modal::ModalDialogHostObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      web_modal::ModalDialogHostObserver* observer) OVERRIDE;

 private:
  virtual ~NativeAppWindowCocoa();

  ShellNSWindow* window() const;

  content::WebContents* web_contents() const {
    return shell_window_->web_contents();
  }
  const extensions::Extension* extension() const {
    return shell_window_->extension();
  }

  // Returns the WindowStyleMask based on the type of window frame.
  // Specifically, this includes NSResizableWindowMask if the window is
  // resizable, and does not include NSTexturedBackgroundWindowMask when a
  // native frame is used.
  NSUInteger GetWindowStyleMask() const;

  void InstallView();
  void UninstallView();
  void InstallDraggableRegionViews();
  void UpdateDraggableRegionsForSystemDrag(
      const std::vector<extensions::DraggableRegion>& regions,
      const extensions::DraggableRegion* draggable_area);
  void UpdateDraggableRegionsForCustomDrag(
      const std::vector<extensions::DraggableRegion>& regions);

  // Cache |restored_bounds_| only if the window is currently restored.
  void UpdateRestoredBounds();

  // Hides the window unconditionally. Used by Hide and HideWithApp.
  void HideWithoutMarkingHidden();

  apps::ShellWindow* shell_window_; // weak - ShellWindow owns NativeAppWindow.

  bool has_frame_;

  // Whether this window is hidden according to the app.window API. This is set
  // by Hide, Show, and ShowInactive.
  bool is_hidden_;
  // Whether this window last became hidden due to a request to hide the entire
  // app, e.g. via the dock menu or Cmd+H. This is set by Hide/ShowWithApp.
  bool is_hidden_with_app_;

  bool is_maximized_;
  bool is_fullscreen_;
  NSRect restored_bounds_;

  gfx::Size min_size_;
  gfx::Size max_size_;
  bool shows_resize_controls_;
  bool shows_fullscreen_controls_;

  base::scoped_nsobject<NativeAppWindowController> window_controller_;
  NSInteger attention_request_id_;  // identifier from requestUserAttention

  // Indicates whether system drag or custom drag should be used, depending on
  // the complexity of draggable regions.
  bool use_system_drag_;

  // For system drag, the whole window is draggable and the non-draggable areas
  // have to been explicitly excluded.
  std::vector<gfx::Rect> system_drag_exclude_areas_;

  // For custom drag, the whole window is non-draggable and the draggable region
  // has to been explicitly provided.
  scoped_ptr<SkRegion> draggable_region_;  // used in custom drag.

  // Mouse location since the last mouse event, in screen coordinates. This is
  // used in custom drag to compute the window movement.
  NSPoint last_mouse_location_;

  // The Extension Command Registry used to determine which keyboard events to
  // handle.
  scoped_ptr<ExtensionKeybindingRegistryCocoa> extension_keybinding_registry_;

  DISALLOW_COPY_AND_ASSIGN(NativeAppWindowCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_APPS_NATIVE_APP_WINDOW_COCOA_H_
