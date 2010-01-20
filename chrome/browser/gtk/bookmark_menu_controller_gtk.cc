// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/bookmark_menu_controller_gtk.h"

#include <gtk/gtk.h>

#include "app/gfx/gtk_util.h"
#include "app/gtk_dnd_util.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/gtk/bookmark_context_menu_gtk.h"
#include "chrome/browser/gtk/bookmark_utils_gtk.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/menu_gtk.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/page_navigator.h"
#include "chrome/common/gtk_util.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "webkit/glue/window_open_disposition.h"

namespace {

// TODO(estade): It might be a good idea to vary this by locale.
const int kMaxChars = 50;

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
// The event mask in this function is taken from gtkmenu.c.
void OnContextMenuHide(GtkWidget* context_menu, GtkWidget* grab_menu) {
  guint time = gtk_get_current_event_time();

  if (GTK_WIDGET_VISIBLE(grab_menu)) {
    if (!gdk_pointer_grab(grab_menu->window, TRUE,
                          GdkEventMask(
                          GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                          GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
                          GDK_POINTER_MOTION_MASK), NULL, NULL, time) == 0) {
      g_object_unref(grab_menu);
      return;
    }
    if (!gdk_keyboard_grab(grab_menu->window, TRUE, time) == 0) {
      gdk_display_pointer_ungrab(gdk_drawable_get_display(grab_menu->window),
                                 time);
      g_object_unref(grab_menu);
      return;
    }
    gtk_grab_add(grab_menu);
  }

  // Match the ref we took when connecting this signal.
  g_object_unref(grab_menu);
}

}  // namespace

BookmarkMenuController::BookmarkMenuController(Browser* browser,
                                               Profile* profile,
                                               PageNavigator* navigator,
                                               GtkWindow* window,
                                               const BookmarkNode* node,
                                               int start_child_index,
                                               bool show_other_folder)
    : browser_(browser),
      profile_(profile),
      page_navigator_(navigator),
      parent_window_(window),
      model_(profile->GetBookmarkModel()),
      node_(node),
      ignore_button_release_(false),
      triggering_widget_(NULL) {
  menu_ = gtk_menu_new();
  BuildMenu(node, start_child_index, menu_);
  g_signal_connect(menu_, "hide",
                   G_CALLBACK(OnMenuHidden), this);
  gtk_widget_show_all(menu_);
}

BookmarkMenuController::~BookmarkMenuController() {

  if (context_menu_.get())
    context_menu_->DelegateDestroyed();
  profile_->GetBookmarkModel()->RemoveObserver(this);
  gtk_menu_popdown(GTK_MENU(menu_));
}

void BookmarkMenuController::Popup(GtkWidget* widget, gint button_type,
                                   guint32 timestamp) {
  profile_->GetBookmarkModel()->AddObserver(this);

  triggering_widget_ = widget;
  gtk_chrome_button_set_paint_state(GTK_CHROME_BUTTON(widget),
                                    GTK_STATE_ACTIVE);
  gtk_menu_popup(GTK_MENU(menu_), NULL, NULL,
                 &MenuGtk::WidgetMenuPositionFunc,
                 widget, button_type, timestamp);
}

void BookmarkMenuController::BookmarkModelChanged() {
  gtk_menu_popdown(GTK_MENU(menu_));
}

void BookmarkMenuController::BookmarkNodeFavIconLoaded(
    BookmarkModel* model, const BookmarkNode* node) {
  std::map<const BookmarkNode*, GtkWidget*>::iterator it =
      node_to_menu_widget_map_.find(node);
  if (it != node_to_menu_widget_map_.end())
    SetImageMenuItem(it->second, node, model);
}

void BookmarkMenuController::WillExecuteCommand() {
  gtk_menu_popdown(GTK_MENU(menu_));
}

void BookmarkMenuController::NavigateToMenuItem(
    GtkWidget* menu_item,
    WindowOpenDisposition disposition) {
  const BookmarkNode* node = GetNodeFromMenuItem(menu_item);
  DCHECK(node);
  DCHECK(page_navigator_);
  page_navigator_->OpenURL(
      node->GetURL(), GURL(), disposition, PageTransition::AUTO_BOOKMARK);
}

