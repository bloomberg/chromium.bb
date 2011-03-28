// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_GTK_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/info_bubble_gtk.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Browser;
class BrowserWindowGtk;
class Extension;
class SkBitmap;

// Provides feedback to the user upon successful installation of an
// extension. Depending on the type of extension, the InfoBubble will
// point to:
//    OMNIBOX_KEYWORD-> The omnibox.
//    BROWSER_ACTION -> The browserAction icon in the toolbar.
//    PAGE_ACTION    -> A preview of the page action icon in the location
//                      bar which is shown while the InfoBubble is shown.
//    GENERIC        -> The wrench menu. This case includes page actions that
//                      don't specify a default icon.
//
// ExtensionInstallBubble manages its own lifetime.
class ExtensionInstalledBubbleGtk
    : public InfoBubbleGtkDelegate,
      public NotificationObserver,
      public base::RefCountedThreadSafe<ExtensionInstalledBubbleGtk> {
 public:
  // The behavior and content of this InfoBubble comes in three varieties.
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
  static void Show(
      const Extension* extension, Browser *browser, const SkBitmap& icon);

 private:
  friend class base::RefCountedThreadSafe<ExtensionInstalledBubbleGtk>;

  // Private ctor. Registers a listener for EXTENSION_LOADED.
  ExtensionInstalledBubbleGtk(const Extension* extension, Browser *browser,
                              const SkBitmap& icon);

  virtual ~ExtensionInstalledBubbleGtk();

  // Shows the bubble. Called internally via PostTask.
  void ShowInternal();

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // InfoBubbleDelegate
  virtual void InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                 bool closed_by_escape);

  // Calls Release() internally. Called internally via PostTask.
  void Close();

  static void OnButtonClick(GtkWidget* button,
                            ExtensionInstalledBubbleGtk* toolbar);

  const Extension* extension_;
  Browser *browser_;
  SkBitmap icon_;
  NotificationRegistrar registrar_;
  BubbleType type_;

  // The number of times to retry showing the bubble if the browser action
  // toolbar is animating.
  int animation_wait_retries_;

  // The 'x' that the user can press to hide the info bubble shelf.
  scoped_ptr<CustomDrawButton> close_button_;

  InfoBubbleGtk* info_bubble_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_GTK_H_
