// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/global_bookmark_menu.h"

#include <dlfcn.h>
#include <gtk/gtk.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/global_bookmark_menu.h"
#include "chrome/browser/ui/gtk/global_menu_bar.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "content/common/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/gtk_util.h"

namespace {

const int kMaxChars = 50;

// We need to know whether we're using a newer GTK at run time because we need
// to prevent.
//
// TODO(erg): Once we've dropped Hardy support, remove this hack.
typedef void (*gtk_menu_item_set_label_func)(GtkMenuItem*, const gchar*);
gtk_menu_item_set_label_func gtk_menu_item_set_label_sym =
#if GTK_CHECK_VERSION(2, 16, 1)
    gtk_menu_item_set_label;
#else
    NULL;
#endif

void EnsureMenuItemFunctions() {
#if !GTK_CHECK_VERSION(2, 16, 1)
  static bool methods_looked_up = false;
  if (!methods_looked_up) {
    methods_looked_up = true;
    gtk_menu_item_set_label_sym =
        reinterpret_cast<gtk_menu_item_set_label_func>(
            dlsym(NULL, "gtk_menu_item_set_label"));
  }
#endif
}

}  // namespace

GlobalBookmarkMenu::GlobalBookmarkMenu(Browser* browser)
    : browser_(browser),
      profile_(browser->profile()),
      default_favicon_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  DCHECK(profile_);

  default_favicon_ = GtkThemeService::GetDefaultFavicon(true);
  default_folder_ = GtkThemeService::GetFolderIcon(true);
  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 Source<Profile>(profile_));
}

GlobalBookmarkMenu::~GlobalBookmarkMenu() {
  profile_->GetBookmarkModel()->RemoveObserver(this);
}

void GlobalBookmarkMenu::Init(GtkWidget* bookmark_menu) {
  bookmark_menu_ = bookmark_menu;

  EnsureMenuItemFunctions();
  if (gtk_menu_item_set_label_sym) {
    BookmarkModel* model = profile_->GetBookmarkModel();
    model->AddObserver(this);
    if (model->IsLoaded())
      Loaded(model);
  }
}

void GlobalBookmarkMenu::RebuildMenuInFuture() {
  method_factory_.RevokeAll();
  MessageLoop::current()->PostTask(
      FROM_HERE,
      method_factory_.NewRunnableMethod(&GlobalBookmarkMenu::RebuildMenu));
}

void GlobalBookmarkMenu::RebuildMenu() {


  BookmarkModel* model = profile_->GetBookmarkModel();
  DCHECK(model);
  DCHECK(model->IsLoaded());

  ClearBookmarkMenu();

  const BookmarkNode* bar_node = model->GetBookmarkBarNode();
  if (bar_node->child_count()) {
    AddBookmarkMenuItem(bookmark_menu_, gtk_separator_menu_item_new());
    AddNodeToMenu(bar_node, bookmark_menu_);
  }

  // Only display the other bookmarks folder in the menu if it has items in it.
  const BookmarkNode* other_node = model->other_node();
  if (other_node->child_count()) {
    GtkWidget* submenu = gtk_menu_new();
    AddNodeToMenu(other_node, submenu);

    AddBookmarkMenuItem(bookmark_menu_, gtk_separator_menu_item_new());

    GtkWidget* menu_item = gtk_image_menu_item_new_with_label(
        l10n_util::GetStringUTF8(IDS_BOOMARK_BAR_OTHER_FOLDER_NAME).c_str());
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    gtk_image_menu_item_set_image(
        GTK_IMAGE_MENU_ITEM(menu_item),
        gtk_image_new_from_pixbuf(default_folder_));

    AddBookmarkMenuItem(bookmark_menu_, menu_item);
  }
}

void GlobalBookmarkMenu::AddBookmarkMenuItem(GtkWidget* menu,
                                             GtkWidget* menu_item) {
  g_object_set_data(G_OBJECT(menu_item), "type-tag",
                    GINT_TO_POINTER(GlobalMenuBar::TAG_BOOKMARK_CLEARABLE));
  gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
  gtk_widget_show(menu_item);
}

void GlobalBookmarkMenu::AddNodeToMenu(const BookmarkNode* node,
                                       GtkWidget* menu) {
  int child_count = node->child_count();
  if (!child_count) {
    GtkWidget* item = gtk_menu_item_new_with_label(
        l10n_util::GetStringUTF8(IDS_MENU_EMPTY_SUBMENU).c_str());
    gtk_widget_set_sensitive(item, FALSE);
    AddBookmarkMenuItem(menu, item);
  } else {
    for (int i = 0; i < child_count; i++) {
      const BookmarkNode* child = node->GetChild(i);
      GtkWidget* item = gtk_image_menu_item_new();
      ConfigureMenuItem(child, item);
      bookmark_nodes_[child] = item;

      if (child->is_folder()) {
        GtkWidget* submenu = gtk_menu_new();
        AddNodeToMenu(child, submenu);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
      } else {
        g_object_set_data(G_OBJECT(item), "bookmark-node",
                          const_cast<BookmarkNode*>(child));
        g_signal_connect(item, "activate",
                         G_CALLBACK(OnBookmarkItemActivatedThunk), this);
      }

      AddBookmarkMenuItem(menu, item);
    }
  }
}

