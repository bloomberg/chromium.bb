// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_H_

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/views/bubble/bubble_delegate.h"

class Browser;

namespace extensions {
class Extension;
}

// Provides feedback to the user upon successful installation of an
// extension. Depending on the type of extension, the Bubble will
// point to:
//    OMNIBOX_KEYWORD-> The omnibox.
//    BROWSER_ACTION -> The browserAction icon in the toolbar.
//    PAGE_ACTION    -> A preview of the pageAction icon in the location
//                      bar which is shown while the Bubble is shown.
//    GENERIC        -> The wrench menu. This case includes pageActions that
//                      don't specify a default icon.
class ExtensionInstalledBubble
    : public views::BubbleDelegateView,
      public content::NotificationObserver {
 public:
  // The behavior and content of this Bubble comes in these varieties:
  enum BubbleType {
    OMNIBOX_KEYWORD = 0,
    BROWSER_ACTION,
    PAGE_ACTION,
    GENERIC,
  };

  // Creates the ExtensionInstalledBubble and schedules it to be shown once
  // the extension has loaded. |extension| is the installed extension. |browser|
  // is the browser window which will host the bubble. |icon| is the install
  // icon of the extension.
  static void Show(const extensions::Extension* extension,
                   Browser *browser,
                   const SkBitmap& icon);

 private:
  // Private ctor. Registers a listener for EXTENSION_LOADED.
  ExtensionInstalledBubble(const extensions::Extension* extension,
                           Browser *browser,
                           const SkBitmap& icon);

  virtual ~ExtensionInstalledBubble();

  // Shows the bubble. Called internally via PostTask.
  void ShowInternal();

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // views::WidgetDelegate
  virtual void WindowClosing() OVERRIDE;

  // views::BubbleDelegate
  virtual gfx::Rect GetAnchorRect() OVERRIDE;

  const extensions::Extension* extension_;
  Browser* browser_;
  SkBitmap icon_;
  content::NotificationRegistrar registrar_;
  BubbleType type_;

  // How many times we've deferred due to animations being in progress.
  int animation_wait_retries_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubble);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_H_
