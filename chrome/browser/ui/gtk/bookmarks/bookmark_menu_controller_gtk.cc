// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/bookmarks/bookmark_menu_controller_gtk.h"

#include <gtk/gtk.h>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/bookmarks/bookmark_utils_gtk.h"
#include "chrome/browser/ui/gtk/event_utils.h"
#include "chrome/browser/ui/gtk/gtk_chrome_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/menu_gtk.h"
#include "content/public/browser/page_navigator.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/dragdrop/gtk_dnd_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/gtk_util.h"

using content::OpenURLParams;
using content::PageNavigator;

namespace {

void SetImageMenuItem(GtkWidget* menu_item,
                      const BookmarkNode* node,
                      BookmarkModel* model) {
  GdkPixbuf* pixbuf = bookmark_utils::GetPixbufForNode(node, model, true);
  gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_item),
                                gtk_image_new_from_pixbuf(pixbuf));
  g_object_unref(pixbuf);
}

const BookmarkNode* GetNodeFromMenuItem(GtkWidget* menu_item) {
  return static_cast<const BookmarkNode*>(
      g_object_get_data(G_OBJECT(menu_item), "bookmark-node"));
}

const BookmarkNode* GetParentNodeFromEmptyMenu(GtkWidget* menu) {
  return static_cast<const BookmarkNode*>(
      g_object_get_data(G_OBJECT(menu), "parent-node"));
}

void* AsVoid(const BookmarkNode* node) {
  return const_cast<BookmarkNode*>(node);
}

// The context menu has been dismissed, restore the X and application grabs
// to whichever menu last had them. (Assuming that menu is still showing.)
void OnContextMenuHide(GtkWidget* context_menu, GtkWidget* grab_menu) {
  gtk_util::GrabAllInput(grab_menu);

  // Match the ref we took when connecting this signal.
  g_object_unref(grab_menu);
}

}  // namespace

BookmarkMenuController::BookmarkMenuController(Browser* browser,
                                               PageNavigator* navigator,
                                               GtkWindow* window,
                                               const BookmarkNode* node,
                                               int start_child_index)
    : browser_(browser),
      page_navigator_(navigator),
      parent_window_(window),
      model_(BookmarkModelFactory::GetForProfile(browser->profile())),
      node_(node),
      drag_icon_(NULL),
      ignore_button_release_(false),
      triggering_widget_(NULL) {
  menu_ = gtk_menu_new();
  g_object_ref_sink(menu_);
  BuildMenu(node, start_child_index, menu_);
  signals_.Connect(menu_, "hide", G_CALLBACK(OnMenuHiddenThunk), this);
  gtk_widget_show_all(menu_);
}

BookmarkMenuController::~BookmarkMenuController() {
  model_->RemoveObserver(this);
  // Make sure the hide handler runs.
  gtk_widget_hide(menu_);
  gtk_widget_destroy(menu_);
  g_object_unref(menu_);
}

void BookmarkMenuController::Popup(GtkWidget* widget, gint button_type,
                                   guint32 timestamp) {
  model_->AddObserver(this);

  triggering_widget_ = widget;
  signals_.Connect(triggering_widget_, "destroy",
                   G_CALLBACK(gtk_widget_destroyed), &triggering_widget_);
  gtk_chrome_button_set_paint_state(GTK_CHROME_BUTTON(widget),
                                    GTK_STATE_ACTIVE);
  gtk_menu_popup(GTK_MENU(menu_), NULL, NULL,
                 &MenuGtk::WidgetMenuPositionFunc,
                 widget, button_type, timestamp);
}

void BookmarkMenuController::BookmarkModelChanged() {
  gtk_menu_popdown(GTK_MENU(menu_));
}

void BookmarkMenuController::BookmarkNodeFaviconChanged(
    BookmarkModel* model, const BookmarkNode* node) {
  std::map<const BookmarkNode*, GtkWidget*>::iterator it =
      node_to_menu_widget_map_.find(node);
  if (it != node_to_menu_widget_map_.end())
    SetImageMenuItem(it->second, node, model);
}

void BookmarkMenuController::WillExecuteCommand() {
  gtk_menu_popdown(GTK_MENU(menu_));
}

void BookmarkMenuController::CloseMenu() {
  context_menu_->Cancel();
}

void BookmarkMenuController::NavigateToMenuItem(
    GtkWidget* menu_item,
    WindowOpenDisposition disposition) {
  const BookmarkNode* node = GetNodeFromMenuItem(menu_item);
  DCHECK(node);
  DCHECK(page_navigator_);
  page_navigator_->OpenURL(OpenURLParams(
      node->url(), content::Referrer(), disposition,
      content::PAGE_TRANSITION_AUTO_BOOKMARK, false));
}