void GlobalBookmarkMenu::ConfigureMenuItem(const BookmarkNode* node,
                                           GtkWidget* menu_item) {
  // This check is only to make things compile on Hardy; this code won't
  // display any visible widgets in older systems that don't have a global menu
  // bar.
  if (gtk_menu_item_set_label_sym) {
    string16 elided_name =
        l10n_util::TruncateString(node->GetTitle(), kMaxChars);
    gtk_menu_item_set_label_sym(GTK_MENU_ITEM(menu_item),
                                UTF16ToUTF8(elided_name).c_str());
  }

  if (node->is_url()) {
    std::string tooltip = gtk_util::BuildTooltipTitleFor(node->GetTitle(),
                                                         node->GetURL());
    gtk_widget_set_tooltip_markup(menu_item, tooltip.c_str());
  }

  const SkBitmap& bitmap = profile_->GetBookmarkModel()->GetFavicon(node);
  if (!bitmap.isNull()) {
    GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&bitmap);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),
                                  gtk_image_new_from_pixbuf(pixbuf));
    g_object_unref(pixbuf);
  } else {
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),
                                  gtk_image_new_from_pixbuf(
                                      node->is_url() ? default_favicon_ :
                                      default_folder_));
  }
}

GtkWidget* GlobalBookmarkMenu::MenuItemForNode(const BookmarkNode* node) {
  if (!node)
    return NULL;
  std::map<const BookmarkNode*, GtkWidget*>::iterator it =
      bookmark_nodes_.find(node);
  if (it == bookmark_nodes_.end())
    return NULL;
  return it->second;
}

void GlobalBookmarkMenu::ClearBookmarkMenu() {
  bookmark_nodes_.clear();

  gtk_container_foreach(GTK_CONTAINER(bookmark_menu_),
                        &ClearBookmarkItemCallback,
                        NULL);
}

// static
void GlobalBookmarkMenu::ClearBookmarkItemCallback(GtkWidget* menu_item,
                                                   void* unused) {
  int tag = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menu_item), "type-tag"));
  if (tag == GlobalMenuBar::TAG_BOOKMARK_CLEARABLE)
    gtk_widget_destroy(menu_item);
}

void GlobalBookmarkMenu::Observe(NotificationType type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  DCHECK(type.value == NotificationType::BROWSER_THEME_CHANGED);

  // Change the icon and invalidate the menu.
  default_favicon_ = GtkThemeService::GetDefaultFavicon(true);
  default_folder_ = GtkThemeService::GetFolderIcon(true);
  RebuildMenuInFuture();
}

void GlobalBookmarkMenu::Loaded(BookmarkModel* model) {
  // If we have a Loaded() event, then we need to build the menu immediately
  // for the first time.
  RebuildMenu();
}

void GlobalBookmarkMenu::BookmarkModelBeingDeleted(BookmarkModel* model) {
  ClearBookmarkMenu();
}

void GlobalBookmarkMenu::BookmarkNodeMoved(BookmarkModel* model,
                                           const BookmarkNode* old_parent,
                                           int old_index,
                                           const BookmarkNode* new_parent,
                                           int new_index) {
  RebuildMenuInFuture();
}

void GlobalBookmarkMenu::BookmarkNodeAdded(BookmarkModel* model,
                                           const BookmarkNode* parent,
                                           int index) {
  RebuildMenuInFuture();
}

void GlobalBookmarkMenu::BookmarkNodeRemoved(BookmarkModel* model,
                                             const BookmarkNode* parent,
                                             int old_index,
                                             const BookmarkNode* node) {
  GtkWidget* item = MenuItemForNode(node);
  if (item) {
    gtk_widget_destroy(item);
    bookmark_nodes_.erase(node);
  }
}

void GlobalBookmarkMenu::BookmarkNodeChanged(BookmarkModel* model,
                                             const BookmarkNode* node) {
  GtkWidget* item = MenuItemForNode(node);
  if (item)
    ConfigureMenuItem(node, item);
}

void GlobalBookmarkMenu::BookmarkNodeFaviconLoaded(BookmarkModel* model,
                                                   const BookmarkNode* node) {
  GtkWidget* item = MenuItemForNode(node);
  if (item)
    ConfigureMenuItem(node, item);
}

void GlobalBookmarkMenu::BookmarkNodeChildrenReordered(
    BookmarkModel* model,
    const BookmarkNode* node) {
  RebuildMenuInFuture();
}

void GlobalBookmarkMenu::OnBookmarkItemActivated(GtkWidget* menu_item) {
  // The actual mouse event that generated this activated event was in a
  // different process. Go with something default.
  const BookmarkNode* node = static_cast<const BookmarkNode*>(
      g_object_get_data(G_OBJECT(menu_item), "bookmark-node"));

  browser_->OpenURL(node->GetURL(), GURL(), NEW_FOREGROUND_TAB,
                    PageTransition::AUTO_BOOKMARK);
}