void BookmarkMenuController::BuildMenu(const BookmarkNode* parent,
                                       int start_child_index,
                                       GtkWidget* menu) {
  DCHECK(!parent->GetChildCount() ||
         start_child_index < parent->GetChildCount());

  g_signal_connect(G_OBJECT(menu), "button-press-event",
                   G_CALLBACK(OnButtonPressed), this);

  for (int i = start_child_index; i < parent->GetChildCount(); ++i) {
    const BookmarkNode* node = parent->GetChild(i);

    // This breaks on word boundaries. Ideally we would break on character
    // boundaries.
    std::wstring elided_name =
        l10n_util::TruncateString(node->GetTitle(), kMaxChars);
    GtkWidget* menu_item =
        gtk_image_menu_item_new_with_label(WideToUTF8(elided_name).c_str());
    g_object_set_data(G_OBJECT(menu_item), "bookmark-node", AsVoid(node));
    SetImageMenuItem(menu_item, node, profile_->GetBookmarkModel());
    gtk_util::SetAlwaysShowImage(menu_item);

    if (node->is_url()) {
      g_signal_connect(G_OBJECT(menu_item), "activate",
                       G_CALLBACK(OnMenuItemActivated), this);
      g_signal_connect(G_OBJECT(menu_item), "button-release-event",
                       G_CALLBACK(OnButtonReleased), this);
    } else if (node->is_folder()) {
      GtkWidget* submenu = gtk_menu_new();
      BuildMenu(node, 0, submenu);
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    } else {
      NOTREACHED();
    }

    gtk_drag_source_set(menu_item, GDK_BUTTON1_MASK, NULL, 0,
        static_cast<GdkDragAction>(GDK_ACTION_COPY | GDK_ACTION_LINK));
    int target_mask = GtkDndUtil::CHROME_BOOKMARK_ITEM;
    if (node->is_url())
      target_mask |= GtkDndUtil::TEXT_URI_LIST | GtkDndUtil::NETSCAPE_URL;
    GtkDndUtil::SetSourceTargetListFromCodeMask(menu_item, target_mask);
    g_signal_connect(G_OBJECT(menu_item), "drag-begin",
                     G_CALLBACK(&OnMenuItemDragBegin), this);
    g_signal_connect(G_OBJECT(menu_item), "drag-end",
                     G_CALLBACK(&OnMenuItemDragEnd), this);
    g_signal_connect(G_OBJECT(menu_item), "drag-data-get",
                     G_CALLBACK(&OnMenuItemDragGet), this);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    node_to_menu_widget_map_[node] = menu_item;
  }

  if (parent->GetChildCount() == 0) {
    GtkWidget* empty_menu = gtk_menu_item_new_with_label(
        l10n_util::GetStringUTF8(IDS_MENU_EMPTY_SUBMENU).c_str());
    gtk_widget_set_sensitive(empty_menu, FALSE);
    g_object_set_data(G_OBJECT(menu), "parent-node", AsVoid(parent));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), empty_menu);
  }
}