void BookmarkMenuController::BuildMenu(const BookmarkNode* parent,
                                       int start_child_index,
                                       GtkWidget* menu) {
  DCHECK(parent->empty() || start_child_index < parent->child_count());

  signals_.Connect(menu, "button-press-event",
                   G_CALLBACK(OnMenuButtonPressedOrReleasedThunk), this);
  signals_.Connect(menu, "button-release-event",
                   G_CALLBACK(OnMenuButtonPressedOrReleasedThunk), this);

  for (int i = start_child_index; i < parent->child_count(); ++i) {
    const BookmarkNode* node = parent->GetChild(i);

    GtkWidget* menu_item = gtk_image_menu_item_new_with_label(
        bookmark_utils::BuildMenuLabelFor(node).c_str());
    g_object_set_data(G_OBJECT(menu_item), "bookmark-node", AsVoid(node));
    SetImageMenuItem(menu_item, node, model_);
    gtk_util::SetAlwaysShowImage(menu_item);

    signals_.Connect(menu_item, "button-release-event",
                     G_CALLBACK(OnButtonReleasedThunk), this);
    if (node->is_url()) {
      signals_.Connect(menu_item, "activate",
                       G_CALLBACK(OnMenuItemActivatedThunk), this);
    } else if (node->is_folder()) {
      GtkWidget* submenu = gtk_menu_new();
      BuildMenu(node, 0, submenu);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    } else {
      NOTREACHED();
    }

    gtk_drag_source_set(menu_item, GDK_BUTTON1_MASK, NULL, 0,
        static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_LINK));
    int target_mask = ui::CHROME_BOOKMARK_ITEM;
    if (node->is_url())
      target_mask |= ui::TEXT_URI_LIST | ui::NETSCAPE_URL;
    ui::SetSourceTargetListFromCodeMask(menu_item, target_mask);
    signals_.Connect(menu_item, "drag-begin",
                     G_CALLBACK(OnMenuItemDragBeginThunk), this);
    signals_.Connect(menu_item, "drag-end",
                     G_CALLBACK(OnMenuItemDragEndThunk), this);
    signals_.Connect(menu_item, "drag-data-get",
                     G_CALLBACK(OnMenuItemDragGetThunk), this);

    // It is important to connect to this signal after setting up the drag
    // source because we only want to stifle the menu's default handler and
    // not the handler that the drag source uses.
    if (node->is_folder()) {
      signals_.Connect(menu_item, "button-press-event",
                       G_CALLBACK(OnFolderButtonPressedThunk), this);
    }

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    node_to_menu_widget_map_[node] = menu_item;
  }

  if (parent->empty()) {
    GtkWidget* empty_menu = gtk_menu_item_new_with_label(
        l10n_util::GetStringUTF8(IDS_MENU_EMPTY_SUBMENU).c_str());
    gtk_widget_set_sensitive(empty_menu, FALSE);
    g_object_set_data(G_OBJECT(menu), "parent-node", AsVoid(parent));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), empty_menu);
  }
}

gboolean BookmarkMenuController::OnMenuButtonPressedOrReleased(
    GtkWidget* sender,
    GdkEventButton* event) {
  // Handle middle mouse downs and right mouse ups.
  if (!((event->button == 2 && event->type == GDK_BUTTON_RELEASE) ||
      (event->button == 3 && event->type == GDK_BUTTON_PRESS))) {
    return FALSE;
  }

  ignore_button_release_ = false;
  GtkMenuShell* menu_shell = GTK_MENU_SHELL(sender);
  // If the cursor is outside our bounds, pass this event up to the parent.
  if (!gtk_util::WidgetContainsCursor(sender)) {
    if (menu_shell->parent_menu_shell) {
      return OnMenuButtonPressedOrReleased(menu_shell->parent_menu_shell,
                                           event);
    } else {
      // We are the top level menu; we can propagate no further.
      return FALSE;
    }
  }

  // This will return NULL if we are not an empty menu.
  const BookmarkNode* parent = GetParentNodeFromEmptyMenu(sender);
  bool is_empty_menu = !!parent;
  // If there is no active menu item and we are not an empty menu, then do
  // nothing. This can happen if the user has canceled a context menu while
  // the cursor is hovering over a bookmark menu. Doing nothing is not optimal
  // (the hovered item should be active), but it's a hopefully rare corner
  // case.
  GtkWidget* menu_item = menu_shell->active_menu_item;
  if (!is_empty_menu && !menu_item)
    return TRUE;
  const BookmarkNode* node =
      menu_item ? GetNodeFromMenuItem(menu_item) : NULL;

  if (event->button == 2 && node && node->is_folder()) {
    chrome::OpenAll(parent_window_, page_navigator_, node, NEW_BACKGROUND_TAB,
                    browser_->profile());
    gtk_menu_popdown(GTK_MENU(menu_));
    return TRUE;
  } else if (event->button == 3) {
    DCHECK_NE(is_empty_menu, !!node);
    if (!is_empty_menu)
      parent = node->parent();

    // Show the right click menu and stop processing this button event.
    std::vector<const BookmarkNode*> nodes;
    if (node)
      nodes.push_back(node);
    context_menu_controller_.reset(
        new BookmarkContextMenuController(
            parent_window_, this, browser_, browser_->profile(),
            page_navigator_, parent, nodes));
    context_menu_.reset(
        new MenuGtk(NULL, context_menu_controller_->menu_model()));

    // Our bookmark folder menu loses the grab to the context menu. When the
    // context menu is hidden, re-assert our grab.
    GtkWidget* grabbing_menu = gtk_grab_get_current();
    g_object_ref(grabbing_menu);
    signals_.Connect(context_menu_->widget(), "hide",
                     G_CALLBACK(OnContextMenuHide), grabbing_menu);

    context_menu_->PopupAsContext(gfx::Point(event->x_root, event->y_root),
                                  event->time);
    return TRUE;
  }

  return FALSE;
}

