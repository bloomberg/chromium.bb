// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_THROBBER_GTK_H_
#define CHROME_BROWSER_UI_GTK_THROBBER_GTK_H_

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/gtk/owned_widget_gtk.h"
#include "ui/gfx/image/image.h"

class GtkThemeService;
class SkBitmap;

// An animating throbber.
class ThrobberGtk : public ui::AnimationDelegate,
                    public content::NotificationObserver {
 public:
  // |theme_service| must not be NULL.
  explicit ThrobberGtk(GtkThemeService* theme_service);
  virtual ~ThrobberGtk();

  // Start or stop the throbbing animation.
  void Start();
  void Stop();

  GtkWidget* widget() const { return widget_.get(); }

  // ui::AnimationDelegate implementation.
  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE;
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  CHROMEGTK_CALLBACK_1(ThrobberGtk, gboolean, OnExpose, GdkEventExpose*);

  // Initialize the widget.
  void Init();

  // Load the animation frames from IDR_THROBBER.
  void LoadFrames();

  content::NotificationRegistrar registrar_;

  // The theme service. Weak pointer.
  GtkThemeService* theme_service_;

  // The actual GtkWidget.
  ui::OwnedWidgetGtk widget_;

  // A linear animation over the loaded frames.
  ui::SlideAnimation animation_;

  // The image containing the throbber frames.
  gfx::Image frames_;

  // The number of frames in |frames_|.
  int num_frames_;

  DISALLOW_COPY_AND_ASSIGN(ThrobberGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_THROBBER_GTK_H_
