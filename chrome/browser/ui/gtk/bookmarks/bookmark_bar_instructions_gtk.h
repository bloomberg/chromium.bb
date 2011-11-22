// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_BAR_INSTRUCTIONS_GTK_H_
#define CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_BAR_INSTRUCTIONS_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"

typedef struct _GtkWidget GtkWidget;
class GtkThemeService;
class Profile;

class BookmarkBarInstructionsGtk : public content::NotificationObserver {
 public:
  // The delegate is notified once the user clicks on the link to import
  // bookmarks.
  class Delegate {
   public:
    virtual void ShowImportDialog() = 0;

   protected:
    virtual ~Delegate() {}
  };

  BookmarkBarInstructionsGtk(Delegate* delegate, Profile* profile);

  // Get the native widget.
  GtkWidget* widget() const { return instructions_hbox_; }

 private:
  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  CHROMEGTK_CALLBACK_0(BookmarkBarInstructionsGtk, void, OnButtonClick);

  // Sets the correct color for |instructions_label_| and |instructions_link_|.
  void UpdateColors();

  Delegate* delegate_;

  Profile* profile_;

  GtkThemeService* theme_service_;

  // HBox that holds the the label and link of bookmark bar import promotion.
  GtkWidget* instructions_hbox_;
  GtkWidget* instructions_label_;
  GtkWidget* instructions_link_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBarInstructionsGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_BAR_INSTRUCTIONS_GTK_H_
