// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_APP_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_APP_DELEGATE_H_

#include "extensions/browser/app_window/app_delegate.h"

namespace extensions {

// app_shell's AppDelegate implementation.
class ShellAppDelegate : public AppDelegate {
 public:
  ShellAppDelegate();
  virtual ~ShellAppDelegate();

  // AppDelegate overrides:
  virtual void InitWebContents(content::WebContents* web_contents) OVERRIDE;
  virtual void ResizeWebContents(content::WebContents* web_contents,
                                 const gfx::Size& size) OVERRIDE;
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
  virtual void RunFileChooser(
      content::WebContents* tab,
      const content::FileChooserParams& params) OVERRIDE;
  virtual void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      const Extension* extension) OVERRIDE;
  virtual bool CheckMediaAccessPermission(content::WebContents* web_contents,
                                          const GURL& security_origin,
                                          content::MediaStreamType type,
                                          const Extension* extension) OVERRIDE;
  virtual int PreferredIconSize() OVERRIDE;
  virtual void SetWebContentsBlocked(content::WebContents* web_contents,
                                     bool blocked) OVERRIDE;
  virtual bool IsWebContentsVisible(
      content::WebContents* web_contents) OVERRIDE;
  virtual void SetTerminatingCallback(const base::Closure& callback) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellAppDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_APP_DELEGATE_H_
