// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/base/gtk/gtk_signal.h"
#include "url/gurl.h"

class BookmarkModel;
class Profile;
class RecentlyUsedFoldersComboModel;

typedef struct _GtkWidget GtkWidget;
typedef struct _GParamSpec GParamSpec;

class BookmarkBubbleGtk : public BubbleDelegateGtk,
                          public content::NotificationObserver {
 public:
  // Shows the bookmark bubble, pointing at |anchor_widget|.
  static void Show(GtkWidget* anchor_widget,
                   Profile* profile,
                   const GURL& url,
                   bool newly_bookmarked);

  // BubbleDelegateGtk:
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  friend class BookmarkBubbleGtkBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(BookmarkBubbleGtkBrowserTest, SyncPromoSignedIn);
  FRIEND_TEST_ALL_PREFIXES(BookmarkBubbleGtkBrowserTest, SyncPromoNotSignedIn);
  FRIEND_TEST_ALL_PREFIXES(BookmarkBubbleGtkBrowserTest, SyncPromoLink);

  BookmarkBubbleGtk(GtkWidget* anchor,
                    Profile* profile,
                    const GURL& url,
                    bool newly_bookmarked);
  virtual ~BookmarkBubbleGtk();

  // Notified when the content is destroyed so we can delete our instance.
  CHROMEGTK_CALLBACK_0(BookmarkBubbleGtk, void, OnDestroy);
  CHROMEGTK_CALLBACK_0(BookmarkBubbleGtk, void, OnNameActivate);
  CHROMEGTK_CALLBACK_0(BookmarkBubbleGtk, void, OnFolderChanged);
  CHROMEGTK_CALLBACK_0(BookmarkBubbleGtk, void, OnEditClicked);
  CHROMEGTK_CALLBACK_0(BookmarkBubbleGtk, void, OnCloseClicked);
  CHROMEGTK_CALLBACK_0(BookmarkBubbleGtk, void, OnRemoveClicked);
  CHROMEGTK_CALLBACK_1(BookmarkBubbleGtk,
                       gboolean,
                       OnSignInClicked,
                       gchar*);
  CHROMEGTK_CALLBACK_0(BookmarkBubbleGtk, void, OnSyncPromoRealize);
  CHROMEGTK_CALLBACK_1(BookmarkBubbleGtk,
                       gboolean,
                       OnSyncPromoExpose,
                       GdkEventExpose*);

  // Sets the colors used in the sync promo according to the current theme.
  void UpdatePromoColors();

  // Update the bookmark with any edits that have been made.
  void ApplyEdits();

  // Open the bookmark editor for the current url and close the bubble.
  void ShowEditor();

  // Return the UTF8 encoded title for the current |url_|.
  std::string GetTitle();

  void InitFolderComboModel();

  // We basically have a singleton, since a bubble is sort of app-modal.  This
  // keeps track of the currently open bubble, or NULL if none is open.
  static BookmarkBubbleGtk* bookmark_bubble_;

  // The URL of the bookmark.
  GURL url_;

  // Our current profile (used to access the bookmark system).
  Profile* profile_;

  // This is owned by the Profile.
  BookmarkModel* model_;

  // Provides colors and stuff.
  GtkThemeService* theme_service_;

  // The widget relative to which we are positioned.
  GtkWidget* anchor_;

  // The button that removes the bookmark.
  GtkWidget* remove_button_;

  // The bookmark sync promo, if shown.
  GtkWidget* promo_;

  // The label in the bookmark sync promo, if shown.
  GtkWidget* promo_label_;

  // The various labels in the interface. We keep track of them for theme
  // changes.
  std::vector<GtkWidget*> labels_;

  // The GtkEntry for editing the bookmark name / title.
  GtkWidget* name_entry_;

  // The combo box for selecting the bookmark folder.
  GtkWidget* folder_combo_;
  scoped_ptr<RecentlyUsedFoldersComboModel> folder_combo_model_;

  BubbleGtk* bubble_;

  // Whether the bubble is creating or editing an existing bookmark.
  bool newly_bookmarked_;
  // When closing the window, whether we should update or remove the bookmark.
  bool apply_edits_;
  bool remove_bookmark_;

  content::NotificationRegistrar registrar_;

  // We need to push some things on the back of the message loop, so we have
  // a factory attached to our instance to manage task lifetimes.
  base::WeakPtrFactory<BookmarkBubbleGtk> factory_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkBubbleGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_BOOKMARKS_BOOKMARK_BUBBLE_GTK_H_
