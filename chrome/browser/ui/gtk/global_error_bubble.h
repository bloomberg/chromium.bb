// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_GLOBAL_ERROR_BUBBLE_H_
#define CHROME_BROWSER_UI_GTK_GLOBAL_ERROR_BUBBLE_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkWidget GtkWidget;

class GlobalError;
class Profile;

class GlobalErrorBubble : public BubbleDelegateGtk,
                          public content::NotificationObserver {
 public:
  GlobalErrorBubble(Profile* profile, GlobalError* error, GtkWidget* anchor);
  virtual ~GlobalErrorBubble();

  // BubbleDelegateGtk implementation.
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

 private:
  CHROMEGTK_CALLBACK_0(GlobalErrorBubble, void, OnDestroy);
  CHROMEGTK_CALLBACK_0(GlobalErrorBubble, void, OnAcceptButton);
  CHROMEGTK_CALLBACK_0(GlobalErrorBubble, void, OnCancelButton);

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  BubbleGtk* bubble_;
  // Weak reference to the GlobalError instance the bubble is shown for. Is
  // reset to |NULL| if instance is removed from GlobalErrorService while
  // the bubble is still showing.
  GlobalError* error_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(GlobalErrorBubble);
};

#endif  // CHROME_BROWSER_UI_GTK_GLOBAL_ERROR_BUBBLE_H_
