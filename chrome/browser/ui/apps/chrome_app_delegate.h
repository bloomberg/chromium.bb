// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_CHROME_APP_DELEGATE_H_
#define CHROME_BROWSER_UI_APPS_CHROME_APP_DELEGATE_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/app_window/app_delegate.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/rect.h"

namespace content {
class BrowserContext;
class WebContents;
}

class ScopedKeepAlive;

class ChromeAppDelegate : public extensions::AppDelegate,
                          public content::NotificationObserver {
 public:
  // Pass a ScopedKeepAlive to prevent the browser process from shutting down
  // while this object exists.
  explicit ChromeAppDelegate(scoped_ptr<ScopedKeepAlive> keep_alive);
  virtual ~ChromeAppDelegate();

  static void DisableExternalOpenForTesting();

 private:
  class NewWindowContentsDelegate;

  // extensions::AppDelegate:
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
      const extensions::Extension* extension) OVERRIDE;
  virtual bool CheckMediaAccessPermission(
      content::WebContents* web_contents,
      const GURL& security_origin,
      content::MediaStreamType type,
      const extensions::Extension* extension) OVERRIDE;
  virtual int PreferredIconSize() OVERRIDE;
  virtual void SetWebContentsBlocked(content::WebContents* web_contents,
                                     bool blocked) OVERRIDE;
  virtual bool IsWebContentsVisible(
      content::WebContents* web_contents) OVERRIDE;
  virtual void SetTerminatingCallback(const base::Closure& callback) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  scoped_ptr<ScopedKeepAlive> keep_alive_;
  scoped_ptr<NewWindowContentsDelegate> new_window_contents_delegate_;
  base::Closure terminating_callback_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAppDelegate);
};

#endif  // CHROME_BROWSER_UI_APPS_CHROME_APP_DELEGATE_H_
