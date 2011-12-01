// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_CUSTOM_BUTTON_H_
#define CHROME_BROWSER_UI_GTK_CUSTOM_BUTTON_H_
#pragma once

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"
#include "ui/gfx/rect.h"

class CairoCachedSurface;
class GtkThemeService;
class SkBitmap;

// These classes implement two kinds of custom-drawn buttons.  They're
// used on the toolbar and the bookmarks bar.

// CustomDrawButtonBase provides the base for building a custom drawn button.
// It handles managing the pixbufs containing all the static images used to draw
// the button.  It also manages painting these pixbufs.
class CustomDrawButtonBase : public content::NotificationObserver {
 public:
  // If the images come from ResourceBundle rather than the theme provider,
  // pass in NULL for |theme_provider|.
  CustomDrawButtonBase(GtkThemeService* theme_provider,
                       int normal_id,
                       int pressed_id,
                       int hover_id,
                       int disabled_id);

  virtual ~CustomDrawButtonBase();

  // Flip the image horizontally. Not to be used for RTL/LTR reasons. (In RTL
  // mode, this will unflip the image.)
  void set_flipped(bool flipped) { flipped_ = flipped; }

  // Returns the dimensions of the first surface.
  int Width() const;
  int Height() const;

  gboolean OnExpose(GtkWidget* widget, GdkEventExpose* e, gdouble hover_state);

  void set_paint_override(int state) { paint_override_ = state; }
  int paint_override() const { return paint_override_; }

  // Set the background details.
  void SetBackground(SkColor color, SkBitmap* image, SkBitmap* mask);

  // Provide content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Get the CairoCachedSurface from |surfaces_| for |state|.
  CairoCachedSurface* PixbufForState(int state);

  // We store one surface for each possible state of the button;
  // INSENSITIVE is the last available state;
  scoped_ptr<CairoCachedSurface> surfaces_[GTK_STATE_INSENSITIVE + 1];

  // The background image.
  scoped_ptr<CairoCachedSurface> background_image_;

  // If non-negative, the state to paint the button.
  int paint_override_;

  // We need to remember the image ids that the user passes in and the theme
  // provider so we can reload images if the user changes theme.
  int normal_id_;
  int pressed_id_;
  int hover_id_;
  int disabled_id_;
  GtkThemeService* theme_service_;

  // Whether the button is flipped horizontally. Not used for RTL (we get
  // flipped versions from the theme provider). Used for the flipped window
  // buttons.
  bool flipped_;

  // Used to listen for theme change notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(CustomDrawButtonBase);
};

// CustomDrawHoverController is a convenience class that eases the common task
// of controlling the hover state of a button. The "hover state" refers to the
// percent opacity of a button's PRELIGHT. The PRELIGHT is animated such that
// when a user moves a mouse over a button the PRELIGHT fades in.
class CustomDrawHoverController : public ui::AnimationDelegate {
 public:
  explicit CustomDrawHoverController(GtkWidget* widget);
  CustomDrawHoverController();

  virtual ~CustomDrawHoverController();

  void Init(GtkWidget* widget);

  double GetCurrentValue() {
    return slide_animation_.GetCurrentValue();
  }

 private:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  CHROMEGTK_CALLBACK_1(CustomDrawHoverController, gboolean, OnEnter,
                       GdkEventCrossing*);
  CHROMEGTK_CALLBACK_1(CustomDrawHoverController, gboolean, OnLeave,
                       GdkEventCrossing*);

  ui::SlideAnimation slide_animation_;
  GtkWidget* widget_;
};

// CustomDrawButton is a plain button where all its various states are drawn
// with static images. In GTK rendering mode, it will show the standard button
// with GTK |stock_id|.
class CustomDrawButton : public content::NotificationObserver {
 public:
  // The constructor takes 4 resource ids.  If a resource doesn't exist for a
  // button, pass in 0.
  CustomDrawButton(int normal_id,
                   int pressed_id,
                   int hover_id,
                   int disabled_id);

  // Same as above, but uses themed (and possibly tinted) images. |stock_id| and
  // |stock_size| are used for GTK+ theme mode.
  CustomDrawButton(GtkThemeService* theme_provider,
                   int normal_id,
                   int pressed_id,
                   int hover_id,
                   int disabled_id,
                   const char* stock_id,
                   GtkIconSize stock_size);

  // As above, but uses an arbitrary GtkImage rather than a stock icon. This
  // constructor takes ownership of |native_widget|.
  CustomDrawButton(GtkThemeService* theme_provider,
                   int normal_id,
                   int pressed_id,
                   int hover_id,
                   int disabled_id,
                   GtkWidget* native_widget);

  virtual ~CustomDrawButton();

  void Init();

  // Make this CustomDrawButton always use the chrome style rendering; it will
  // never render gtk-like.
  void ForceChromeTheme();

  // Flip the image horizontally. Not to be used for RTL/LTR reasons. (In RTL
  // mode, this will unflip the image.)
  void set_flipped(bool flipped) { button_base_.set_flipped(flipped); }

  GtkWidget* widget() const { return widget_.get(); }

  // Returns the width of the widget's allocation.
  int WidgetWidth() const;

  // Returns the dimensions of the first surface.
  int SurfaceWidth() const;
  int SurfaceHeight() const;

  // Set the state to draw. We will paint the widget as if it were in this
  // state.
  void SetPaintOverride(GtkStateType state);

  // Resume normal drawing of the widget's state.
  void UnsetPaintOverride();

  // Set the background details.
  void SetBackground(SkColor color, SkBitmap* image, SkBitmap* mask);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Returns a standard close button. Pass a |theme_provider| to use Gtk icons
  // in Gtk rendering mode.
  static CustomDrawButton* CloseButton(GtkThemeService* theme_provider);

 private:
  // Sets the button to themed or not.
  void SetBrowserTheme();

  // Whether to use the GTK+ theme. For this to be true, we have to be in GTK+
  // theme mode and we must have a valid stock icon resource.
  bool UseGtkTheme();

  // Callback for custom button expose, used to draw the custom graphics.
  CHROMEGTK_CALLBACK_1(CustomDrawButton, gboolean, OnCustomExpose,
                       GdkEventExpose*);

  // The actual button widget.
  ui::OwnedWidgetGtk widget_;

  CustomDrawButtonBase button_base_;

  CustomDrawHoverController hover_controller_;

  // The widget to use when we are displaying in GTK+ theme mode.
  ui::OwnedWidgetGtk native_widget_;

  // Our theme provider.
  GtkThemeService* theme_service_;

  // True if we should never do gtk rendering.
  bool forcing_chrome_theme_;

  // Used to listen for theme change notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(CustomDrawButton);
};

#endif  // CHROME_BROWSER_UI_GTK_CUSTOM_BUTTON_H_
