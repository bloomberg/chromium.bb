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
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/bookmarks/bookmark_utils_gtk.h"
#include "chrome/browser/ui/gtk/global_bookmark_menu.h"
#include "chrome/browser/ui/gtk/global_menu_bar.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/common/notification_service.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/gtk_util.h"

GlobalBookmarkMenu::GlobalBookmarkMenu(Browser* browser)
    : browser_(browser),
      profile_(browser->profile()),
      default_favicon_(NULL),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  DCHECK(profile_);

  default_favicon_ = GtkThemeService::GetDefaultFavicon(true);
  default_folder_ = GtkThemeService::GetFolderIcon(true);
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(profile_)));
}

GlobalBookmarkMenu::~GlobalBookmarkMenu() {
  profile_->GetBookmarkModel()->RemoveObserver(this);
}

void GlobalBookmarkMenu::Init(GtkWidget* bookmark_menu,
                              GtkWidget* bookmark_menu_item) {
  bookmark_menu_.Own(bookmark_menu);

  BookmarkModel* model = profile_->GetBookmarkModel();
  model->AddObserver(this);
  if (model->IsLoaded())
    Loaded(model, false);
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

  const BookmarkNode* bar_node = model->bookmark_bar_node();
  if (!bar_node->empty()) {
    AddBookmarkMenuItem(bookmark_menu_.get(), gtk_separator_menu_item_new());
    AddNodeToMenu(bar_node, bookmark_menu_.get());
  }

  // Only display the other bookmarks folder in the menu if it has items in it.
  const BookmarkNode* other_node = model->other_node();
  if (!other_node->empty()) {
    GtkWidget* submenu = gtk_menu_new();
    AddNodeToMenu(other_node, submenu);

    AddBookmarkMenuItem(bookmark_menu_.get(), gtk_separator_menu_item_new());

    GtkWidget* menu_item = gtk_image_menu_item_new_with_label(
        l10n_util::GetStringUTF8(IDS_BOOKMARK_BAR_OTHER_FOLDER_NAME).c_str());
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    gtk_image_menu_item_set_image(
        GTK_IMAGE_MENU_ITEM(menu_item),
        gtk_image_new_from_pixbuf(default_folder_));
    gtk_util::SetAlwaysShowImage(menu_item);

    AddBookmarkMenuItem(bookmark_menu_.get(), menu_item);
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
  if (node->empty()) {
    GtkWidget* item = gtk_menu_item_new_with_label(
        l10n_util::GetStringUTF8(IDS_MENU_EMPTY_SUBMENU).c_str());
    gtk_widget_set_sensitive(item, FALSE);
    AddBookmarkMenuItem(menu, item);
  } else {
    for (int i = 0; i < node->child_count(); ++i) {
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
  CHECK(node);
  CHECK(menu_item);

  gtk_menu_item_set_label(GTK_MENU_ITEM(menu_item),
                          bookmark_utils::BuildMenuLabelFor(node).c_str());

  if (node->is_url()) {
    gtk_widget_set_tooltip_markup(
        menu_item,
        bookmark_utils::BuildTooltipFor(node).c_str());
  }

  const SkBitmap& bitmap = profile_->GetBookmarkModel()->GetFavicon(node);
  if (!bitmap.isNull()) {
    GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&bitmap);
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),
                                  gtk_image_new_from_pixbuf(pixbuf));
    g_object_unref(pixbuf);
  } else {
    GdkPixbuf* pixbuf = node->is_url() ? default_favicon_ : default_folder_;
    CHECK(pixbuf);
    GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
    CHECK(image);

    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item), image);
  }

  gtk_util::SetAlwaysShowImage(menu_item);
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

  gtk_container_foreach(GTK_CONTAINER(bookmark_menu_.get()),
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

void GlobalBookmarkMenu::Observe(int type,
                                 const NotificationSource& source,
                                 const NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_BROWSER_THEME_CHANGED);

  // Change the icon and invalidate the menu.
  default_favicon_ = GtkThemeService::GetDefaultFavicon(true);
  default_folder_ = GtkThemeService::GetFolderIcon(true);
  RebuildMenuInFuture();
}

void GlobalBookmarkMenu::Loaded(BookmarkModel* model, bool ids_reassigned) {
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

void GlobalBookmarkMenu::BookmarkNodeFaviconChanged(BookmarkModel* model,
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

  browser_->OpenURL(OpenURLParams(node->url(), GURL(), NEW_FOREGROUND_TAB,
                    PageTransition::AUTO_BOOKMARK));
}

