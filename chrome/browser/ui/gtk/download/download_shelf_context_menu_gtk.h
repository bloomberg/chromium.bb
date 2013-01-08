// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_GTK_H_
#define CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_GTK_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/download/download_shelf_context_menu.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"

class DownloadItemGtk;

class DownloadShelfContextMenuGtk : public DownloadShelfContextMenu,
                                    public MenuGtk::Delegate {
 public:
  DownloadShelfContextMenuGtk(DownloadItemGtk* download_item,
                              content::PageNavigator* navigator);
  virtual ~DownloadShelfContextMenuGtk();

  void Popup(GtkWidget* widget, GdkEventButton* event);

 private:
  // MenuGtk::Delegate:
  virtual void StoppedShowing() OVERRIDE;
  virtual GtkWidget* GetImageForCommandId(int command_id) const OVERRIDE;

  // The menu we show on Popup(). We keep a pointer to it for a couple reasons:
  //  * we don't want to have to recreate the menu every time it's popped up.
  //  * we have to keep it in scope for longer than the duration of Popup(), or
  //    completing the user-selected action races against the menu's
  //    destruction.
  scoped_ptr<MenuGtk> menu_;

  // The download item that created us.
  DownloadItemGtk* download_item_gtk_;

  DISALLOW_COPY_AND_ASSIGN(DownloadShelfContextMenuGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_DOWNLOAD_DOWNLOAD_SHELF_CONTEXT_MENU_GTK_H_
