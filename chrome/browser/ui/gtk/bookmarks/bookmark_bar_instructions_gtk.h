// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_BAR_INSTRUCTIONS_GTK_H_
#define CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_BAR_INSTRUCTIONS_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkWidget GtkWidget;
class GtkThemeProvider;
class Profile;

class BookmarkBarInstructionsGtk : public NotificationObserver {
 public:
  // The delegate is notified once the user clicks on the link to import
  // bookmarks.
  class Delegate {
   public:
    virtual void ShowImportDialog() = 0;

   protected:
    virtual ~Delegate() {}
  };

  explicit BookmarkBarInstructionsGtk(Delegate* delegate, Profile* profile);

  // Get the native widget.
  GtkWidget* widget() const { return instructions_hbox_; }

 private:
  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  CHROMEGTK_CALLBACK_0(BookmarkBarInstructionsGtk, void, OnButtonClick);

  // Sets the correct color for |instructions_label_| and |instructions_link_|.
  void UpdateColors();

  Delegate* delegate_;

  Profile* profile_;

  GtkThemeProvider* theme_provider_;

  // HBox that holds the the label and link of bookmark bar import promotion.
  GtkWidget* instructions_hbox_;
  GtkWidget* instructions_label_;
  GtkWidget* instructions_link_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarInstructionsGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_BAR_INSTRUCTIONS_GTK_H_
