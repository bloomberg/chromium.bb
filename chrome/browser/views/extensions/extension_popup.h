// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_
#define CHROME_BROWSER_VIEWS_EXTENSIONS_EXTENSION_POPUP_H_

#include "googleurl/src/gurl.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/views/browser_bubble.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class Browser;
class ExtensionHost;

class ExtensionPopup : public BrowserBubble,
                       public NotificationObserver {
 public:
  virtual ~ExtensionPopup();

  // Create and show a popup with |url| positioned below |relative_to| in
  // |browser| coordinates. This is anchored to the lower right corner of the
  // rect, extending to the left, just like the wrench and page menus.
  //
  // The actual display of the popup is delayed until the page contents
  // finish loading in order to minimize UI flashing and resizing.
  static ExtensionPopup* Show(const GURL& url, Browser* browser,
                              const gfx::Rect& relative_to,
                              int height);

  ExtensionHost* host() const { return extension_host_.get(); }

  // BrowserBubble overrides.
  virtual void Show();

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  ExtensionPopup(ExtensionHost* host,
                 views::Widget* frame,
                 const gfx::Rect& relative_to);

  // The area on the screen that the popup should be positioned relative to.
  const gfx::Rect relative_to_;

  // The contained host for the view.
  scoped_ptr<ExtensionHost> extension_host_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionPopup);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_POPUP_H_
