// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_CHROME_SHELL_WINDOW_DELEGATE_H_
#define CHROME_BROWSER_UI_APPS_CHROME_SHELL_WINDOW_DELEGATE_H_

#include "apps/shell_window.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/rect.h"

class ShellWindowLinkDelegate : public content::WebContentsDelegate {
 public:
  ShellWindowLinkDelegate();
  virtual ~ShellWindowLinkDelegate();

 private:
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowLinkDelegate);
};

class ChromeShellWindowDelegate : public apps::ShellWindow::Delegate {
 public:
  ChromeShellWindowDelegate();
  virtual ~ChromeShellWindowDelegate();

  static void DisableExternalOpenForTesting();

 private:
  // apps::ShellWindow::Delegate:
  virtual void InitWebContents(content::WebContents* web_contents) OVERRIDE;
  virtual apps::NativeAppWindow* CreateNativeAppWindow(
      apps::ShellWindow* window,
      const apps::ShellWindow::CreateParams& params) OVERRIDE;
  virtual content::WebContents* OpenURLFromTab(
      Profile* profile,
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE;
  virtual void AddNewContents(Profile* profile,
                              content::WebContents* new_contents,
                              WindowOpenDisposition disposition,
                              const gfx::Rect& initial_pos,
                              bool user_gesture,
                              bool* was_blocked) OVERRIDE;
  virtual content::ColorChooser* ShowColorChooser(
      content::WebContents* web_contents,
      SkColor initial_color) OVERRIDE;
  virtual void RunFileChooser(
      content::WebContents* tab,
      const content::FileChooserParams& params) OVERRIDE;
  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      const extensions::Extension* extension) OVERRIDE;
  virtual int PreferredIconSize() OVERRIDE;
  virtual void SetWebContentsBlocked(content::WebContents* web_contents,
                                     bool blocked) OVERRIDE;
  virtual bool IsWebContentsVisible(
      content::WebContents* web_contents) OVERRIDE;

  // Implemented in platform specific code.
  static apps::NativeAppWindow* CreateNativeAppWindowImpl(
      apps::ShellWindow* window,
      const apps::ShellWindow::CreateParams& params);

  scoped_ptr<ShellWindowLinkDelegate> shell_window_link_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ChromeShellWindowDelegate);
};

#endif  // CHROME_BROWSER_UI_APPS_CHROME_SHELL_WINDOW_DELEGATE_H_
