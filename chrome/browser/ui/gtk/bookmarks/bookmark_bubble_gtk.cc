// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/bookmarks/bookmark_bubble_gtk.h"

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/recently_used_folders_combo_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/user_metrics.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas_paint_gtk.h"

using content::UserMetricsAction;

namespace {

enum {
  COLUMN_NAME,
  COLUMN_IS_SEPARATOR,
  COLUMN_COUNT
};

// Thickness of the bubble's border.
const int kBubbleBorderThickness = 1;

// Color of the bubble's border.
const SkColor kBubbleBorderColor = SkColorSetRGB(0x63, 0x63, 0x63);

// Background color of the sync promo.
const GdkColor kPromoBackgroundColor = GDK_COLOR_RGB(0xf5, 0xf5, 0xf5);

// Color of the border of the sync promo.
const SkColor kPromoBorderColor = SkColorSetRGB(0xe5, 0xe5, 0xe5);

// Color of the text in the sync promo.
const GdkColor kPromoTextColor = GDK_COLOR_RGB(0x66, 0x66, 0x66);

// Vertical padding inside the sync promo.
const int kPromoVerticalPadding = 15;

// Pango markup for the "Sign in" link in the sync promo.
const char kPromoLinkMarkup[] =
    "<a href='signin'><span underline='none'>%s</span></a>";

// Style to make the sync promo link blue.
const char kPromoLinkStyle[] =
    "style \"sign-in-link\" {\n"
    "  GtkWidget::link-color=\"blue\"\n"
    "}\n"
    "widget \"*sign-in-link\" style \"sign-in-link\"\n";

gboolean IsSeparator(GtkTreeModel* model, GtkTreeIter* iter, gpointer data) {
  gboolean is_separator;
  gtk_tree_model_get(model, iter, COLUMN_IS_SEPARATOR, &is_separator, -1);
  return is_separator;
}

}  // namespace

BookmarkBubbleGtk* BookmarkBubbleGtk::bookmark_bubble_ = NULL;

