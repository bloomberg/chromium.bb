// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/login/login_html_dialog.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "views/focus/focus_manager.h"
#include "views/view.h"

class DOMView;
class GURL;
class KeyboardContainerView;
class NotificationDetails;
class NotificationSource;
class Profile;

namespace views {
class Widget;
class WindowDelegate;
}

namespace chromeos {

class StatusAreaView;

// View used to render a WebUI supporting Widget. This widget is used for the
// WebUI based start up and lock screens. It contains a StatusAreaView, DOMView,
// and on touch builds a KeyboardContainerView.
class WebUILoginView : public views::View,
                       public StatusAreaHost,
                       public chromeos::LoginHtmlDialog::Delegate,
                       public views::FocusChangeListener,
                       public NotificationObserver {
 public:
  enum VirtualKeyboardType {
    NONE,
    GENERIC,
    URL,
  };

  // Internal class name.
  static const char kViewClassName[];

  WebUILoginView();

  // Initializes the webui login view. |login_url| must be specified.
  void Init(const GURL& login_url);

  // Creates a window containing an instance of WebUILoginView as the root
  // view. The caller is responsible for showing (and closing) the returned
  // widget. The WebUILoginView is set in |view|. |login_url| is the webpage
  // that will be opened in the DOMView.
  static views::Widget* CreateWindowContainingView(
      const gfx::Rect& bounds,
      const GURL& login_url,
      WebUILoginView** view);

  // Overriden from views::Views:
  virtual std::string GetClassName() const OVERRIDE;

  // Overridden from StatusAreaHost:
  virtual gfx::NativeWindow GetNativeWindow() const;

  // Overridden from views::FocusChangeListener:
  virtual void FocusWillChange(views::View* focused_before,
                               views::View* focused_now);

 protected:
  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;

  // Overridden from StatusAreaHost:
  virtual Profile* GetProfile() const OVERRIDE;
  virtual void ExecuteBrowserCommand(int id) const OVERRIDE;
  virtual bool ShouldOpenButtonOptions(
      const views::View* button_view) const OVERRIDE;
  virtual void OpenButtonOptions(const views::View* button_view) OVERRIDE;
  virtual ScreenMode GetScreenMode() const OVERRIDE;
  virtual TextStyle GetTextStyle() const OVERRIDE;

  // Overridden from LoginHtmlDialog::Delegate:
  virtual void OnDialogClosed() OVERRIDE;
  virtual void OnLocaleChanged() OVERRIDE;

 private:
  // Creates and adds the status_area.
  void InitStatusArea();

  // Invokes SetWindowType for the window. This is invoked during startup and
  // after we've painted.
  void UpdateWindowType();

  void InitVirtualKeyboard();
  void UpdateKeyboardAndLayout(bool should_show_keyboard);
  VirtualKeyboardType DecideKeyboardStateForView(views::View* view);

  // Overridden from NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  Profile* profile_;

  StatusAreaView* status_area_;

  // DOMView for rendering a webpage as a webui login.
  DOMView* webui_login_;

  // Proxy settings dialog that can be invoked from network menu.
  scoped_ptr<LoginHtmlDialog> proxy_settings_dialog_;

  bool keyboard_showing_;
  bool focus_listener_added_;
  KeyboardContainerView* keyboard_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WebUILoginView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_VIEW_H_
