// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLBAR_MODEL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLBAR_MODEL_H_
#pragma once

#include "base/observer_list.h"
#include "chrome/common/extensions/extension.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class ExtensionService;
class PrefService;

// Model for the browser actions toolbar.
class ExtensionToolbarModel : public NotificationObserver {
 public:
  explicit ExtensionToolbarModel(ExtensionService* service);
  ~ExtensionToolbarModel();

  // Notifies the toolbar model that the Profile that suplied its
  // prefs is being destroyed.
  void DestroyingProfile();

  // A class which is informed of changes to the model; represents the view of
  // MVC.
  class Observer {
   public:
    // An extension with a browser action button has been added, and should go
    // in the toolbar at |index|.
    virtual void BrowserActionAdded(const Extension* extension, int index) {}

    // The browser action button for |extension| should no longer show.
    virtual void BrowserActionRemoved(const Extension* extension) {}

    // The browser action button for |extension| has been moved to |index|.
    virtual void BrowserActionMoved(const Extension* extension, int index) {}

    // Called when the model has finished loading.
    virtual void ModelLoaded() {}

   protected:
    virtual ~Observer() {}
  };

  // Functions called by the view.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void MoveBrowserAction(const Extension* extension, int index);
  // If count == size(), this will set the visible icon count to -1, meaning
  // "show all actions".
  void SetVisibleIconCount(int count);
  // As above, a return value of -1 represents "show all actions".
  int GetVisibleIconCount() { return visible_icon_count_; }

  bool extensions_initialized() const { return extensions_initialized_; }

  size_t size() const {
    return toolitems_.size();
  }

  ExtensionList::iterator begin() {
    return toolitems_.begin();
  }

  ExtensionList::iterator end() {
    return toolitems_.end();
  }

  const Extension* GetExtensionByIndex(int index) const;

  // Utility functions for converting between an index into the list of
  // incognito-enabled browser actions, and the list of all browser actions.
  int IncognitoIndexToOriginal(int incognito_index);
  int OriginalIndexToIncognito(int original_index);

 private:
  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // To be called after the extension service is ready; gets loaded extensions
  // from the extension service and their saved order from the pref service
  // and constructs |toolitems_| from these data.
  void InitializeExtensionList();

  // Save the model to prefs.
  void UpdatePrefs();

  // Our observers.
  ObserverList<Observer> observers_;

  void AddExtension(const Extension* extension);
  void RemoveExtension(const Extension* extension);

  // Our ExtensionService, guaranteed to outlive us.
  ExtensionService* service_;

  PrefService* prefs_;

  // True if we've handled the initial EXTENSIONS_READY notification.
  bool extensions_initialized_;

  // Ordered list of browser action buttons.
  ExtensionList toolitems_;

  // Keeps track of what the last extension to get disabled was.
  std::string last_extension_removed_;

  // Keeps track of where the last extension to get disabled was in the list.
  size_t last_extension_removed_index_;

  // The number of icons visible (the rest should be hidden in the overflow
  // chevron).
  int visible_icon_count_;

  NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLBAR_MODEL_H_
