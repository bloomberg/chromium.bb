// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_BROWSER_ACTIONS_TOOLBAR_GTK_H_
#define CHROME_BROWSER_GTK_BROWSER_ACTIONS_TOOLBAR_GTK_H_

#include <gtk/gtk.h>

#include <map>
#include <string>

#include "app/gtk_signal.h"
#include "base/linked_ptr.h"
#include "base/task.h"
#include "chrome/browser/extensions/extension_toolbar_model.h"
#include "chrome/browser/gtk/overflow_button.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/owned_widget_gtk.h"

class Browser;
class BrowserActionButton;
class Extension;
class GtkThemeProvider;
class Profile;

typedef struct _GtkWidget GtkWidget;

class BrowserActionsToolbarGtk : public ExtensionToolbarModel::Observer {
 public:
  explicit BrowserActionsToolbarGtk(Browser* browser);
  virtual ~BrowserActionsToolbarGtk();

  GtkWidget* widget() { return hbox_.get(); }

  // Returns the widget in use by the BrowserActionButton corresponding to
  // |extension|. Used in positioning the ExtensionInstalledBubble for
  // BrowserActions.
  GtkWidget* GetBrowserActionWidget(Extension* extension);

  int button_count() { return extension_button_map_.size(); }

  Browser* browser() { return browser_; }

  // Returns the currently selected tab ID, or -1 if there is none.
  int GetCurrentTabId();

  // Update the display of all buttons.
  void Update();

 private:
  friend class BrowserActionButton;

  // Initialize drag and drop.
  void SetupDrags();

  // Query the extensions service for all extensions with browser actions,
  // and create the UI for them.
  void CreateAllButtons();

  // Create the UI for a single browser action. This will stick the button
  // at the end of the toolbar.
  void CreateButtonForExtension(Extension* extension, int index);

  // Delete resources associated with UI for a browser action.
  void RemoveButtonForExtension(Extension* extension);

  // Change the visibility of widget() based on whether we have any buttons
  // to show.
  void UpdateVisibility();

  // Hide the extension popup, if any.
  void HidePopup();

  // Returns true if this extension should be shown in this toolbar. This can
  // return false if we are in an incognito window and the extension is disabled
  // for incognito.
  bool ShouldDisplayBrowserAction(Extension* extension);

  // ExtensionToolbarModel::Observer implementation.
  virtual void BrowserActionAdded(Extension* extension, int index);
  virtual void BrowserActionRemoved(Extension* extension);
  virtual void BrowserActionMoved(Extension* extension, int index);

  // Called by the BrowserActionButton in response to drag-begin.
  void DragStarted(BrowserActionButton* button, GdkDragContext* drag_context);

  // Sets the width of the button area of the toolbar to |new_width|, clamping
  // it to appropriate values and updating the model.
  void SetButtonHBoxWidth(int new_width);

  CHROMEGTK_CALLBACK_4(BrowserActionsToolbarGtk, gboolean, OnDragMotion,
                       GdkDragContext*, gint, gint, guint);
  CHROMEGTK_CALLBACK_1(BrowserActionsToolbarGtk, void, OnDragEnd,
                       GdkDragContext*);
  CHROMEGTK_CALLBACK_2(BrowserActionsToolbarGtk, gboolean, OnDragFailed,
                       GdkDragContext*, GtkDragResult);
  CHROMEGTK_CALLBACK_1(BrowserActionsToolbarGtk, void, OnHierarchyChanged,
                       GtkWidget*);
  CHROMEGTK_CALLBACK_1(BrowserActionsToolbarGtk, void, OnSetFocus, GtkWidget*);
  CHROMEGTK_CALLBACK_1(BrowserActionsToolbarGtk, gboolean,
                       OnGripperMotionNotify, GdkEventMotion*);
  CHROMEGTK_CALLBACK_1(BrowserActionsToolbarGtk, gboolean, OnGripperExpose,
                       GdkEventExpose*);

  Browser* browser_;

  Profile* profile_;
  GtkThemeProvider* theme_provider_;

  ExtensionToolbarModel* model_;

  // Contains the drag gripper, browser action buttons, and overflow chevron.
  OwnedWidgetGtk hbox_;

  // Contains the browser action buttons.
  GtkWidget* button_hbox_;

  OverflowButton overflow_button_;

  // The button that is currently being dragged, or NULL.
  BrowserActionButton* drag_button_;

  // The new position of the button in the drag, or -1.
  int drop_index_;

  // Map from extension ID to BrowserActionButton, which is a wrapper for
  // a chrome button and related functionality. There should be one entry
  // for every extension that has a browser action.
  typedef std::map<std::string, linked_ptr<BrowserActionButton> >
      ExtensionButtonMap;
  ExtensionButtonMap extension_button_map_;

  ScopedRunnableMethodFactory<BrowserActionsToolbarGtk> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionsToolbarGtk);
};

#endif  // CHROME_BROWSER_GTK_BROWSER_ACTIONS_TOOLBAR_GTK_H_
