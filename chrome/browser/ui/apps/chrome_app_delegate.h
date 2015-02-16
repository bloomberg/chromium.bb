// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_CHROME_APP_DELEGATE_H_
#define CHROME_BROWSER_UI_APPS_CHROME_APP_DELEGATE_H_

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/app_window/app_delegate.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/rect.h"

class ScopedKeepAlive;

class ChromeAppDelegate : public extensions::AppDelegate,
                          public content::NotificationObserver {
 public:
  // Pass a ScopedKeepAlive to prevent the browser process from shutting down
  // while this object exists.
  explicit ChromeAppDelegate(scoped_ptr<ScopedKeepAlive> keep_alive);
  ~ChromeAppDelegate() override;

  static void DisableExternalOpenForTesting();

 private:
  static void RelinquishKeepAliveAfterTimeout(
      const base::WeakPtr<ChromeAppDelegate>& chrome_app_delegate);

  class NewWindowContentsDelegate;

  // extensions::AppDelegate:
  void InitWebContents(content::WebContents* web_contents) override;
  void RenderViewCreated(content::RenderViewHost* render_view_host) override;
  void ResizeWebContents(content::WebContents* web_contents,
                         const gfx::Size& size) override;
  content::WebContents* OpenURLFromTab(
      content::BrowserContext* context,
      content::WebContents* source,
      const content::OpenURLParams& params) override;
  void AddNewContents(content::BrowserContext* context,
                      content::WebContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;
  content::ColorChooser* ShowColorChooser(content::WebContents* web_contents,
                                          SkColor initial_color) override;
  void RunFileChooser(content::WebContents* tab,
                      const content::FileChooserParams& params) override;
  void RequestMediaAccessPermission(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback,
      const extensions::Extension* extension) override;
  bool CheckMediaAccessPermission(
      content::WebContents* web_contents,
      const GURL& security_origin,
      content::MediaStreamType type,
      const extensions::Extension* extension) override;
  int PreferredIconSize() override;
  void SetWebContentsBlocked(content::WebContents* web_contents,
                             bool blocked) override;
  bool IsWebContentsVisible(content::WebContents* web_contents) override;
  void SetTerminatingCallback(const base::Closure& callback) override;
  void OnHide() override;
  void OnShow() override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  bool has_been_shown_;
  bool is_hidden_;
  scoped_ptr<ScopedKeepAlive> keep_alive_;
  scoped_ptr<NewWindowContentsDelegate> new_window_contents_delegate_;
  base::Closure terminating_callback_;
  content::NotificationRegistrar registrar_;
  base::WeakPtrFactory<ChromeAppDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAppDelegate);
};

#endif  // CHROME_BROWSER_UI_APPS_CHROME_APP_DELEGATE_H_
