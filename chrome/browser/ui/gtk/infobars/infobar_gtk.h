// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_INFOBARS_INFOBAR_GTK_H_
#define CHROME_BROWSER_UI_GTK_INFOBARS_INFOBAR_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/ui/gtk/infobars/infobar_arrow_model.h"
#include "chrome/browser/ui/gtk/owned_widget_gtk.h"
#include "chrome/browser/ui/gtk/slide_animator_gtk.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "ui/base/gtk/gtk_signal.h"

class CustomDrawButton;
class GtkThemeProvider;
class InfoBarContainerGtk;
class InfoBarDelegate;

class InfoBar : public SlideAnimatorGtk::Delegate,
                public NotificationObserver,
                public InfoBarArrowModel::Observer {
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

  // Returns true if the infobar is showing the close animation.
  bool IsClosing();

  void SetThemeProvider(GtkThemeProvider* theme_provider);

  // Show an arrow that originates from another infobar (i.e. a bar was added
  // below this one). If |other| is NULL, stop showing the arrow.
  void ShowArrowFor(InfoBar* other, bool animate);

  // InfoBarArrowModel::Observer implementation.
  virtual void PaintStateChanged();

  // SlideAnimatorGtk::Delegate implementation.
  virtual void Closed();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Retrieves the component colors for the infobar's background
  // gradient. (This varies by infobars and can be animated to change).
  virtual void GetTopColor(InfoBarDelegate::Type type,
                           double* r, double* g, double *b);
  virtual void GetBottomColor(InfoBarDelegate::Type type,
                              double* r, double* g, double *b);

  // The total height of the info bar.
  static const int kInfoBarHeight;

 protected:
  // Spacing after message (and before buttons).
  static const int kEndOfLabelSpacing;
  // Spacing between buttons.
  static const int kButtonButtonSpacing;

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

  // The top level widget of the infobar.
  scoped_ptr<SlideAnimatorGtk> slide_widget_;

  // The second highest widget in the hierarchy (after the slide widget).
  GtkWidget* bg_box_;

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

  // The model that tracks the paint state of the arrow for the infobar
  // below this one (if it exists).
  InfoBarArrowModel arrow_model_;

  NotificationRegistrar registrar_;

 private:
  CHROMEGTK_CALLBACK_0(InfoBar, void, OnCloseButton);
  CHROMEGTK_CALLBACK_1(InfoBar, gboolean, OnBackgroundExpose, GdkEventExpose*);

  void UpdateBorderColor();

  DISALLOW_COPY_AND_ASSIGN(InfoBar);
};

#endif  // CHROME_BROWSER_UI_GTK_INFOBARS_INFOBAR_GTK_H_
