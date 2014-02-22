// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_SHELL_APP_WINDOW_DELEGATE_H_
#define APPS_SHELL_BROWSER_SHELL_APP_WINDOW_DELEGATE_H_

#include "apps/app_window.h"

namespace apps {

// The AppWindow::Delegate for app_shell. Used to create instances of
// NativeAppWindow. Other functionality is not supported.
class ShellAppWindowDelegate : public AppWindow::Delegate {
 public:
  ShellAppWindowDelegate();
  virtual ~ShellAppWindowDelegate();

 private:
  // ShellWindow::Delegate:
  virtual void InitWebContents(content::WebContents* web_contents) OVERRIDE;
  virtual NativeAppWindow* CreateNativeAppWindow(
      AppWindow* window,
      const AppWindow::CreateParams& params) OVERRIDE;
  virtual content::WebContents* OpenURLFromTab(
      content::BrowserContext* context,
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void AddNewContents(content::BrowserContext* context,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual content::ColorChooser* ShowColorChooser(
      content::WebContents* web_contents,
      SkColor initial_color) OVERRIDE;
  virtual void RunFileChooser(content::WebContents* tab,
                              const content::FileChooserParams& params)
      OVERRIDE;
  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      const extensions::Extension* extension) OVERRIDE;
  virtual int PreferredIconSize() OVERRIDE;
  virtual void SetWebContentsBlocked(content::WebContents* web_contents,
                                     bool blocked) OVERRIDE;
  virtual bool IsWebContentsVisible(content::WebContents* web_contents)
      OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ShellAppWindowDelegate);
};

}  // namespace apps

#endif  // APPS_SHELL_BROWSER_SHELL_APP_WINDOW_DELEGATE_H_
