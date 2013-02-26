// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/bookmarks/bookmark_bubble_gtk.h"

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_editor.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/bookmarks/recently_used_folders_combo_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"

using content::UserMetricsAction;

namespace {

// We basically have a singleton, since a bubble is sort of app-modal.  This
// keeps track of the currently open bubble, or NULL if none is open.
BookmarkBubbleGtk* g_bubble = NULL;

}  // namespace

// static
void BookmarkBubbleGtk::Show(GtkWidget* anchor,
                             Profile* profile,
                             const GURL& url,
                             bool newly_bookmarked) {
  // Sometimes Ctrl+D may get pressed more than once on top level window
  // before the bookmark bubble window is shown and takes the keyboad focus.
  if (g_bubble)
    return;
  g_bubble = new BookmarkBubbleGtk(anchor, profile, url, newly_bookmarked);
}

void BookmarkBubbleGtk::BubbleClosing(BubbleGtk* bubble,
                                      bool closed_by_escape) {
  if (closed_by_escape) {
    remove_bookmark_ = newly_bookmarked_;
    apply_edits_ = false;
  }
}

void BookmarkBubbleGtk::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_BROWSER_THEME_CHANGED);

  if (theme_service_->UsingNativeTheme()) {
    for (std::vector<GtkWidget*>::iterator it = labels_.begin();
         it != labels_.end(); ++it) {
      gtk_widget_modify_fg(*it, GTK_STATE_NORMAL, NULL);
    }
  } else {
    for (std::vector<GtkWidget*>::iterator it = labels_.begin();
         it != labels_.end(); ++it) {
      gtk_widget_modify_fg(*it, GTK_STATE_NORMAL, &ui::kGdkBlack);
    }
  }
}

BookmarkBubbleGtk::BookmarkBubbleGtk(GtkWidget* anchor,
                                     Profile* profile,
                                     const GURL& url,
                                     bool newly_bookmarked)
    : url_(url),
      profile_(profile),
      model_(BookmarkModelFactory::GetForProfile(profile)),
      theme_service_(GtkThemeService::GetFrom(profile_)),
      anchor_(anchor),
      content_(NULL),
      name_entry_(NULL),
      folder_combo_(NULL),
      bubble_(NULL),
      factory_(this),
      newly_bookmarked_(newly_bookmarked),
      apply_edits_(true),
      remove_bookmark_(false) {
  GtkWidget* label = gtk_label_new(l10n_util::GetStringUTF8(
      newly_bookmarked_ ? IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARKED :
                          IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARK).c_str());
  labels_.push_back(label);
  remove_button_ = theme_service_->BuildChromeLinkButton(
      l10n_util::GetStringUTF8(IDS_BOOKMARK_BUBBLE_REMOVE_BOOKMARK));
  GtkWidget* edit_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_BOOKMARK_BUBBLE_OPTIONS).c_str());
  GtkWidget* close_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_DONE).c_str());

  // Our content is arranged in 3 rows.  |top| contains a left justified
  // message, and a right justified remove link button.  |table| is the middle
  // portion with the name entry and the folder combo.  |bottom| is the final
  // row with a spacer, and the edit... and close buttons on the right.
  GtkWidget* content = gtk_vbox_new(FALSE, 5);
  gtk_container_set_border_width(GTK_CONTAINER(content),
                                 ui::kContentAreaBorder);
  GtkWidget* top = gtk_hbox_new(FALSE, 0);

  gtk_misc_set_alignment(GTK_MISC(label), 0, 1);
  gtk_box_pack_start(GTK_BOX(top), label,
                     TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(top), remove_button_,
                     FALSE, FALSE, 0);

  folder_combo_ = gtk_combo_box_new_text();
  InitFolderComboModel();

  // Create the edit entry for updating the bookmark name / title.
  name_entry_ = gtk_entry_new();
  gtk_entry_set_text(GTK_ENTRY(name_entry_), GetTitle().c_str());

  // We use a table to allow the labels to line up with each other, along
  // with the entry and folder combo lining up.
  GtkWidget* table = gtk_util::CreateLabeledControlsGroup(
      &labels_,
      l10n_util::GetStringUTF8(IDS_BOOKMARK_BUBBLE_TITLE_TEXT).c_str(),
      name_entry_,
      l10n_util::GetStringUTF8(IDS_BOOKMARK_BUBBLE_FOLDER_TEXT).c_str(),
      folder_combo_,
      NULL);

  GtkWidget* bottom = gtk_hbox_new(FALSE, 0);
  // We want the buttons on the right, so just use an expanding label to fill
  // all of the extra space on the right.
  gtk_box_pack_start(GTK_BOX(bottom), gtk_label_new(""),
                     TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(bottom), edit_button,
                     FALSE, FALSE, 4);
  gtk_box_pack_start(GTK_BOX(bottom), close_button,
                     FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(content), top, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(content), table, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(content), bottom, TRUE, TRUE, 0);
  // We want the focus to start on the entry, not on the remove button.
  gtk_container_set_focus_child(GTK_CONTAINER(content), table);

  bubble_ = BubbleGtk::Show(anchor_,
                            NULL,
                            content,
                            BubbleGtk::ANCHOR_TOP_RIGHT,
                            BubbleGtk::MATCH_SYSTEM_THEME |
                                BubbleGtk::POPUP_WINDOW |
                                BubbleGtk::GRAB_INPUT,
                            theme_service_,
                            this);  // delegate
  if (!bubble_) {
    NOTREACHED();
    return;
  }

  g_signal_connect(content, "destroy",
                   G_CALLBACK(&OnDestroyThunk), this);
  g_signal_connect(name_entry_, "activate",
                   G_CALLBACK(&OnNameActivateThunk), this);
  g_signal_connect(folder_combo_, "changed",
                   G_CALLBACK(&OnFolderChangedThunk), this);
  g_signal_connect(edit_button, "clicked",
                   G_CALLBACK(&OnEditClickedThunk), this);
  g_signal_connect(close_button, "clicked",
                   G_CALLBACK(&OnCloseClickedThunk), this);
  g_signal_connect(remove_button_, "clicked",
                   G_CALLBACK(&OnRemoveClickedThunk), this);

  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_service_));
  theme_service_->InitThemesFor(this);
}

