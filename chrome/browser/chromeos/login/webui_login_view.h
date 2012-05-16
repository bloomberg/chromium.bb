// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_VIEW_H_
#pragma once

#include <map>
#include <string>

#include "chrome/browser/tab_render_watcher.h"
#include "chrome/browser/ui/views/unhandled_keyboard_event_handler.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

class GURL;

namespace content {
class WebUI;
}

namespace views {
class View;
class WebView;
class Widget;
}

class TabContentsWrapper;

namespace chromeos {

// View used to render a WebUI supporting Widget. This widget is used for the
// WebUI based start up and lock screens. It contains a WebView.
class WebUILoginView : public views::WidgetDelegateView,
                       public content::WebContentsDelegate,
                       public content::NotificationObserver,
                       public TabRenderWatcher::Delegate {
 public:
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
  content::WebUI* GetWebUI();

  // Opens proxy settings dialog.
  void OpenProxySettings();

  // Toggles status area visibility.
  void SetStatusAreaVisible(bool visible);

 protected:
  // Let non-login derived classes suppress emission of this signal.
  void set_should_emit_login_prompt_visible(bool emit) {
    should_emit_login_prompt_visible_ = emit;
  }

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual void OnLocaleChanged() OVERRIDE;
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;
  virtual void AboutToRequestFocusFromTabTraversal(bool reverse) OVERRIDE;

  // TabRenderWatcher::Delegate implementation.
  virtual void OnRenderHostCreated(content::RenderViewHost* host) OVERRIDE;
  virtual void OnTabMainFrameLoaded() OVERRIDE;
  virtual void OnTabMainFrameRender() OVERRIDE;

  // Overridden from content::NotificationObserver.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // WebView for rendering a webpage as a webui login.
  views::WebView* webui_login_;

 private:
  // Map type for the accelerator-to-identifier map.
  typedef std::map<ui::Accelerator, std::string> AccelMap;

  // Overridden from content::WebContentsDelegate.
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) OVERRIDE;
  virtual void HandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) OVERRIDE;
  virtual bool IsPopupOrPanel(
      const content::WebContents* source) const OVERRIDE;
  virtual bool TakeFocus(bool reverse) OVERRIDE;

  // Called when focus is returned from status area.
  // |reverse| is true when focus is traversed backwards (using Shift-Tab).
  void ReturnFocus(bool reverse);

  content::NotificationRegistrar registrar_;

  // TabContentsWrapper for the WebView.
  // TODO: this is needed for password manager, should be refactored/replaced
  //       so that this code can move to src/ash.
  scoped_ptr<TabContentsWrapper> wrapper_;

  // Login window which shows the view.
  views::Widget* login_window_;

  // Converts keyboard events on the WebContents to accelerators.
  UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Maps installed accelerators to OOBE webui accelerator identifiers.
  AccelMap accel_map_;

  // Watches webui_login_'s WebContents rendering.
  scoped_ptr<TabRenderWatcher> tab_watcher_;

  // Whether the host window is frozen.
  bool host_window_frozen_;

  // Has the login page told us that it's ready?  This is triggered by either
  // all of the user images or the GAIA prompt being loaded, whichever comes
  // first.
  bool login_page_is_loaded_;

  // Should we emit the login-prompt-visible signal when the login page is
  // displayed?
  bool should_emit_login_prompt_visible_;

  DISALLOW_COPY_AND_ASSIGN(WebUILoginView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_VIEW_H_
