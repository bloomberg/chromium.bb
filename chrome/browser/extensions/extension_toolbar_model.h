// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLBAR_MODEL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLBAR_MODEL_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;
class ExtensionService;
class PrefService;

// Model for the browser actions toolbar.
class ExtensionToolbarModel : public content::NotificationObserver {
 public:
  explicit ExtensionToolbarModel(ExtensionService* service);
  virtual ~ExtensionToolbarModel();

  // A class which is informed of changes to the model; represents the view of
  // MVC.
  class Observer {
   public:
    // An extension with a browser action button has been added, and should go
    // in the toolbar at |index|.
    virtual void BrowserActionAdded(const extensions::Extension* extension,
                                    int index) {}

    // The browser action button for |extension| should no longer show.
    virtual void BrowserActionRemoved(const extensions::Extension* extension) {}

    // The browser action button for |extension| has been moved to |index|.
    virtual void BrowserActionMoved(const extensions::Extension* extension,
                                    int index) {}

    // The browser action button for |extension_id| (which was not a popup) was
    // clicked, executing it.
    virtual void BrowserActionExecuted(const std::string& extension_id,
                                       Browser* browser) {}

    // Called when the model has finished loading.
    virtual void ModelLoaded() {}

   protected:
    virtual ~Observer() {}
  };

  // Functions called by the view.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void MoveBrowserAction(const extensions::Extension* extension, int index);
  void ExecuteBrowserAction(const std::string& extension_id, Browser* browser);
  // If count == size(), this will set the visible icon count to -1, meaning
  // "show all actions".
  void SetVisibleIconCount(int count);
  // As above, a return value of -1 represents "show all actions".
  int GetVisibleIconCount() { return visible_icon_count_; }

  bool extensions_initialized() const { return extensions_initialized_; }

  size_t size() const {
    return toolitems_.size();
  }

  extensions::ExtensionList::iterator begin() {
    return toolitems_.begin();
  }

  extensions::ExtensionList::iterator end() {
    return toolitems_.end();
  }

  const extensions::Extension* GetExtensionByIndex(int index) const;

  // Utility functions for converting between an index into the list of
  // incognito-enabled browser actions, and the list of all browser actions.
  int IncognitoIndexToOriginal(int incognito_index);
  int OriginalIndexToIncognito(int original_index);

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // To be called after the extension service is ready; gets loaded extensions
  // from the extension service and their saved order from the pref service
  // and constructs |toolitems_| from these data.
  void InitializeExtensionList();

  // Save the model to prefs.
  void UpdatePrefs();

  // Our observers.
  ObserverList<Observer> observers_;

  void AddExtension(const extensions::Extension* extension);
  void RemoveExtension(const extensions::Extension* extension);

  // Our ExtensionService, guaranteed to outlive us.
  ExtensionService* service_;

  PrefService* prefs_;

  // True if we've handled the initial EXTENSIONS_READY notification.
  bool extensions_initialized_;

  // Ordered list of browser action buttons.
  extensions::ExtensionList toolitems_;

  // Keeps track of what the last extension to get disabled was.
  std::string last_extension_removed_;

  // Keeps track of where the last extension to get disabled was in the list.
  size_t last_extension_removed_index_;

  // The number of icons visible (the rest should be hidden in the overflow
  // chevron).
  int visible_icon_count_;

  content::NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLBAR_MODEL_H_
