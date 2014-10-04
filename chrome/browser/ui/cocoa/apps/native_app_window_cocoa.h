// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPS_NATIVE_APP_WINDOW_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_APPS_NATIVE_APP_WINDOW_COCOA_H_

#import <Cocoa/Cocoa.h>
#include <vector>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/browser_command_executor.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/app_window/size_constraints.h"
#include "extensions/common/draggable_region.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/gfx/rect.h"

class ExtensionKeybindingRegistryCocoa;
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
// Only extensions with the given |priority| are considered.
- (BOOL)handledByExtensionCommand:(NSEvent*)event
    priority:(ui::AcceleratorManager::HandlerPriority)priority;

@end

// Cocoa bridge to AppWindow.
class NativeAppWindowCocoa : public extensions::NativeAppWindow,
                             public content::WebContentsObserver {
 public:
  NativeAppWindowCocoa(extensions::AppWindow* app_window,
                       const extensions::AppWindow::CreateParams& params);

  // ui::BaseWindow implementation.
  virtual bool IsActive() const override;
  virtual bool IsMaximized() const override;
  virtual bool IsMinimized() const override;
  virtual bool IsFullscreen() const override;
  virtual gfx::NativeWindow GetNativeWindow() override;
  virtual gfx::Rect GetRestoredBounds() const override;
  virtual ui::WindowShowState GetRestoredState() const override;
  virtual gfx::Rect GetBounds() const override;
  virtual void Show() override;
  virtual void ShowInactive() override;
  virtual void Hide() override;
  virtual void Close() override;
  virtual void Activate() override;
  virtual void Deactivate() override;
  virtual void Maximize() override;
  virtual void Minimize() override;
  virtual void Restore() override;
  virtual void SetBounds(const gfx::Rect& bounds) override;
  virtual void FlashFrame(bool flash) override;
  virtual bool IsAlwaysOnTop() const override;

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

  // Called when the window enters fullscreen.
  void WindowDidEnterFullscreen();

  // Called when the window exits fullscreen.
  void WindowDidExitFullscreen();

  // Called to handle a key event.
  bool HandledByExtensionCommand(
      NSEvent* event,
      ui::AcceleratorManager::HandlerPriority priority);

  // Returns true if |point| in local Cocoa coordinate system falls within
  // the draggable region.
  bool IsWithinDraggableRegion(NSPoint point) const;

  NSRect restored_bounds() const { return restored_bounds_; }

 protected:
  // NativeAppWindow implementation.
  virtual void SetFullscreen(int fullscreen_types) override;
  virtual bool IsFullscreenOrPending() const override;
  virtual void UpdateWindowIcon() override;
  virtual void UpdateWindowTitle() override;
  virtual void UpdateBadgeIcon() override;
  virtual void UpdateShape(scoped_ptr<SkRegion> region) override;
  virtual void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions) override;
  virtual SkRegion* GetDraggableRegion() override;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  virtual bool IsFrameless() const override;
  virtual bool HasFrameColor() const override;
  virtual SkColor ActiveFrameColor() const override;
  virtual SkColor InactiveFrameColor() const override;
  virtual gfx::Insets GetFrameInsets() const override;
  virtual bool CanHaveAlphaEnabled() const override;

  // These are used to simulate Mac-style hide/show. Since windows can be hidden
  // and shown using the app.window API, this sets is_hidden_with_app_ to
  // differentiate the reason a window was hidden.
  virtual void ShowWithApp() override;
  virtual void HideWithApp() override;
  virtual void UpdateShelfMenu() override;
  virtual gfx::Size GetContentMinimumSize() const override;
  virtual gfx::Size GetContentMaximumSize() const override;
  virtual void SetContentSizeConstraints(const gfx::Size& min_size,
                                         const gfx::Size& max_size) override;
  virtual void SetVisibleOnAllWorkspaces(bool always_visible) override;

  // WebContentsObserver implementation.
  virtual void RenderViewCreated(content::RenderViewHost* rvh) override;

  virtual void SetAlwaysOnTop(bool always_on_top) override;

  // WebContentsModalDialogHost implementation.
  virtual gfx::NativeView GetHostView() const override;
  virtual gfx::Point GetDialogPosition(const gfx::Size& size) override;
  virtual gfx::Size GetMaximumDialogSize() override;
  virtual void AddObserver(
      web_modal::ModalDialogHostObserver* observer) override;
  virtual void RemoveObserver(
      web_modal::ModalDialogHostObserver* observer) override;

 private:
  virtual ~NativeAppWindowCocoa();

  ShellNSWindow* window() const;
  content::WebContents* WebContents() const;

  // Returns the WindowStyleMask based on the type of window frame.
  // This includes NSResizableWindowMask if the window is resizable.
  NSUInteger GetWindowStyleMask() const;

  void InstallView();
  void UninstallView();
  void UpdateDraggableRegionViews();

  // Cache |restored_bounds_| only if the window is currently restored.
  void UpdateRestoredBounds();

  // Hides the window unconditionally. Used by Hide and HideWithApp.
  void HideWithoutMarkingHidden();

  extensions::AppWindow* app_window_;  // weak - AppWindow owns NativeAppWindow.

  bool has_frame_;

  // Whether this window last became hidden due to a request to hide the entire
  // app, e.g. via the dock menu or Cmd+H. This is set by Hide/ShowWithApp.
  bool is_hidden_with_app_;

  bool is_maximized_;
  bool is_fullscreen_;
  NSRect restored_bounds_;

  bool is_resizable_;
  bool shows_resize_controls_;
  bool shows_fullscreen_controls_;

  extensions::SizeConstraints size_constraints_;

  bool has_frame_color_;
  SkColor active_frame_color_;
  SkColor inactive_frame_color_;

  base::scoped_nsobject<NativeAppWindowController> window_controller_;

  // For system drag, the whole window is draggable and the non-draggable areas
  // have to been explicitly excluded.
  std::vector<extensions::DraggableRegion> draggable_regions_;

  // The Extension Command Registry used to determine which keyboard events to
  // handle.
  scoped_ptr<ExtensionKeybindingRegistryCocoa> extension_keybinding_registry_;

  DISALLOW_COPY_AND_ASSIGN(NativeAppWindowCocoa);
};

#endif  // CHROME_BROWSER_UI_COCOA_APPS_NATIVE_APP_WINDOW_COCOA_H_
