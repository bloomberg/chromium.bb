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
class Command;
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
//    GENERIC        -> The app menu. This case includes page actions that
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
  class ExtensionInstalledBubbleUi {
   public:
    virtual ~ExtensionInstalledBubbleUi() {}

    // Returns false if, because of animating (such as from adding a new browser
    // action to the toolbar), the bubble could not be shown immediately.
    // TODO(hcarmona): Detect animation in a platform-agnostic manner.
    static bool ShouldShow(ExtensionInstalledBubble* bubble);

    // Shows the bubble UI. Should be implemented per platform.
    virtual void Show() = 0;
  };

  ExtensionInstalledBubble(const extensions::Extension* extension,
                           Browser* browser,
                           const SkBitmap& icon);

  ~ExtensionInstalledBubble() override;

  const extensions::Extension* extension() const { return extension_; }
  Browser* browser() { return browser_; }
  const Browser* browser() const { return browser_; }
  const SkBitmap& icon() const { return icon_; }
  BubbleType type() const { return type_; }
  bool has_command_keybinding() const { return action_command_; }

  // Stop listening to NOTIFICATION_BROWSER_CLOSING.
  void IgnoreBrowserClosing();

  // Returns the string describing how to use the new extension.
  base::string16 GetHowToUseDescription() const;

  // Sets the UI that corresponds to this bubble.
  void SetBubbleUi(ExtensionInstalledBubbleUi* bubble_ui);

 private:
  // Delegates showing the view to our |view_|. Called internally via PostTask.
  void ShowInternal();

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const extensions::Extension* extension,
      extensions::UnloadedExtensionInfo::Reason reason) override;

  // The view that shows the bubble. Owns us.
  ExtensionInstalledBubbleUi* bubble_ui_;

  // |extension_| is NULL when we are deleted.
  const extensions::Extension* extension_;
  Browser* browser_;
  const SkBitmap icon_;
  BubbleType type_;
  content::NotificationRegistrar registrar_;

  // The command to execute the extension action, if one exists.
  scoped_ptr<extensions::Command> action_command_;

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
