// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_OVERFLOW_BUTTON_H_
#define CHROME_BROWSER_UI_GTK_OVERFLOW_BUTTON_H_
#pragma once

#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/gtk/owned_widget_gtk.h"

typedef struct _GtkWidget GtkWidget;
class Profile;

// An overflow chevron button. The button itself is a plain gtk_chrome_button,
// and this class handles theming it.
class OverflowButton : public NotificationObserver {
 public:
  explicit OverflowButton(Profile* profile);
  virtual ~OverflowButton();

  GtkWidget* widget() { return widget_.get(); }

 private:
  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  ui::OwnedWidgetGtk widget_;

  Profile* profile_;

  NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_UI_GTK_OVERFLOW_BUTTON_H_