BookmarkBubbleGtk::~BookmarkBubbleGtk() {
  DCHECK(!content_);  // |content_| should have already been destroyed.

  DCHECK(g_bubble);
  g_bubble = NULL;

  if (apply_edits_) {
    ApplyEdits();
  } else if (remove_bookmark_) {
    const BookmarkNode* node = model_->GetMostRecentlyAddedNodeForURL(url_);
    if (node)
      model_->Remove(node->parent(), node->parent()->GetIndexOf(node));
  }
}

void BookmarkBubbleGtk::OnDestroy(GtkWidget* widget) {
  // We are self deleting, we have a destroy signal setup to catch when we
  // destroyed (via the BubbleGtk being destroyed), and delete ourself.
  content_ = NULL;  // We are being destroyed.
  delete this;
}

void BookmarkBubbleGtk::OnNameActivate(GtkWidget* widget) {
  bubble_->Close();
}

void BookmarkBubbleGtk::OnFolderChanged(GtkWidget* widget) {
  int index = gtk_combo_box_get_active(GTK_COMBO_BOX(folder_combo_));
  if (index == folder_combo_model_->GetItemCount() - 1) {
    content::RecordAction(
        UserMetricsAction("BookmarkBubble_EditFromCombobox"));
    // GTK doesn't handle having the combo box destroyed from the changed
    // signal.  Since showing the editor also closes the bubble, delay this
    // so that GTK can unwind.  Specifically gtk_menu_shell_button_release
    // will run, and we need to keep the combo box alive until then.
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&BookmarkBubbleGtk::ShowEditor, factory_.GetWeakPtr()));
  }
}

void BookmarkBubbleGtk::OnEditClicked(GtkWidget* widget) {
  content::RecordAction(UserMetricsAction("BookmarkBubble_Edit"));
  ShowEditor();
}

void BookmarkBubbleGtk::OnCloseClicked(GtkWidget* widget) {
  bubble_->Close();
}

void BookmarkBubbleGtk::OnRemoveClicked(GtkWidget* widget) {
  content::RecordAction(UserMetricsAction("BookmarkBubble_Unstar"));

  apply_edits_ = false;
  remove_bookmark_ = true;
  bubble_->Close();
}

void BookmarkBubbleGtk::ApplyEdits() {
  // Set this to make sure we don't attempt to apply edits again.
  apply_edits_ = false;

  const BookmarkNode* node = model_->GetMostRecentlyAddedNodeForURL(url_);
  if (node) {
    const string16 new_title(
        UTF8ToUTF16(gtk_entry_get_text(GTK_ENTRY(name_entry_))));

    if (new_title != node->GetTitle()) {
      model_->SetTitle(node, new_title);
      content::RecordAction(
          UserMetricsAction("BookmarkBubble_ChangeTitleInBubble"));
    }

    int index = gtk_combo_box_get_active(GTK_COMBO_BOX(folder_combo_));

    // Last index means 'Choose another folder...'
    if (index < folder_combo_model_->GetItemCount() - 1) {
      const BookmarkNode* new_parent = folder_combo_model_->GetNodeAt(index);
      if (new_parent != node->parent()) {
        content::RecordAction(
            UserMetricsAction("BookmarkBubble_ChangeParent"));
        model_->Move(node, new_parent, new_parent->child_count());
      }
    }
  }
}

std::string BookmarkBubbleGtk::GetTitle() {
  const BookmarkNode* node = model_->GetMostRecentlyAddedNodeForURL(url_);
  if (!node) {
    NOTREACHED();
    return std::string();
  }

  return UTF16ToUTF8(node->GetTitle());
}

void BookmarkBubbleGtk::ShowEditor() {
  const BookmarkNode* node = model_->GetMostRecentlyAddedNodeForURL(url_);

  // Commit any edits now.
  ApplyEdits();

  // Closing might delete us, so we'll cache what we need on the stack.
  Profile* profile = profile_;
  GtkWindow* toplevel = GTK_WINDOW(gtk_widget_get_toplevel(anchor_));

  // Close the bubble, deleting the C++ objects, etc.
  bubble_->Close();

  if (node) {
    BookmarkEditor::Show(toplevel, profile,
                         BookmarkEditor::EditDetails::EditNode(node),
                         BookmarkEditor::SHOW_TREE);
  }
}

void BookmarkBubbleGtk::InitFolderComboModel() {
  const BookmarkNode* node = model_->GetMostRecentlyAddedNodeForURL(url_);
  DCHECK(node);

  folder_combo_model_.reset(new RecentlyUsedFoldersComboModel(model_, node));

  // We always have nodes + 1 entries in the combo.  The last entry will be
  // the 'Select another folder...' entry that opens the bookmark editor.
  for (int i = 0; i < folder_combo_model_->GetItemCount(); ++i) {
    gtk_combo_box_append_text(GTK_COMBO_BOX(folder_combo_),
        UTF16ToUTF8(folder_combo_model_->GetItemAt(i)).c_str());
  }

  gtk_combo_box_set_active(GTK_COMBO_BOX(folder_combo_),
                           folder_combo_model_->GetDefaultIndex());
}