gboolean BookmarkMenuController::OnButtonReleased(
    GtkWidget* sender,
    GdkEventButton* event) {
  if (ignore_button_release_) {
    // Don't handle this message; it was a drag.
    ignore_button_release_ = false;
    return FALSE;
  }

  // Releasing either button 1 or 2 should trigger the bookmark.
  if (!gtk_menu_item_get_submenu(GTK_MENU_ITEM(sender))) {
    // The menu item is a link node.
    if (event->button == 1 || event->button == 2) {
      WindowOpenDisposition disposition =
        event_utils::DispositionFromGdkState(event->state);

      NavigateToMenuItem(sender, disposition);

      // We need to manually dismiss the popup menu because we're overriding
      // button-release-event.
      gtk_menu_popdown(GTK_MENU(menu_));
      return TRUE;
    }
  } else {
    // The menu item is a folder node.
    if (event->button == 1) {
      // Having overriden the normal handling, we need to manually activate
      // the item.
      gtk_menu_shell_select_item(GTK_MENU_SHELL(sender->parent), sender);
      g_signal_emit_by_name(sender->parent, "activate-current");
      return TRUE;
    }
  }

  return FALSE;
}

gboolean BookmarkMenuController::OnFolderButtonPressed(
    GtkWidget* sender, GdkEventButton* event) {
  // The button press may start a drag; don't let the default handler run.
  if (event->button == 1)
    return TRUE;
  return FALSE;
}

void BookmarkMenuController::OnMenuHidden(GtkWidget* menu) {
  if (triggering_widget_)
    gtk_chrome_button_unset_paint_state(GTK_CHROME_BUTTON(triggering_widget_));
}

void BookmarkMenuController::OnMenuItemActivated(GtkWidget* menu_item) {
  NavigateToMenuItem(menu_item, CURRENT_TAB);
}

void BookmarkMenuController::OnMenuItemDragBegin(GtkWidget* menu_item,
                                                 GdkDragContext* drag_context) {
  // The parent menu item might be removed during the drag. Ref it so |button|
  // won't get destroyed.
  g_object_ref(menu_item->parent);

  // Signal to any future OnButtonReleased calls that we're dragging instead of
  // pressing.
  ignore_button_release_ = true;

  const BookmarkNode* node = bookmark_utils::BookmarkNodeForWidget(menu_item);
  drag_icon_ = bookmark_utils::GetDragRepresentationForNode(
      node, model_, GtkThemeService::GetFrom(browser_->profile()));
  gint x, y;
  gtk_widget_get_pointer(menu_item, &x, &y);
  gtk_drag_set_icon_widget(drag_context, drag_icon_, x, y);

  // Hide our node.
  gtk_widget_hide(menu_item);
}

void BookmarkMenuController::OnMenuItemDragEnd(GtkWidget* menu_item,
                                               GdkDragContext* drag_context) {
  gtk_widget_show(menu_item);
  g_object_unref(menu_item->parent);

  gtk_widget_destroy(drag_icon_);
  drag_icon_ = NULL;
}

void BookmarkMenuController::OnMenuItemDragGet(
    GtkWidget* widget, GdkDragContext* context,
    GtkSelectionData* selection_data,
    guint target_type, guint time) {
  const BookmarkNode* node = bookmark_utils::BookmarkNodeForWidget(widget);
  bookmark_utils::WriteBookmarkToSelection(node, selection_data, target_type,
                                           browser_->profile());
}
