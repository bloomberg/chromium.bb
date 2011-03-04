// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_H_
#pragma once

#include "base/ref_counted.h"
#include "chrome/browser/ui/views/info_bubble.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Browser;
class Extension;
class InstalledBubbleContent;
class SkBitmap;

// Provides feedback to the user upon successful installation of an
// extension. Depending on the type of extension, the InfoBubble will
// point to:
//    OMNIBOX_KEYWORD-> The omnibox.
//    BROWSER_ACTION -> The browserAction icon in the toolbar.
//    PAGE_ACTION    -> A preview of the pageAction icon in the location
//                      bar which is shown while the InfoBubble is shown.
//    GENERIC        -> The wrench menu. This case includes pageActions that
//                      don't specify a default icon.
//
// ExtensionInstallBubble manages its own lifetime.
class ExtensionInstalledBubble
    : public InfoBubbleDelegate,
      public NotificationObserver,
      public base::RefCountedThreadSafe<ExtensionInstalledBubble> {
 public:
  // The behavior and content of this InfoBubble comes in these varieties:
  enum BubbleType {
    OMNIBOX_KEYWORD,
    BROWSER_ACTION,
    PAGE_ACTION,
    GENERIC
  };

  // Creates the ExtensionInstalledBubble and schedules it to be shown once
  // the extension has loaded. |extension| is the installed extension. |browser|
  // is the browser window which will host the bubble. |icon| is the install
  // icon of the extension.
  static void Show(const Extension* extension, Browser *browser, SkBitmap icon);

 private:
  friend class base::RefCountedThreadSafe<ExtensionInstalledBubble>;

  // Private ctor. Registers a listener for EXTENSION_LOADED.
  ExtensionInstalledBubble(const Extension* extension, Browser *browser,
                           SkBitmap icon);

  virtual ~ExtensionInstalledBubble();

  // Shows the bubble. Called internally via PostTask.
  void ShowInternal();

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // InfoBubbleDelegate
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape);
  virtual bool CloseOnEscape();
  virtual bool FadeInOnShow();

  const Extension* extension_;
  Browser* browser_;
  SkBitmap icon_;
  NotificationRegistrar registrar_;
  InstalledBubbleContent* bubble_content_;
  BubbleType type_;

  // How many times we've deferred due to animations being in progress.
  int animation_wait_retries_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubble);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_H_
