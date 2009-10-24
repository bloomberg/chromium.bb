// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BROWSER_ACTIONS_TOOLBAR_GTK_H_
#define CHROME_BROWSER_GTK_BROWSER_ACTIONS_TOOLBAR_GTK_H_

#include <map>
#include <string>

#include "base/linked_ptr.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/owned_widget_gtk.h"

class Browser;
class BrowserActionButton;
class Extension;
class Profile;

typedef struct _GtkWidget GtkWidget;

class BrowserActionsToolbarGtk : public NotificationObserver {
 public:
  explicit BrowserActionsToolbarGtk(Browser* browser);
  ~BrowserActionsToolbarGtk();

  GtkWidget* widget() { return hbox_.get(); }

  int button_count() { return extension_button_map_.size(); }

  Browser* browser() { return browser_; }

  // Returns the currently selected tab ID, or -1 if there is none.
  int GetCurrentTabId();

  // Update the display of all buttons.
  void Update();

  // NotificationObserver implementation.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details);

 private:
  // Query the extensions service for all extensions with browser actions,
  // and create the UI for them.
  void CreateAllButtons();

  // Create the UI for a single browser action. This will stick the button
  // at the end of the toolbar.
  // TODO(estade): is this OK, or does it need to place it in a specific
  // location on the toolbar?
  void CreateButtonForExtension(Extension* extension);

  // Delete resources associated with UI for a browser action.
  void RemoveButtonForExtension(Extension* extension);

  // Change the visibility of widget() based on whether we have any buttons
  // to show.
  void UpdateVisibility();

  Browser* browser_;

  Profile* profile_;

  OwnedWidgetGtk hbox_;

  NotificationRegistrar registrar_;

  // Map from extension ID to BrowserActionButton, which is a wrapper for
  // a chrome button and related functionality. There should be one entry
  // for every extension that has a browser action.
  typedef std::map<std::string, linked_ptr<BrowserActionButton> >
      ExtensionButtonMap;
  ExtensionButtonMap extension_button_map_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionsToolbarGtk);
};

#endif  // CHROME_BROWSER_GTK_BROWSER_ACTIONS_TOOLBAR_GTK_H_