// static
void BookmarkBubbleGtk::Show(GtkWidget* anchor,
                             Profile* profile,
                             const GURL& url,
                             bool newly_bookmarked) {
  // Sometimes Ctrl+D may get pressed more than once on top level window
  // before the bookmark bubble window is shown and takes the keyboad focus.
  if (bookmark_bubble_)
    return;
  bookmark_bubble_ = new BookmarkBubbleGtk(anchor,
                                           profile,
                                           url,
                                           newly_bookmarked);
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

  UpdatePromoColors();
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
      promo_(NULL),
      promo_label_(NULL),
      name_entry_(NULL),
      folder_combo_(NULL),
      bubble_(NULL),
      newly_bookmarked_(newly_bookmarked),
      apply_edits_(true),
      remove_bookmark_(false),
      factory_(this) {
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

  GtkWidget* bubble_container = gtk_vbox_new(FALSE, 0);

  // Prevent the content of the bubble to be drawn on the border.
  gtk_container_set_border_width(GTK_CONTAINER(bubble_container),
                                 kBubbleBorderThickness);

  // Our content is arranged in 3 rows.  |top| contains a left justified
  // message, and a right justified remove link button.  |table| is the middle
  // portion with the name entry and the folder combo.  |bottom| is the final
  // row with a spacer, and the edit... and close buttons on the right.
  GtkWidget* content = gtk_vbox_new(FALSE, 5);
  gtk_container_set_border_width(
      GTK_CONTAINER(content),
      ui::kContentAreaBorder - kBubbleBorderThickness);
  GtkWidget* top = gtk_hbox_new(FALSE, 0);

  gtk_misc_set_alignment(GTK_MISC(label), 0, 1);
  gtk_box_pack_start(GTK_BOX(top), label,
                     TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(top), remove_button_,
                     FALSE, FALSE, 0);

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

  gtk_box_pack_start(GTK_BOX(bubble_container), content, TRUE, TRUE, 0);

  if (SyncPromoUI::ShouldShowSyncPromo(profile_)) {
    std::string link_text =
        l10n_util::GetStringUTF8(IDS_BOOKMARK_SYNC_PROMO_LINK);
    char* link_markup = g_markup_printf_escaped(kPromoLinkMarkup,
                                                link_text.c_str());
    string16 link_markup_utf16;
    base::UTF8ToUTF16(link_markup, strlen(link_markup), &link_markup_utf16);
    g_free(link_markup);

    std::string promo_markup = l10n_util::GetStringFUTF8(
        IDS_BOOKMARK_SYNC_PROMO_MESSAGE,
        link_markup_utf16);

    promo_ = gtk_event_box_new();
    gtk_widget_set_app_paintable(promo_, TRUE);

    promo_label_ = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(promo_label_), promo_markup.c_str());
    gtk_misc_set_alignment(GTK_MISC(promo_label_), 0.0, 0.0);
    gtk_misc_set_padding(GTK_MISC(promo_label_),
                         ui::kContentAreaBorder,
                         kPromoVerticalPadding);

    // Custom link color.
    gtk_rc_parse_string(kPromoLinkStyle);

    UpdatePromoColors();

    gtk_container_add(GTK_CONTAINER(promo_), promo_label_);
    gtk_box_pack_start(GTK_BOX(bubble_container), promo_, TRUE, TRUE, 0);
    g_signal_connect(promo_,
                     "realize",
                     G_CALLBACK(&OnSyncPromoRealizeThunk),
                     this);
    g_signal_connect(promo_,
                     "expose-event",
                     G_CALLBACK(&OnSyncPromoExposeThunk),
                     this);
    g_signal_connect(promo_label_,
                     "activate-link",
                     G_CALLBACK(&OnSignInClickedThunk),
                     this);
  }

  bubble_ = BubbleGtk::Show(anchor_,
                            NULL,
                            bubble_container,
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
  DCHECK(bookmark_bubble_);
  bookmark_bubble_ = NULL;

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
    base::MessageLoop::current()->PostTask(
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

gboolean BookmarkBubbleGtk::OnSignInClicked(GtkWidget* widget, gchar* uri) {
  GtkWindow* window = GTK_WINDOW(gtk_widget_get_toplevel(anchor_));
  Browser* browser = chrome::FindBrowserWithWindow(window);
  chrome::ShowBrowserSignin(browser, signin::SOURCE_BOOKMARK_BUBBLE);
  bubble_->Close();
  return TRUE;
}

void BookmarkBubbleGtk::OnSyncPromoRealize(GtkWidget* widget) {
  int width = gtk_util::GetWidgetSize(widget).width();
  gtk_util::SetLabelWidth(promo_label_, width);
}

gboolean BookmarkBubbleGtk::OnSyncPromoExpose(GtkWidget* widget,
                                              GdkEventExpose* event) {
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);

  gfx::CanvasSkiaPaint canvas(event);

  // Draw a border on top of the promo.
  canvas.DrawLine(gfx::Point(0, 0),
                  gfx::Point(allocation.width + 1, 0),
                  kPromoBorderColor);

  // Redraw the rounded corners of the bubble that are hidden by the
  // background of the promo.
  SkPaint points_paint;
  points_paint.setColor(kBubbleBorderColor);
  points_paint.setStrokeWidth(SkIntToScalar(1));
  canvas.DrawPoint(gfx::Point(0, allocation.height - 1), points_paint);
  canvas.DrawPoint(gfx::Point(allocation.width - 1, allocation.height - 1),
                   points_paint);

  return FALSE; // Propagate expose to children.
}

void BookmarkBubbleGtk::UpdatePromoColors() {
  if (!promo_)
    return;

  GdkColor promo_background_color;

  if (!theme_service_->UsingNativeTheme()) {
    promo_background_color = kPromoBackgroundColor;
    gtk_widget_set_name(promo_label_, "sign-in-link");
    gtk_util::SetLabelColor(promo_label_, &kPromoTextColor);
  } else {
    promo_background_color = theme_service_->GetGdkColor(
        ThemeProperties::COLOR_TOOLBAR);
    gtk_widget_set_name(promo_label_, "sign-in-link-theme-color");
  }

  gtk_widget_modify_bg(promo_, GTK_STATE_NORMAL, &promo_background_color);

  // No visible highlight color when the mouse is over the link.
  gtk_widget_modify_base(promo_label_,
                         GTK_STATE_ACTIVE,
                         &promo_background_color);
  gtk_widget_modify_base(promo_label_,
                         GTK_STATE_PRELIGHT,
                         &promo_background_color);
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

    folder_combo_model_->MaybeChangeParent(
        node, gtk_combo_box_get_active(GTK_COMBO_BOX(folder_combo_)));
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

  GtkListStore* store = gtk_list_store_new(COLUMN_COUNT,
                                           G_TYPE_STRING, G_TYPE_BOOLEAN);

  // We always have nodes + 1 entries in the combo. The last entry is an entry
  // that reads 'Choose Another Folder...' and when chosen Bookmark Editor is
  // opened.
  for (int i = 0; i < folder_combo_model_->GetItemCount(); ++i) {
    const bool is_separator = folder_combo_model_->IsItemSeparatorAt(i);
    const std::string name = is_separator ?
        std::string() : UTF16ToUTF8(folder_combo_model_->GetItemAt(i));

    GtkTreeIter iter;
    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter,
                       COLUMN_NAME, name.c_str(),
                       COLUMN_IS_SEPARATOR, is_separator,
                       -1);
  }

  folder_combo_ = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));

  gtk_combo_box_set_active(GTK_COMBO_BOX(folder_combo_),
                           folder_combo_model_->GetDefaultIndex());
  gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(folder_combo_),
                                       IsSeparator, NULL, NULL);
  g_object_unref(store);

  GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(folder_combo_), renderer, TRUE);
  gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(folder_combo_), renderer,
                                 "text", COLUMN_NAME,
                                 NULL);
}
