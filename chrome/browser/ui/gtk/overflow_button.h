// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_OVERFLOW_BUTTON_H_
#define CHROME_BROWSER_UI_GTK_OVERFLOW_BUTTON_H_
#pragma once

#include "base/compiler_specific.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/owned_widget_gtk.h"

typedef struct _GtkWidget GtkWidget;
class Profile;

// An overflow chevron button. The button itself is a plain gtk_chrome_button,
// and this class handles theming it.
class OverflowButton : public content::NotificationObserver {
 public:
  explicit OverflowButton(Profile* profile);
  virtual ~OverflowButton();

  GtkWidget* widget() { return widget_.get(); }

 private:
  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  ui::OwnedWidgetGtk widget_;

  Profile* profile_;

  content::NotificationRegistrar registrar_;
};

#endif  // CHROME_BROWSER_UI_GTK_OVERFLOW_BUTTON_H_
