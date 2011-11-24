// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_VIEW_H_
#pragma once

#include <map>
#include <string>

#include "chrome/browser/chromeos/login/login_html_dialog.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/tab_first_render_watcher.h"
#include "chrome/browser/ui/views/unhandled_keyboard_event_handler.h"
#include "content/browser/tab_contents/tab_contents_delegate.h"
#include "ui/views/widget/widget_delegate.h"

class DOMView;
class GURL;
class WebUI;

namespace views {
class View;
class Widget;
}

namespace chromeos {

class StatusAreaViewChromeos;

// View used to render a WebUI supporting Widget. This widget is used for the
// WebUI based start up and lock screens. It contains a StatusAreaView and
// DOMView.
class WebUILoginView : public views::WidgetDelegateView,
                       public StatusAreaButton::Delegate,
                       public TabContentsDelegate,
                       public TabFirstRenderWatcher::Delegate {
 public:
  static const int kStatusAreaCornerPadding;

  WebUILoginView();
  virtual ~WebUILoginView();

  // Initializes the webui login view.
  virtual void Init(views::Widget* login_window);

  // Overridden from views::Views:
  virtual bool AcceleratorPressed(
      const ui::Accelerator& accelerator) OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;

  // Called when WebUI window is created.
  virtual void OnWindowCreated();

  // Gets the native window from the view widget.
  gfx::NativeWindow GetNativeWindow() const;

  // Invokes SetWindowType for the window. This is invoked during startup and
  // after we've painted.
  void UpdateWindowType();

  // Loads given page. Should be called after Init() has been called.
  void LoadURL(const GURL& url);

  // Returns current WebUI.
  WebUI* GetWebUI();

  // Toggles whether status area is enabled.
  void SetStatusAreaEnabled(bool enable);

  // Toggles status area visibility.
  void SetStatusAreaVisible(bool visible);

 protected:
  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual void OnLocaleChanged() OVERRIDE;
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;

  // Overridden from StatusAreaButton::Delegate:
  virtual bool ShouldExecuteStatusAreaCommand(
      const views::View* button_view, int command_id) const OVERRIDE;
  virtual void ExecuteStatusAreaCommand(
      const views::View* button_view, int command_id) OVERRIDE;
  virtual gfx::Font GetStatusAreaFont(const gfx::Font& font) const OVERRIDE;
  virtual StatusAreaButton::TextStyle GetStatusAreaTextStyle() const OVERRIDE;
  virtual void ButtonVisibilityChanged(views::View* button_view) OVERRIDE;

  // TabFirstRenderWatcher::Delegate implementation.
  virtual void OnRenderHostCreated(RenderViewHost* host) OVERRIDE;
  virtual void OnTabMainFrameLoaded() OVERRIDE;
  virtual void OnTabMainFrameFirstRender() OVERRIDE;

  // Creates and adds the status area (separate window).
  virtual void InitStatusArea();

  StatusAreaViewChromeos* status_area_;

  // DOMView for rendering a webpage as a webui login.
  DOMView* webui_login_;

 private:
  // Map type for the accelerator-to-identifier map.
  typedef std::map<ui::Accelerator, std::string> AccelMap;

  // Overridden from TabContentsDelegate.
  virtual bool HandleContextMenu(const ContextMenuParams& params) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual bool IsPopupOrPanel(const TabContents* source) const OVERRIDE;
  virtual bool TakeFocus(bool reverse) OVERRIDE;

  // Called when focus is returned from status area.
  // |reverse| is true when focus is traversed backwards (using Shift-Tab).
  void ReturnFocus(bool reverse);

  // Login window which shows the view.
  views::Widget* login_window_;

  // Window that contains status area.
  // TODO(nkostylev): Temporary solution till we have
  // RenderWidgetHostViewViews working.
  views::Widget* status_window_;

  // Converts keyboard events on the TabContents to accelerators.
  UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Maps installed accelerators to OOBE webui accelerator identifiers.
  AccelMap accel_map_;

  // Proxy settings dialog that can be invoked from network menu.
  scoped_ptr<LoginHtmlDialog> proxy_settings_dialog_;

  // Watches webui_login_'s TabContents rendering.
  scoped_ptr<TabFirstRenderWatcher> tab_watcher_;

  // Whether the host window is frozen.
  bool host_window_frozen_;

  // Caches StatusArea visibility setting before it has been initialized.
  bool status_area_visibility_on_init_;

  DISALLOW_COPY_AND_ASSIGN(WebUILoginView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_VIEW_H_
