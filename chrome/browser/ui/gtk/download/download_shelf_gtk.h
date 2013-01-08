// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_SHELF_GTK_H_
#define CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_SHELF_GTK_H_

#include <gtk/gtk.h>

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/ui/gtk/slide_animator_gtk.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"
#include "ui/gfx/native_widget_types.h"

class Browser;
class CustomDrawButton;
class DownloadItemGtk;
class GtkThemeService;

namespace content {
class PageNavigator;
}

namespace gfx {
class Point;
}

class DownloadShelfGtk : public DownloadShelf,
                         public content::NotificationObserver,
                         public SlideAnimatorGtk::Delegate,
                         public MessageLoopForUI::Observer {
 public:
  DownloadShelfGtk(Browser* browser, gfx::NativeView view);

  virtual ~DownloadShelfGtk();

  // Retrieves the navigator for loading pages.
  content::PageNavigator* GetNavigator();

  // DownloadShelf implementation.
  virtual bool IsShowing() const OVERRIDE;
  virtual bool IsClosing() const OVERRIDE;
  virtual Browser* browser() const OVERRIDE;

  // SlideAnimatorGtk::Delegate implementation.
  virtual void Closed() OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Returns the current height of the shelf.
  int GetHeight() const;

  // MessageLoop::Observer implementation:
  virtual void WillProcessEvent(GdkEvent* event) OVERRIDE;
  virtual void DidProcessEvent(GdkEvent* event) OVERRIDE;

 protected:
  // DownloadShelf implementation.
  virtual void DoAddDownload(content::DownloadItem* download) OVERRIDE;
  virtual void DoShow() OVERRIDE;
  virtual void DoClose() OVERRIDE;

 private:
  // Remove |download_item| from the download shelf and delete it.
  void RemoveDownloadItem(DownloadItemGtk* download_item);

  // Get the hbox download items ought to pack themselves into.
  GtkWidget* GetHBox() const;

  // Show more hidden download items if there is enough space in the shelf.
  // It's called when a download item is removed from the shelf or an item's
  // size is changed.
  void MaybeShowMoreDownloadItems();

  // Checks that all download items have been opened, and sets the auto-close
  // state of the shelf if so.
  void AutoCloseIfPossible();

  // Cancels the auto-close state set by AutoCloseIfPossible, including any
  // pending close tasks that have already been posted.
  void CancelAutoClose();

  // A download item has been opened. It might be possible to automatically
  // close now.
  void ItemOpened();

  // Sets whether the shelf should automatically close.
  void SetCloseOnMouseOut(bool close);

  // Returns whether the given point is within the "zone" of the shelf, which is
  // the shelf and a band of 40 pixels on the top of it.
  bool IsCursorInShelfZone(const gfx::Point& cursor_screen_coords);

  // Synthesized enter-notify and leave-notify events for the shelf's "zone".
  void MouseLeftShelf();
  void MouseEnteredShelf();

  CHROMEGTK_CALLBACK_0(DownloadShelfGtk, void, OnButtonClick);

  // The browser that owns this download shelf.
  Browser* browser_;

  // The top level widget of the shelf.
  scoped_ptr<SlideAnimatorGtk> slide_widget_;

  // |items_hbox_| holds the download items.
  ui::OwnedWidgetGtk items_hbox_;

  // |shelf_| is the second highest level widget. See the constructor
  // for an explanation of the widget layout.
  ui::OwnedWidgetGtk shelf_;

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
  GtkThemeService* theme_service_;

  content::NotificationRegistrar registrar_;

  // True if the shelf will automatically close when the user mouses out.
  bool close_on_mouse_out_;

  // True if the mouse is within the shelf's bounds, as of the last mouse event
  // we received.
  bool mouse_in_shelf_;

  base::WeakPtrFactory<DownloadShelfGtk> weak_factory_;

  friend class DownloadItemGtk;
};

#endif  // CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_SHELF_GTK_H_
