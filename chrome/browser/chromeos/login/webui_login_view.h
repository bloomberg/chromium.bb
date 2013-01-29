// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_VIEW_H_

#include <map>
#include <string>

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

namespace chromeos {

// View used to render a WebUI supporting Widget. This widget is used for the
// WebUI based start up and lock screens. It contains a WebView.
class WebUILoginView : public views::WidgetDelegateView,
                       public content::WebContentsDelegate,
                       public content::NotificationObserver {
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

  // Returns current WebContents.
  content::WebContents* GetWebContents();

  // Opens proxy settings dialog.
  void OpenProxySettings();

  // Called when WebUI is being shown after being initilized hidden.
  void OnPostponedShow();

  // Toggles status area visibility.
  void SetStatusAreaVisible(bool visible);

  // Sets whether UI should be enabled.
  void SetUIEnabled(bool enabled);

  void set_is_hidden(bool hidden) { is_hidden_ = hidden; }

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
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual bool IsPopupOrPanel(
      const content::WebContents* source) const OVERRIDE;
  virtual bool TakeFocus(content::WebContents* source, bool reverse) OVERRIDE;
  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE;

  // Performs series of actions when login prompt is considered
  // to be ready and visible.
  // 1. Emits LoginPromptVisible signal if needed
  // 2. Notifies OOBE/sign classes.
  void OnLoginPromptVisible();

  // Called when focus is returned from status area.
  // |reverse| is true when focus is traversed backwards (using Shift-Tab).
  void ReturnFocus(bool reverse);

  content::NotificationRegistrar registrar_;

  // Login window which shows the view.
  views::Widget* login_window_;

  // Converts keyboard events on the WebContents to accelerators.
  UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  // Maps installed accelerators to OOBE webui accelerator identifiers.
  AccelMap accel_map_;

  // Whether the host window is frozen.
  bool host_window_frozen_;

  // True when WebUI is being initialized hidden.
  bool is_hidden_;

  // True is login-prompt-visible event has been already handled.
  bool login_prompt_visible_handled_;

  // Should we emit the login-prompt-visible signal when the login page is
  // displayed?
  bool should_emit_login_prompt_visible_;

  // True to forward keyboard event.
  bool forward_keyboard_event_;

  DISALLOW_COPY_AND_ASSIGN(WebUILoginView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_VIEW_H_
