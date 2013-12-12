// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_NATIVE_APP_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_NATIVE_APP_WINDOW_VIEWS_H_

#include "apps/shell_window.h"
#include "apps/ui/native_app_window.h"
#include "base/observer_list.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/rect.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

#if defined(OS_WIN)
#include "chrome/browser/shell_integration.h"
#endif

#if defined(USE_ASH)
namespace ash {
class ImmersiveFullscreenController;
}
#endif

class ExtensionKeybindingRegistryViews;
class Profile;

namespace apps {
class ShellWindowFrameView;
}

namespace content {
class RenderViewHost;
class WebContents;
}

namespace extensions {
class Extension;
}

namespace ui {
class MenuModel;
}

namespace views {
class MenuRunner;
class WebView;
}

class NativeAppWindowViews : public apps::NativeAppWindow,
                             public content::WebContentsObserver,
                             public views::ContextMenuController,
                             public views::WidgetDelegateView,
                             public views::WidgetObserver {
 public:
  NativeAppWindowViews();
  virtual ~NativeAppWindowViews();
  void Init(apps::ShellWindow* shell_window,
            const apps::ShellWindow::CreateParams& create_params);

 protected:
  // Called before views::Widget::Init() to allow subclasses to customize
  // the InitParams that would be passed.
  virtual void OnBeforeWidgetInit(views::Widget::InitParams* init_params,
                                  views::Widget* widget);

  // ui::BaseWindow implementation that subclasses may override.
  virtual void Show() OVERRIDE;
  virtual void Activate() OVERRIDE;

  Profile* profile() { return shell_window_->profile(); }
  const extensions::Extension* extension() {
    return shell_window_->extension();
  }

 private:
  void InitializeDefaultWindow(
      const apps::ShellWindow::CreateParams& create_params);
  void InitializePanelWindow(
      const apps::ShellWindow::CreateParams& create_params);
  void OnViewWasResized();

  bool ShouldUseChromeStyleFrame() const;

  // Caller owns the returned object.
  apps::ShellWindowFrameView* CreateShellWindowFrameView();

#if defined(OS_WIN)
  void OnShortcutInfoLoaded(
      const ShellIntegration::ShortcutInfo& shortcut_info);
  HWND GetNativeAppWindowHWND() const;
#endif

  // ui::BaseWindow implementation.
  virtual bool IsActive() const OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual ui::WindowShowState GetRestoredState() const OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void ShowInactive() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void FlashFrame(bool flash) OVERRIDE;
  virtual bool IsAlwaysOnTop() const OVERRIDE;
  virtual void SetAlwaysOnTop(bool always_on_top) OVERRIDE;

  // Overridden from views::ContextMenuController:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& p,
                                      ui::MenuSourceType source_type) OVERRIDE;

  // WidgetDelegate implementation.
  virtual void OnWidgetMove() OVERRIDE;
  virtual views::View* GetInitiallyFocusedView() OVERRIDE;
  virtual bool CanResize() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual bool ShouldShowWindowTitle() const OVERRIDE;
  virtual gfx::ImageSkia GetWindowAppIcon() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual bool ShouldShowWindowIcon() const OVERRIDE;
  virtual void SaveWindowPlacement(const gfx::Rect& bounds,
                                   ui::WindowShowState show_state) OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;
  virtual views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) OVERRIDE;
  virtual bool WidgetHasHitTestMask() const OVERRIDE;
  virtual void GetWidgetHitTestMask(gfx::Path* mask) const OVERRIDE;
  virtual bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location) OVERRIDE;

  // WidgetObserver implementation.
  virtual void OnWidgetVisibilityChanged(views::Widget* widget,
                                         bool visible) OVERRIDE;
  virtual void OnWidgetActivationChanged(views::Widget* widget,
                                         bool active) OVERRIDE;

  // WebContentsObserver implementation.
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void RenderViewHostChanged(
      content::RenderViewHost* old_host,
      content::RenderViewHost* new_host) OVERRIDE;

  // views::View implementation.
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual gfx::Size GetMinimumSize() OVERRIDE;
  virtual gfx::Size GetMaximumSize() OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual bool AcceleratorPressed(const ui::Accelerator& accelerator) OVERRIDE;

  // NativeAppWindow implementation.
  virtual void SetFullscreen(int fullscreen_types) OVERRIDE;
  virtual bool IsFullscreenOrPending() const OVERRIDE;
  virtual bool IsDetached() const OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;
  virtual void UpdateWindowTitle() OVERRIDE;
  virtual void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions) OVERRIDE;
  virtual SkRegion* GetDraggableRegion() OVERRIDE;
  virtual void UpdateShape(scoped_ptr<SkRegion> region) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual bool IsFrameless() const OVERRIDE;
  virtual gfx::Insets GetFrameInsets() const OVERRIDE;
  virtual void HideWithApp() OVERRIDE;
  virtual void ShowWithApp() OVERRIDE;
  virtual void UpdateWindowMinMaxSize() OVERRIDE;

  // web_modal::WebContentsModalDialogHost implementation.
  virtual gfx::NativeView GetHostView() const OVERRIDE;
  virtual gfx::Point GetDialogPosition(const gfx::Size& size) OVERRIDE;
  virtual gfx::Size GetMaximumDialogSize() OVERRIDE;
  virtual void AddObserver(
      web_modal::ModalDialogHostObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      web_modal::ModalDialogHostObserver* observer) OVERRIDE;

  content::WebContents* web_contents() {
    return shell_window_->web_contents();
  }

  apps::ShellWindow* shell_window_; // weak - ShellWindow owns NativeAppWindow.
  views::WebView* web_view_;
  views::Widget* window_;
  bool is_fullscreen_;

  // Custom shape of the window. If this is not set then the window has a
  // default shape, usually rectangular.
  scoped_ptr<SkRegion> shape_;

  scoped_ptr<SkRegion> draggable_region_;

  bool frameless_;
  bool transparent_background_;
  gfx::Size preferred_size_;
  bool resizable_;

  // The class that registers for keyboard shortcuts for extension commands.
  scoped_ptr<ExtensionKeybindingRegistryViews> extension_keybinding_registry_;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

#if defined(USE_ASH)
  // Used to put non-frameless windows into immersive fullscreen on ChromeOS. In
  // immersive fullscreen, the window header (title bar and window controls)
  // slides onscreen as an overlay when the mouse is hovered at the top of the
  // screen.
  scoped_ptr<ash::ImmersiveFullscreenController>
      immersive_fullscreen_controller_;
#endif

  ObserverList<web_modal::ModalDialogHostObserver> observer_list_;

  base::WeakPtrFactory<NativeAppWindowViews> weak_ptr_factory_;

  // Used to show the system menu.
  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(NativeAppWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_NATIVE_APP_WINDOW_VIEWS_H_
