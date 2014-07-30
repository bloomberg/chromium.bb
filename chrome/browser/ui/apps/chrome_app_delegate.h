// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_CHROME_APP_DELEGATE_H_
#define CHROME_BROWSER_UI_APPS_CHROME_APP_DELEGATE_H_

#include "apps/app_delegate.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/rect.h"

namespace content {
class BrowserContext;
class WebContents;
}

class ChromeAppDelegate : public apps::AppDelegate {
 public:
  ChromeAppDelegate();
  virtual ~ChromeAppDelegate();

  static void DisableExternalOpenForTesting();

 private:
  class NewWindowContentsDelegate;

  // apps::AppDelegate:
  virtual void InitWebContents(content::WebContents* web_contents) OVERRIDE;
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
      const extensions::Extension* extension) OVERRIDE;
  virtual int PreferredIconSize() OVERRIDE;
  virtual void SetWebContentsBlocked(content::WebContents* web_contents,
                                     bool blocked) OVERRIDE;
  virtual bool IsWebContentsVisible(
      content::WebContents* web_contents) OVERRIDE;

  scoped_ptr<NewWindowContentsDelegate> new_window_contents_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAppDelegate);
};

#endif  // CHROME_BROWSER_UI_APPS_CHROME_APP_DELEGATE_H_
