// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the GTK implementation of the bookmark bubble, the dialog box
// presented to create or edit a bookmark.  There can only ever be a single
// bubble open, so the class presents only static methods, and handles the
// singleton behavior for you.  It also handles the object and widget
// lifetimes, destroying everything and possibly committing any changes when
// the bubble is closed.

#ifndef CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_BUBBLE_GTK_H_
#define CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_BUBBLE_GTK_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/ui/gtk/info_bubble_gtk.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "ui/base/gtk/gtk_signal.h"

class BookmarkNode;
class Profile;
class RecentlyUsedFoldersComboModel;

typedef struct _GtkWidget GtkWidget;
typedef struct _GParamSpec GParamSpec;

class BookmarkBubbleGtk : public InfoBubbleGtkDelegate,
                          public NotificationObserver {
 public:
  // Shows the bookmark bubble, pointing at |anchor_widget|.
  static void Show(GtkWidget* anchor_widget,
                   Profile* profile,
                   const GURL& url,
                   bool newly_bookmarked);

  // Implements the InfoBubbleGtkDelegate.  We are notified when the bubble
  // is about to be closed, so we have a chance to save any state / input in
  // our widgets before they are destroyed.
  virtual void InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                 bool closed_by_escape);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  BookmarkBubbleGtk(GtkWidget* anchor,
                    Profile* profile,
                    const GURL& url,
                    bool newly_bookmarked);
  ~BookmarkBubbleGtk();

  // Notified when |content_| is destroyed so we can delete our instance.
  CHROMEGTK_CALLBACK_0(BookmarkBubbleGtk, void, OnDestroy);
  CHROMEGTK_CALLBACK_0(BookmarkBubbleGtk, void, OnNameActivate);
  CHROMEGTK_CALLBACK_0(BookmarkBubbleGtk, void, OnFolderChanged);
  CHROMEGTK_CALLBACK_1(BookmarkBubbleGtk, void, OnFolderPopupShown,
                       GParamSpec*);
  CHROMEGTK_CALLBACK_0(BookmarkBubbleGtk, void, OnEditClicked);
  CHROMEGTK_CALLBACK_0(BookmarkBubbleGtk, void, OnCloseClicked);
  CHROMEGTK_CALLBACK_0(BookmarkBubbleGtk, void, OnRemoveClicked);

  // Update the bookmark with any edits that have been made.
  void ApplyEdits();

  // Open the bookmark editor for the current url and close the bubble.
  void ShowEditor();

  // Return the UTF8 encoded title for the current |url_|.
  std::string GetTitle();

  void InitFolderComboModel();

  // The URL of the bookmark.
  GURL url_;
  // Our current profile (used to access the bookmark system).
  Profile* profile_;

  // Provides colors and stuff.
  GtkThemeProvider* theme_provider_;

  // The widget relative to which we are positioned.
  GtkWidget* anchor_;

  // We let the InfoBubble own our content, and then we delete ourself
  // when the widget is destroyed (when the InfoBubble is destroyed).
  GtkWidget* content_;

  // The button that removes the bookmark.
  GtkWidget* remove_button_;

  // The various labels in the interface. We keep track of them for theme
  // changes.
  std::vector<GtkWidget*> labels_;

  // The GtkEntry for editing the bookmark name / title.
  GtkWidget* name_entry_;

  // The combo box for selecting the bookmark folder.
  GtkWidget* folder_combo_;
  scoped_ptr<RecentlyUsedFoldersComboModel> folder_combo_model_;

  InfoBubbleGtk* bubble_;

  // We need to push some things on the back of the message loop, so we have
  // a factory attached to our instance to manage task lifetimes.
  ScopedRunnableMethodFactory<BookmarkBubbleGtk> factory_;

  // Whether the bubble is creating or editing an existing bookmark.
  bool newly_bookmarked_;
  // When closing the window, whether we should update or remove the bookmark.
  bool apply_edits_;
  bool remove_bookmark_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_BUBBLE_GTK_H_
