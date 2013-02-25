// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_THEME_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_THEME_HANDLER_H_

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui_message_handler.h"

class Profile;

namespace content {
class WebUI;
}

// A class to keep the ThemeSource up to date when theme changes.
class ThemeHandler : public content::WebUIMessageHandler,
                     public content::NotificationObserver {
 public:
  explicit ThemeHandler();
  virtual ~ThemeHandler();

 private:
  // content::WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Re/set the CSS caches.
  void InitializeCSSCaches();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Profile* GetProfile() const;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ThemeHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_THEME_HANDLER_H_
