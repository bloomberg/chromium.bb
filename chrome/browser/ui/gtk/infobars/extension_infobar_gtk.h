// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_INFOBARS_EXTENSION_INFOBAR_GTK_H_
#define CHROME_BROWSER_UI_GTK_INFOBARS_EXTENSION_INFOBAR_GTK_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/ui/gtk/extensions/extension_view_gtk.h"
#include "chrome/browser/ui/gtk/infobars/infobar_gtk.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "ui/gfx/gtk_util.h"

class ExtensionResource;
class ExtensionViewGtk;
class MenuGtk;

class ExtensionInfoBarGtk : public InfoBarGtk,
                            public ImageLoadingTracker::Observer,
                            public MenuGtk::Delegate {
 public:
  ExtensionInfoBarGtk(InfoBarTabHelper* owner,
                      ExtensionInfoBarDelegate* delegate);
  virtual ~ExtensionInfoBarGtk();

  // Overridden from InfoBar (through InfoBarGtk):
  virtual void PlatformSpecificHide(bool animate) OVERRIDE;

  // Overridden from InfoBarGtk:
  virtual void GetTopColor(InfoBarDelegate::Type type,
                           double* r, double* g, double* b) OVERRIDE;
  virtual void GetBottomColor(InfoBarDelegate::Type type,
                              double* r, double* g, double* b) OVERRIDE;

  // Overridden from ImageLoadingTracker::Observer:
  virtual void OnImageLoaded(
      SkBitmap* image, const ExtensionResource& resource, int index) OVERRIDE;

  // Overridden from MenuGtk::Delegate:
  virtual void StoppedShowing() OVERRIDE;

 private:
  // Build the widgets of the Infobar.
  void BuildWidgets();

  // Looks at the window the infobar is in and gets the browser. Can return
  // NULL if we aren't attached.
  Browser* GetBrowser();

  // Returns the context menu model for this extension. Can be NULL if
  // extension context menus are disabled.
  ui::MenuModel* BuildMenuModel();

  CHROMEGTK_CALLBACK_1(ExtensionInfoBarGtk, void, OnSizeAllocate,
                       GtkAllocation*);

  CHROMEGTK_CALLBACK_1(ExtensionInfoBarGtk, gboolean, OnButtonPress,
                       GdkEventButton*);

  CHROMEGTK_CALLBACK_1(ExtensionInfoBarGtk, gboolean, OnExpose,
                       GdkEventExpose*);

  ImageLoadingTracker tracker_;

  ExtensionInfoBarDelegate* delegate_;

  ExtensionViewGtk* view_;

  // The button that activates the extension popup menu. Parent of |icon_|.
  GtkWidget* button_;

  // The GtkImage that is inside of |button_|, composed in OnImageLoaded().
  GtkWidget* icon_;

  // An alignment with one pixel of bottom padding. This is set so the |view_|
  // doesn't overlap the bottom separator. This also makes it more convenient
  // to reattach the view since the alignment_ will have the |hbox_| packing
  // child properties. Reparenting becomes easier too.
  GtkWidget* alignment_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInfoBarGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_INFOBARS_EXTENSION_INFOBAR_GTK_H_