// static
gboolean BookmarkMenuController::OnButtonPressed(
    GtkWidget* sender,
    GdkEventButton* event,
    BookmarkMenuController* controller) {
  controller->ignore_button_release_ = false;
  GtkMenuShell* menu_shell = GTK_MENU_SHELL(sender);

  if (event->button == 3) {
    // If the cursor is outside our bounds, pass this event up to the parent.
    if (!gtk_util::WidgetContainsCursor(sender)) {
      if (menu_shell->parent_menu_shell) {
        return OnButtonPressed(menu_shell->parent_menu_shell, event,
                               controller);
      } else {
        // We are the top level menu; we can propagate no further.
        return FALSE;
      }
    }

    // This will return NULL if we are not an empty menu.
    const BookmarkNode* parent = GetParentNodeFromEmptyMenu(sender);
    bool is_empty_menu = !!parent;
    // If there is no active menu item and we are not an empty menu, then do
    // nothing.
    GtkWidget* menu_item = menu_shell->active_menu_item;
    if (!is_empty_menu && !menu_item)
      return FALSE;

    const BookmarkNode* node =
        menu_item ? GetNodeFromMenuItem(menu_item) : NULL;
    DCHECK_NE(is_empty_menu, !!node);
    if (!is_empty_menu)
      parent = node->GetParent();

    // Show the right click menu and stop processing this button event.
    std::vector<const BookmarkNode*> nodes;
    if (node)
      nodes.push_back(node);
    controller->context_menu_.reset(
        new BookmarkContextMenuGtk(
            controller->parent_window_, controller->profile_,
            controller->browser_, controller->page_navigator_, parent, nodes,
            BookmarkContextMenuGtk::BOOKMARK_BAR, controller));

    // Our bookmark folder menu loses the grab to the context menu. When the
    // context menu is hidden, re-assert our grab.
    GtkWidget* grabbing_menu = gtk_grab_get_current();
    g_object_ref(grabbing_menu);
    g_signal_connect(controller->context_menu_->menu(), "hide",
                     G_CALLBACK(OnContextMenuHide), grabbing_menu);

    controller->context_menu_->PopupAsContext(event->time);
    return TRUE;
  }

  return FALSE;
}

// static
gboolean BookmarkMenuController::OnButtonReleased(
    GtkWidget* sender,
    GdkEventButton* event,
    BookmarkMenuController* controller) {
  if (controller->ignore_button_release_) {
    // Don't handle this message; it was a drag.
    controller->ignore_button_release_ = false;
    return FALSE;
  }

  // Releasing either button 1 or 2 should trigger the bookmark menu.
  if (event->button == 1 || event->button == 2) {
    WindowOpenDisposition disposition =
        event_utils::DispositionFromEventFlags(event->state);
    controller->NavigateToMenuItem(sender, disposition);

    // We need to manually dismiss the popup menu because we're overriding
    // button-release-event.
    gtk_menu_popdown(GTK_MENU(controller->menu_));
    return TRUE;
  }

  return FALSE;
}

// static
void BookmarkMenuController::OnMenuHidden(GtkWidget* menu,
    BookmarkMenuController* controller) {
  if (controller->triggering_widget_) {
    gtk_chrome_button_unset_paint_state(
        GTK_CHROME_BUTTON(controller->triggering_widget_));
  }
}

// static
void BookmarkMenuController::OnMenuItemActivated(
    GtkMenuItem* menu_item, BookmarkMenuController* controller) {
  controller->NavigateToMenuItem(GTK_WIDGET(menu_item), CURRENT_TAB);
}

// static
void BookmarkMenuController::OnMenuItemDragBegin(
    GtkWidget* menu_item,
    GdkDragContext* drag_context,
    BookmarkMenuController* controller) {
  // The parent menu item might be removed during the drag. Ref it so |button|
  // won't get destroyed.
  g_object_ref(menu_item->parent);

  // Signal to any future OnButtonReleased calls that we're dragging instead of
  // pressing.
  controller->ignore_button_release_ = true;

  const BookmarkNode* node = bookmark_utils::BookmarkNodeForWidget(menu_item);
  GtkWidget* window = bookmark_utils::GetDragRepresentation(
      node, controller->model_,
      GtkThemeProvider::GetFrom(controller->profile_));
  gint x, y;
  gtk_widget_get_pointer(menu_item, &x, &y);
  gtk_drag_set_icon_widget(drag_context, window, x, y);

  // Hide our node.
  gtk_widget_hide(menu_item);
}

// static
void BookmarkMenuController::OnMenuItemDragEnd(
    GtkWidget* menu_item,
    GdkDragContext* drag_context,
    BookmarkMenuController* controller) {
  gtk_widget_show(menu_item);
  g_object_unref(menu_item->parent);
}

// static
void BookmarkMenuController::OnMenuItemDragGet(
    GtkWidget* widget, GdkDragContext* context,
    GtkSelectionData* selection_data,
    guint target_type, guint time,
    BookmarkMenuController* controller) {
  const BookmarkNode* node = bookmark_utils::BookmarkNodeForWidget(widget);
  bookmark_utils::WriteBookmarkToSelection(node, selection_data, target_type,
                                           controller->profile_);
}
