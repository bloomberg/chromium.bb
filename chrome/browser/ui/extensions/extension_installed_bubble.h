// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/browser/extension_registry_observer.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Browser;

namespace extensions {
class Extension;
class ExtensionRegistry;
}

// Provides feedback to the user upon successful installation of an
// extension. Depending on the type of extension, the Bubble will
// point to:
//    OMNIBOX_KEYWORD-> The omnibox.
//    BROWSER_ACTION -> The browser action icon in the toolbar.
//    PAGE_ACTION    -> A preview of the page action icon in the location
//                      bar which is shown while the Bubble is shown.
//    GENERIC        -> The wrench menu. This case includes page actions that
//                      don't specify a default icon.
//
// ExtensionInstallBubble manages its own lifetime.
class ExtensionInstalledBubble : public content::NotificationObserver,
                                 public extensions::ExtensionRegistryObserver {
 public:
  // The behavior and content of this Bubble comes in these varieties:
  enum BubbleType {
    OMNIBOX_KEYWORD,
    BROWSER_ACTION,
    PAGE_ACTION,
    GENERIC
  };

  // Implements the UI for showing the bubble. Owns us.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Attempts to show the bubble. Called from ShowInternal. Returns false
    // if, because of animating (such as from adding a new browser action
    // to the toolbar), the bubble could not be shown immediately.
    virtual bool MaybeShowNow() = 0;
  };

  ExtensionInstalledBubble(Delegate* delegate,
                           const extensions::Extension* extension,
                           Browser *browser,
                           const SkBitmap& icon);

  virtual ~ExtensionInstalledBubble();

  const extensions::Extension* extension() const { return extension_; }
  Browser* browser() { return browser_; }
  const Browser* browser() const { return browser_; }
  const SkBitmap& icon() const { return icon_; }
  BubbleType type() const { return type_; }

  // Stop listening to NOTIFICATION_BROWSER_CLOSING.
  void IgnoreBrowserClosing();

 private:
  // Delegates showing the view to our |view_|. Called internally via PostTask.
  void ShowInternal();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // extensions::ExtensionRegistryObserver:
  virtual void OnExtensionLoaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension) OVERRIDE;
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) OVERRIDE;

  // The view delegate that shows the bubble. Owns us.
  Delegate* delegate_;

  // |extension_| is NULL when we are deleted.
  const extensions::Extension* extension_;
  Browser* browser_;
  const SkBitmap icon_;
  BubbleType type_;
  content::NotificationRegistrar registrar_;

  // Listen to extension load, unloaded notifications.
  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  // The number of times to retry showing the bubble if the browser action
  // toolbar is animating.
  int animation_wait_retries_;

  base::WeakPtrFactory<ExtensionInstalledBubble> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubble);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_H_
