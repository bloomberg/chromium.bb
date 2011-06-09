// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/login/login_html_dialog.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "views/view.h"

class DOMView;
class GURL;
class Profile;
class WebUI;

namespace views {
class Widget;
class WindowDelegate;
}

namespace chromeos {

class StatusAreaView;

// View used to render a WebUI supporting Widget. This widget is used for the
// WebUI based start up and lock screens. It contains a StatusAreaView and
// DOMView.
class WebUILoginView : public views::View,
                       public StatusAreaHost,
                       public chromeos::LoginHtmlDialog::Delegate {
 public:
  WebUILoginView();
  virtual ~WebUILoginView();

  // Initializes the webui login view.
  virtual void Init();

  // Overridden from views::Views:
  virtual std::string GetClassName() const OVERRIDE;

  // Overridden from StatusAreaHost:
  virtual gfx::NativeWindow GetNativeWindow() const;

  // Invokes SetWindowType for the window. This is invoked during startup and
  // after we've painted.
  void UpdateWindowType();

  // Loads given page. Should be called after Init() has been called.
  void LoadURL(const GURL& url);

  // Returns current WebUI.
  WebUI* GetWebUI();

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

 protected:
  // Creates and adds the status_area.
  void InitStatusArea();

  Profile* profile_;
  StatusAreaView* status_area_;
  // DOMView for rendering a webpage as a webui login.
  DOMView* webui_login_;
  // Proxy settings dialog that can be invoked from network menu.
  scoped_ptr<LoginHtmlDialog> proxy_settings_dialog_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebUILoginView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WEBUI_LOGIN_VIEW_H_
