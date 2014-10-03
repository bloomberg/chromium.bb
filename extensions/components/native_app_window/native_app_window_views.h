// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMPONENTS_NATIVE_APP_WINDOW_NATIVE_APP_WINDOW_VIEWS_H_
#define EXTENSIONS_COMPONENTS_NATIVE_APP_WINDOW_NATIVE_APP_WINDOW_VIEWS_H_

#include "base/observer_list.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/browser/app_window/size_constraints.h"
#include "ui/gfx/rect.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

class SkRegion;

namespace content {
class BrowserContext;
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

namespace native_app_window {

// A NativeAppWindow backed by a views::Widget. This class may be used alone
// as a stub or subclassed (for example, ChromeNativeAppWindowViews).
class NativeAppWindowViews : public extensions::NativeAppWindow,
                             public content::WebContentsObserver,
                             public views::WidgetDelegateView,
                             public views::WidgetObserver {
 public:
  NativeAppWindowViews();
  virtual ~NativeAppWindowViews();
  void Init(extensions::AppWindow* app_window,
            const extensions::AppWindow::CreateParams& create_params);

  // Signal that CanHaveTransparentBackground has changed.
  void OnCanHaveAlphaEnabledChanged();

  views::Widget* widget() { return widget_; }

  void set_window_for_testing(views::Widget* window) { widget_ = window; }
  void set_web_view_for_testing(views::WebView* view) { web_view_ = view; }

 protected:
  extensions::AppWindow* app_window() { return app_window_; }
  const extensions::AppWindow* app_window() const { return app_window_; }

  const views::Widget* widget() const { return widget_; }

  views::WebView* web_view() { return web_view_; }

  // Initializes |widget_| for |app_window|.
  virtual void InitializeWindow(
      extensions::AppWindow* app_window,
      const extensions::AppWindow::CreateParams& create_params);

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
  virtual void SetAlwaysOnTop(bool always_on_top) override;

  // WidgetDelegate implementation.
  virtual void OnWidgetMove() override;
  virtual views::View* GetInitiallyFocusedView() override;
  virtual bool CanResize() const override;
  virtual bool CanMaximize() const override;
  virtual bool CanMinimize() const override;
  virtual base::string16 GetWindowTitle() const override;
  virtual bool ShouldShowWindowTitle() const override;
  virtual bool ShouldShowWindowIcon() const override;
  virtual void SaveWindowPlacement(const gfx::Rect& bounds,
                                   ui::WindowShowState show_state) override;
  virtual void DeleteDelegate() override;
  virtual views::Widget* GetWidget() override;
  virtual const views::Widget* GetWidget() const override;
  virtual views::View* GetContentsView() override;
  virtual bool ShouldDescendIntoChildForEventHandling(
      gfx::NativeView child,
      const gfx::Point& location) override;

  // WidgetObserver implementation.
  virtual void OnWidgetVisibilityChanged(views::Widget* widget,
                                         bool visible) override;
  virtual void OnWidgetActivationChanged(views::Widget* widget,
                                         bool active) override;

  // WebContentsObserver implementation.
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) override;
  virtual void RenderViewHostChanged(
      content::RenderViewHost* old_host,
      content::RenderViewHost* new_host) override;

  // views::View implementation.
  virtual void Layout() override;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) override;
  virtual gfx::Size GetMinimumSize() const override;
  virtual gfx::Size GetMaximumSize() const override;
  virtual void OnFocus() override;

  // NativeAppWindow implementation.
  virtual void SetFullscreen(int fullscreen_types) override;
  virtual bool IsFullscreenOrPending() const override;
  virtual void UpdateWindowIcon() override;
  virtual void UpdateWindowTitle() override;
  virtual void UpdateBadgeIcon() override;
  virtual void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions) override;
  virtual SkRegion* GetDraggableRegion() override;
  virtual void UpdateShape(scoped_ptr<SkRegion> region) override;
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) override;
  virtual bool IsFrameless() const override;
  virtual bool HasFrameColor() const override;
  virtual SkColor ActiveFrameColor() const override;
  virtual SkColor InactiveFrameColor() const override;
  virtual gfx::Insets GetFrameInsets() const override;
  virtual void HideWithApp() override;
  virtual void ShowWithApp() override;
  virtual void UpdateShelfMenu() override;
  virtual gfx::Size GetContentMinimumSize() const override;
  virtual gfx::Size GetContentMaximumSize() const override;
  virtual void SetContentSizeConstraints(const gfx::Size& min_size,
                                         const gfx::Size& max_size) override;
  virtual bool CanHaveAlphaEnabled() const override;
  virtual void SetVisibleOnAllWorkspaces(bool always_visible) override;

  // web_modal::WebContentsModalDialogHost implementation.
  virtual gfx::NativeView GetHostView() const override;
  virtual gfx::Point GetDialogPosition(const gfx::Size& size) override;
  virtual gfx::Size GetMaximumDialogSize() override;
  virtual void AddObserver(
      web_modal::ModalDialogHostObserver* observer) override;
  virtual void RemoveObserver(
      web_modal::ModalDialogHostObserver* observer) override;

 private:
  // Informs modal dialogs that they need to update their positions.
  void OnViewWasResized();

  extensions::AppWindow* app_window_;  // Not owned.
  views::WebView* web_view_;
  views::Widget* widget_;

  scoped_ptr<SkRegion> draggable_region_;

  bool frameless_;
  bool resizable_;
  extensions::SizeConstraints size_constraints_;

  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  ObserverList<web_modal::ModalDialogHostObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(NativeAppWindowViews);
};

}  // namespace native_app_window

#endif  // EXTENSIONS_COMPONENTS_NATIVE_APP_WINDOW_NATIVE_APP_WINDOW_VIEWS_H_
