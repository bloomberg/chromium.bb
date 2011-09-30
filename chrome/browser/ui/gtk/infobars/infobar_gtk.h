// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_INFOBARS_INFOBAR_GTK_H_
#define CHROME_BROWSER_UI_GTK_INFOBARS_INFOBAR_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/infobars/infobar.h"
#include "chrome/browser/infobars/infobar_delegate.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"

class CustomDrawButton;
class GtkThemeService;
class MenuGtk;

namespace ui {
class GtkSignalRegistrar;
class MenuModel;
}

class InfoBarGtk : public InfoBar,
                   public NotificationObserver {
 public:
  InfoBarGtk(InfoBarTabHelper* owner, InfoBarDelegate* delegate);
  virtual ~InfoBarGtk();

  // Get the top level native GTK widget for this infobar.
  GtkWidget* widget();

  GdkColor GetBorderColor() const;

  // Returns the target height of the infobar if the bar is animating,
  // otherwise 0. We care about this number since we want to prevent
  // unnecessary renderer repaints while animating.
  int AnimatingHeight() const;

  // Conversion from cairo colors to SkColor.
  typedef void (InfoBarGtk::*ColorGetter)(InfoBarDelegate::Type,
                                          double* r, double* g, double* b);
  SkColor ConvertGetColor(ColorGetter getter);

  // Retrieves the component colors for the infobar's background
  // gradient. (This varies by infobars and can be animated to change).
  virtual void GetTopColor(InfoBarDelegate::Type type,
                           double* r, double* g, double* b);
  virtual void GetBottomColor(InfoBarDelegate::Type type,
                              double* r, double* g, double* b);

 protected:
  // Spacing after message (and before buttons).
  static const int kEndOfLabelSpacing;

  // Returns the signal registrar for this infobar. All signals representing
  // user actions on visible widgets must go through this registrar!
  ui::GtkSignalRegistrar* Signals();

  // Creates a label with the appropriate font and color for the current
  // gtk-theme state. It is InfoBarGtk's responsibility to observe browser
  // theme changes and update the label's state.
  GtkWidget* CreateLabel(const std::string& text);

  // Creates a link button with the appropriate current gtk-theme state.
  GtkWidget* CreateLinkButton(const std::string& text);

  // Adds |display_text| to the infobar. If |link_text| is not empty, it is
  // rendered as a hyperlink and inserted into |display_text| at |link_offset|,
  // or right aligned in the infobar if |link_offset| is |npos|. If a link is
  // supplied, |link_callback| must not be null. It will be invoked on click.
  void AddLabelWithInlineLink(const string16& display_text,
                              const string16& link_text,
                              size_t link_offset,
                              GCallback callback);

  // Shows the menu with |model| with the context of |sender|. InfobarGtk takes
  // ownership of the model.
  void ShowMenuWithModel(GtkWidget* sender,
                         MenuGtk::Delegate* delegate,
                         ui::MenuModel* model);

  // InfoBar:
  virtual void PlatformSpecificShow(bool animate) OVERRIDE;
  virtual void PlatformSpecificOnCloseSoon() OVERRIDE;
  virtual void PlatformSpecificOnHeightsRecalculated() OVERRIDE;

  // NotificationObserver:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // The second highest widget in the hierarchy (after the |widget_|).
  GtkWidget* bg_box_;

  // The hbox that holds infobar elements (button, text, icon, etc.).
  GtkWidget* hbox_;

  // The x that closes the bar.
  scoped_ptr<CustomDrawButton> close_button_;

  // The theme provider, used for getting border colors.
  GtkThemeService* theme_service_;

  NotificationRegistrar registrar_;

 private:
  CHROMEGTK_CALLBACK_0(InfoBarGtk, void, OnCloseButton);
  CHROMEGTK_CALLBACK_1(InfoBarGtk, gboolean, OnBackgroundExpose,
                       GdkEventExpose*);

  CHROMEGTK_CALLBACK_2(InfoBarGtk, void, OnChildSizeRequest, GtkWidget*,
                       GtkRequisition*);

  void UpdateBorderColor();

  // A GtkExpandedContainer that contains |bg_box_| so we can varry the height
  // of the infobar.
  ui::OwnedWidgetGtk widget_;

  // A list of signals which we clear out once we're closing.
  scoped_ptr<ui::GtkSignalRegistrar> signals_;

  // The current menu displayed. Can be null. We own this on the base class so
  // we can cancel the menu while we're closing.
  scoped_ptr<ui::MenuModel> menu_model_;
  scoped_ptr<MenuGtk> menu_;

  DISALLOW_COPY_AND_ASSIGN(InfoBarGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_INFOBARS_INFOBAR_GTK_H_
