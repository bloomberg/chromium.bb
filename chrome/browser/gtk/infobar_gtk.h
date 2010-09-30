// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_INFOBAR_GTK_H_
#define CHROME_BROWSER_GTK_INFOBAR_GTK_H_
#pragma once

#include "app/gtk_signal.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/gtk/owned_widget_gtk.h"
#include "chrome/browser/gtk/slide_animator_gtk.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class CustomDrawButton;
class GtkThemeProvider;
class InfoBarContainerGtk;
class InfoBarDelegate;

class InfoBar : public SlideAnimatorGtk::Delegate,
                public NotificationObserver {
 public:
  explicit InfoBar(InfoBarDelegate* delegate);
  virtual ~InfoBar();

  InfoBarDelegate* delegate() const { return delegate_; }

  // Get the top level native GTK widget for this infobar.
  GtkWidget* widget();

  // Set a link to the parent InfoBarContainer. This must be set before the
  // InfoBar is added to the view hierarchy.
  void set_container(InfoBarContainerGtk* container) { container_ = container; }

  // Starts animating the InfoBar open.
  void AnimateOpen();

  // Opens the InfoBar immediately.
  void Open();

  // Starts animating the InfoBar closed. It will not be closed until the
  // animation has completed, when |Close| will be called.
  void AnimateClose();

  // Closes the InfoBar immediately and removes it from its container. Notifies
  // the delegate that it has closed. The InfoBar is deleted after this function
  // is called.
  void Close();

  // Returns true if the infobar is showing the its open or close animation.
  bool IsAnimating();

  void SetThemeProvider(GtkThemeProvider* theme_provider);

  // SlideAnimatorGtk::Delegate implementation.
  virtual void Closed();

  // NotificationOPbserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  // Removes our associated InfoBarDelegate from the associated TabContents.
  // (Will lead to this InfoBar being closed).
  void RemoveInfoBar() const;

  // Adds |display_text| to the infobar. If |link_text| is not empty, it is
  // rendered as a hyperlink and inserted into |display_text| at |link_offset|,
  // or right aligned in the infobar if |link_offset| is |npos|. If a link is
  // supplied, |link_callback| must not be null. It will be invoked on click.
  void AddLabelWithInlineLink(const string16& display_text,
                              const string16& link_text,
                              size_t link_offset,
                              GCallback callback);

  // Adds |display_text| to the infobar. If |link_text| is not empty, it is
  // right aligned in the infobar.
  void AddLabelAndLink(const string16& display_text,
                       const string16& link_text,
                       GCallback callback);

  // Retrieves the component colors for the infobar's background
  // gradient. (This varies by infobars and can be animated to change).
  virtual void GetTopColor(InfoBarDelegate::Type type,
                           double* r, double* g, double *b);
  virtual void GetBottomColor(InfoBarDelegate::Type type,
                              double* r, double* g, double *b);

  // The top level widget of the infobar.
  scoped_ptr<SlideAnimatorGtk> slide_widget_;

  // The second highest level widget of the infobar.
  OwnedWidgetGtk border_bin_;

  // The hbox that holds infobar elements (button, text, icon, etc.).
  GtkWidget* hbox_;

  // The x that closes the bar.
  scoped_ptr<CustomDrawButton> close_button_;

  // The InfoBar's container
  InfoBarContainerGtk* container_;

  // The InfoBar's delegate.
  InfoBarDelegate* delegate_;

  // The theme provider, used for getting border colors.
  GtkThemeProvider* theme_provider_;

  NotificationRegistrar registrar_;

 private:
  CHROMEGTK_CALLBACK_0(InfoBar, void, OnCloseButton);
  CHROMEGTK_CALLBACK_1(InfoBar, gboolean, OnBackgroundExpose, GdkEventExpose*);

  void UpdateBorderColor();

  DISALLOW_COPY_AND_ASSIGN(InfoBar);
};

#endif  // CHROME_BROWSER_GTK_INFOBAR_GTK_H_
