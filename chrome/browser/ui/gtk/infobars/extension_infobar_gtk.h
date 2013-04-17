// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_INFOBARS_EXTENSION_INFOBAR_GTK_H_
#define CHROME_BROWSER_UI_GTK_INFOBARS_EXTENSION_INFOBAR_GTK_H_

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_infobar_delegate.h"
#include "chrome/browser/ui/gtk/extensions/extension_view_gtk.h"
#include "chrome/browser/ui/gtk/infobars/infobar_gtk.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "ui/gfx/gtk_util.h"

class ExtensionContextMenuModel;
class ExtensionViewGtk;
class MenuGtk;

class ExtensionInfoBarGtk : public InfoBarGtk,
                            public MenuGtk::Delegate,
                            public ExtensionInfoBarDelegate::DelegateObserver {
 public:
  ExtensionInfoBarGtk(InfoBarService* owner,
                      ExtensionInfoBarDelegate* delegate);
  virtual ~ExtensionInfoBarGtk();

  // Overridden from InfoBar (through InfoBarGtk):
  virtual void PlatformSpecificHide(bool animate) OVERRIDE;

  // Overridden from InfoBarGtk:
  virtual void GetTopColor(InfoBarDelegate::Type type,
                           double* r, double* g, double* b) OVERRIDE;
  virtual void GetBottomColor(InfoBarDelegate::Type type,
                              double* r, double* g, double* b) OVERRIDE;

  // Overridden from MenuGtk::Delegate:
  virtual void StoppedShowing() OVERRIDE;

  // Overridden from ExtensionInfoBarDelegate::DelegateObserver:
  virtual void OnDelegateDeleted() OVERRIDE;

 private:
  // Build the widgets of the Infobar.
  void BuildWidgets();

  void OnImageLoaded(const gfx::Image& image);

  // Looks at the window the infobar is in and gets the browser. Can return
  // NULL if we aren't attached.
  Browser* GetBrowser();

  // Returns the context menu model for this extension. Can be NULL if
  // extension context menus are disabled.
  ExtensionContextMenuModel* BuildMenuModel();

  CHROMEGTK_CALLBACK_1(ExtensionInfoBarGtk, void, OnSizeAllocate,
                       GtkAllocation*);

  CHROMEGTK_CALLBACK_1(ExtensionInfoBarGtk, gboolean, OnButtonPress,
                       GdkEventButton*);

  CHROMEGTK_CALLBACK_1(ExtensionInfoBarGtk, gboolean, OnExpose,
                       GdkEventExpose*);

  ExtensionInfoBarDelegate* delegate_;

  ExtensionViewGtk* view_;

  // The button that activates the extension popup menu. Non-NULL if the
  // extension shows configure context menus and a dropdown menu should be used
  // in place of the icon. If set, parents |icon_|.
  GtkWidget* button_;

  // The GtkImage that shows the extension icon. If a dropdown menu should be
  // used, it's put inside |button_|, otherwise it's put directly in the hbox
  // containing the infobar element. Composed in OnImageLoaded().
  GtkWidget* icon_;

  // An alignment with one pixel of bottom padding. This is set so the |view_|
  // doesn't overlap the bottom separator. This also makes it more convenient
  // to reattach the view since the alignment_ will have the |hbox_| packing
  // child properties. Reparenting becomes easier too.
  GtkWidget* alignment_;

  // The model for the current menu displayed.
  scoped_refptr<ExtensionContextMenuModel> context_menu_model_;

  base::WeakPtrFactory<ExtensionInfoBarGtk> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInfoBarGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_INFOBARS_EXTENSION_INFOBAR_GTK_H_
