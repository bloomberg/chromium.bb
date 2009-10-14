// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_EXTENSION_SHELF_GTK_H_
#define CHROME_BROWSER_GTK_EXTENSION_SHELF_GTK_H_

#include <gtk/gtk.h>

#include <set>

#include "base/scoped_ptr.h"
#include "chrome/browser/extensions/extension_shelf_model.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/owned_widget_gtk.h"

class Browser;
class BrowserWindowGtk;
class CustomContainerButton;
class NineBox;
class Profile;
struct GtkThemeProvider;

class ExtensionShelfGtk : public ExtensionShelfModelObserver,
                          public NotificationObserver {
 public:
  ExtensionShelfGtk(Profile* profile, Browser* browser);
  virtual ~ExtensionShelfGtk();

  // Change the visibility of the bookmarks bar. (Starts out hidden, per GTK's
  // default behaviour).
  void Show();
  void Hide();

  // ExtensionShelfModelObserver
  virtual void ToolstripInsertedAt(ExtensionHost* host, int index);
  virtual void ToolstripRemovingAt(ExtensionHost* host, int index);
  virtual void ToolstripMoved(ExtensionHost* host,
                              int from_index,
                              int to_index);
  virtual void ToolstripChanged(ExtensionShelfModel::iterator toolstrip);
  virtual void ExtensionShelfEmpty();
  virtual void ShelfModelReloaded();
  virtual void ShelfModelDeleting();

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  GtkWidget* widget() {
    return event_box_.get();
  }

 private:
  class Toolstrip;

  // Create the contents of the extension shelf.
  void Init(Profile* profile);

  // Lazily initialize background bitmap. Can be called many times.
  void InitBackground();

  // Determines what is our target height and sets it.
  void AdjustHeight();

  void LoadFromModel();

  Toolstrip* ToolstripAtIndex(int index);

  // GtkHBox callbacks.
  static gboolean OnHBoxExpose(GtkWidget* widget, GdkEventExpose* event,
                               ExtensionShelfGtk* window);

  Browser* browser_;

  // Top level event box which draws the one pixel border.
  GtkWidget* top_border_;

  // Contains |shelf_hbox_|. Event box exists to prevent leakage of
  // background color from the toplevel application window's GDK window.
  OwnedWidgetGtk event_box_;

  // Used to position all children.
  GtkWidget* shelf_hbox_;

  // Lazily-initialized background for toolstrips.
  scoped_ptr<SkBitmap> background_;

  GtkThemeProvider* theme_provider_;

  // The model representing the toolstrips on the shelf.
  ExtensionShelfModel* model_;

  // Set of toolstrip views which are really on the shelf.
  std::set<Toolstrip*> toolstrips_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionShelfGtk);
};

#endif  // CHROME_BROWSER_GTK_EXTENSION_SHELF_GTK_H_
