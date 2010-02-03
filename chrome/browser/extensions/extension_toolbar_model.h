// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLBAR_MODEL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLBAR_MODEL_H_

#include "base/observer_list.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class ExtensionsService;

// Model for the browser actions toolbar.
class ExtensionToolbarModel : public NotificationObserver {
 public:
  explicit ExtensionToolbarModel(ExtensionsService* service);
  ~ExtensionToolbarModel();

  // A class which is informed of changes to the model; represents the view of
  // MVC.
  class Observer {
   public:
    // An extension with a browser action button has been added, and should go
    // in the toolbar at |index|.
    virtual void BrowserActionAdded(Extension* extension, int index) {}

    // The browser action button for |extension| should no longer show.
    virtual void BrowserActionRemoved(Extension* extension) {}

    // The browser action button for |extension| has been moved to |index|.
    virtual void BrowserActionMoved(Extension* extension, int index) {}
  };

  // Functions called by the view.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  void MoveBrowserAction(Extension* extension, int index);

  size_t size() const {
    return toolitems_.size();
  }

  ExtensionList::iterator begin() {
    return toolitems_.begin();
  }

  ExtensionList::iterator end() {
    return toolitems_.end();
  }

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

  void AddExtension(Extension* extension);
  void RemoveExtension(Extension* extension);

  // Our ExtensionsService, guaranteed to outlive us.
  ExtensionsService* service_;

  // Ordered list of browser action buttons.
  ExtensionList toolitems_;

  NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLBAR_MODEL_H_
