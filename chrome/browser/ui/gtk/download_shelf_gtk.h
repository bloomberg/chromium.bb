// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_DOWNLOAD_SHELF_GTK_H_
#define CHROME_BROWSER_UI_GTK_DOWNLOAD_SHELF_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/ui/gtk/owned_widget_gtk.h"
#include "chrome/browser/ui/gtk/slide_animator_gtk.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "gfx/native_widget_types.h"
#include "ui/base/gtk/gtk_signal.h"

class BaseDownloadItemModel;
class Browser;
class CustomDrawButton;
class DownloadItemGtk;
class GtkThemeProvider;
class SlideAnimatorGtk;

class DownloadShelfGtk : public DownloadShelf,
                         public NotificationObserver,
                         public SlideAnimatorGtk::Delegate {
 public:
  explicit DownloadShelfGtk(Browser* browser, gfx::NativeView view);

  ~DownloadShelfGtk();

  // DownloadShelf implementation.
  virtual void AddDownload(BaseDownloadItemModel* download_model);
  virtual bool IsShowing() const;
  virtual bool IsClosing() const;
  virtual void Show();
  virtual void Close();
  virtual Browser* browser() const;

  // SlideAnimatorGtk::Delegate implementation.
  virtual void Closed();

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Returns the current height of the shelf.
  int GetHeight() const;

 private:
  // Remove |download_item| from the download shelf and delete it.
  void RemoveDownloadItem(DownloadItemGtk* download_item);

  // Get the hbox download items ought to pack themselves into.
  GtkWidget* GetHBox() const;

  // Show more hidden download items if there is enough space in the shelf.
  // It's called when a download item is removed from the shelf or an item's
  // size is changed.
  void MaybeShowMoreDownloadItems();

  CHROMEGTK_CALLBACK_0(DownloadShelfGtk, void, OnButtonClick);

  // The browser that owns this download shelf.
  Browser* browser_;

  // The top level widget of the shelf.
  scoped_ptr<SlideAnimatorGtk> slide_widget_;

  // |items_hbox_| holds the download items.
  OwnedWidgetGtk items_hbox_;

  // |shelf_| is the second highest level widget. See the constructor
  // for an explanation of the widget layout.
  OwnedWidgetGtk shelf_;

  // Top level event box which draws the one pixel border.
  GtkWidget* top_border_;

  // A GtkEventBox which we color.
  GtkWidget* padding_bg_;

  // The "Show all downloads..." link.
  GtkWidget* link_button_;

  // The 'x' that the user can press to hide the download shelf.
  scoped_ptr<CustomDrawButton> close_button_;

  // Keeps track of our current hide/show state.
  bool is_showing_;

  // The download items we have added to our shelf.
  std::vector<DownloadItemGtk*> download_items_;

  // Gives us our colors and theme information.
  GtkThemeProvider* theme_provider_;

  NotificationRegistrar registrar_;

  friend class DownloadItemGtk;
};

#endif  // CHROME_BROWSER_UI_GTK_DOWNLOAD_SHELF_GTK_H_
