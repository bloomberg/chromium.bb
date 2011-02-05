// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/options/cookies_view.h"

#include <gdk/gdkkeysyms.h>
#include <set>
#include <string>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/cookies_tree_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/gtk_util.h"

namespace {

// Initial size for dialog.
const int kDialogDefaultWidth = 550;
const int kDialogDefaultHeight = 550;

// Delay after entering filter text before filtering occurs.
const int kSearchFilterDelayMs = 500;

// Response ids for our custom buttons.
enum {
  RESPONSE_REMOVE = 1,
  RESPONSE_REMOVE_ALL
};

// The currently open cookie manager, if any.
CookiesView* instance_ = NULL;

}  // namespace

CookiesView::~CookiesView() {
  if (destroy_dialog_in_destructor_)
    gtk_widget_destroy(dialog_);
}

// static
void CookiesView::Show(
    GtkWindow* parent,
    Profile* profile,
    BrowsingDataDatabaseHelper* browsing_data_database_helper,
    BrowsingDataLocalStorageHelper* browsing_data_local_storage_helper,
    BrowsingDataAppCacheHelper* browsing_data_appcache_helper,
    BrowsingDataIndexedDBHelper* browsing_data_indexed_db_helper) {
  DCHECK(profile);
  DCHECK(browsing_data_database_helper);
  DCHECK(browsing_data_local_storage_helper);
  DCHECK(browsing_data_appcache_helper);
  DCHECK(browsing_data_indexed_db_helper);

  // If there's already an existing editor window, activate it.
  if (instance_) {
    gtk_util::PresentWindow(instance_->dialog_, 0);
  } else {
    instance_ = new CookiesView(parent,
                                profile,
                                browsing_data_database_helper,
                                browsing_data_local_storage_helper,
                                browsing_data_appcache_helper,
                                browsing_data_indexed_db_helper);
  }
}

CookiesView::CookiesView(
    GtkWindow* parent,
    Profile* profile,
    BrowsingDataDatabaseHelper* browsing_data_database_helper,
    BrowsingDataLocalStorageHelper* browsing_data_local_storage_helper,
    BrowsingDataAppCacheHelper* browsing_data_appcache_helper,
    BrowsingDataIndexedDBHelper* browsing_data_indexed_db_helper)
    : profile_(profile),
      browsing_data_database_helper_(browsing_data_database_helper),
      browsing_data_local_storage_helper_(browsing_data_local_storage_helper),
      browsing_data_appcache_helper_(browsing_data_appcache_helper),
      browsing_data_indexed_db_helper_(browsing_data_indexed_db_helper),
      filter_update_factory_(this),
      destroy_dialog_in_destructor_(false) {
  Init(parent);

  gtk_util::ShowDialogWithLocalizedSize(dialog_,
      IDS_COOKIES_DIALOG_WIDTH_CHARS,
      -1,
      true);

  gtk_chrome_cookie_view_clear(GTK_CHROME_COOKIE_VIEW(cookie_display_));
}

void CookiesView::TestDestroySynchronously() {
  g_signal_handler_disconnect(dialog_, destroy_handler_);
  destroy_dialog_in_destructor_ = true;
}

void CookiesView::Init(GtkWindow* parent) {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(
          IDS_COOKIES_WEBSITE_PERMISSIONS_WINDOW_TITLE).c_str(),
      parent,
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CLOSE,
      GTK_RESPONSE_CLOSE,
      NULL);
  // Allow browser windows to go in front of the options dialog in metacity.
  gtk_window_set_type_hint(GTK_WINDOW(dialog_), GDK_WINDOW_TYPE_HINT_NORMAL);
  gtk_util::SetWindowIcon(GTK_WINDOW(dialog_));

  remove_button_ = gtk_util::AddButtonToDialog(
      dialog_,
      gfx::ConvertAcceleratorsFromWindowsStyle(
        l10n_util::GetStringUTF8(IDS_COOKIES_REMOVE_LABEL)).c_str(),
      GTK_STOCK_REMOVE,
      RESPONSE_REMOVE);
  gtk_button_set_use_underline(GTK_BUTTON(remove_button_), TRUE);
  gtk_button_box_set_child_secondary(
      GTK_BUTTON_BOX(GTK_DIALOG(dialog_)->action_area),
      remove_button_,
      TRUE);

  remove_all_button_ = gtk_util::AddButtonToDialog(
      dialog_,
      gfx::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(IDS_COOKIES_REMOVE_ALL_LABEL)).c_str(),
      GTK_STOCK_CLEAR,
      RESPONSE_REMOVE_ALL);
  gtk_button_set_use_underline(GTK_BUTTON(remove_all_button_), TRUE);
  gtk_button_box_set_child_secondary(
      GTK_BUTTON_BOX(GTK_DIALOG(dialog_)->action_area),
      remove_all_button_,
      TRUE);

  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_CLOSE);
  gtk_window_set_default_size(GTK_WINDOW(dialog_), kDialogDefaultWidth,
                              kDialogDefaultHeight);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);
  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);
  destroy_handler_ = g_signal_connect(dialog_, "destroy",
                                      G_CALLBACK(OnWindowDestroyThunk), this);

  // Filtering controls.
  GtkWidget* filter_hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  filter_entry_ = gtk_entry_new();
  g_signal_connect(filter_entry_, "activate",
                   G_CALLBACK(OnFilterEntryActivatedThunk), this);
  g_signal_connect(filter_entry_, "changed",
                   G_CALLBACK(OnFilterEntryChangedThunk), this);
  gtk_box_pack_start(GTK_BOX(filter_hbox), filter_entry_,
                     TRUE, TRUE, 0);
  filter_clear_button_ = gtk_button_new_with_mnemonic(
      gfx::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(IDS_COOKIES_CLEAR_SEARCH_LABEL)).c_str());
  g_signal_connect(filter_clear_button_, "clicked",
                   G_CALLBACK(OnFilterClearButtonClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(filter_hbox), filter_clear_button_,
                     FALSE, FALSE, 0);

  GtkWidget* filter_controls = gtk_util::CreateLabeledControlsGroup(NULL,
      l10n_util::GetStringUTF8(IDS_COOKIES_SEARCH_LABEL).c_str(), filter_hbox,
      NULL);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox), filter_controls,
                     FALSE, FALSE, 0);

  // Cookie list.
  GtkWidget* cookie_list_vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox), cookie_list_vbox,
                     TRUE, TRUE, 0);

  description_label_ = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_COOKIES_INFO_LABEL).c_str());
  gtk_misc_set_alignment(GTK_MISC(description_label_), 0, 0.5);
  gtk_box_pack_start(GTK_BOX(cookie_list_vbox), description_label_,
                     FALSE, FALSE, 0);

  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(cookie_list_vbox), scroll_window, TRUE, TRUE, 0);

  cookies_tree_model_.reset(new CookiesTreeModel(
      profile_->GetRequestContext()->GetCookieStore()->GetCookieMonster(),
      browsing_data_database_helper_,
      browsing_data_local_storage_helper_,
      NULL,
      browsing_data_appcache_helper_,
      browsing_data_indexed_db_helper_));
  cookies_tree_adapter_.reset(
      new gtk_tree::TreeAdapter(this, cookies_tree_model_.get()));
  tree_ = gtk_tree_view_new_with_model(
      GTK_TREE_MODEL(cookies_tree_adapter_->tree_store()));
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_), FALSE);
  gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(tree_), TRUE);
  gtk_container_add(GTK_CONTAINER(scroll_window), tree_);

  GtkTreeViewColumn* title_column = gtk_tree_view_column_new();
  GtkCellRenderer* pixbuf_renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(title_column, pixbuf_renderer, FALSE);
  gtk_tree_view_column_add_attribute(title_column, pixbuf_renderer, "pixbuf",
                                     gtk_tree::TreeAdapter::COL_ICON);
  GtkCellRenderer* title_renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(title_column, title_renderer, TRUE);
  gtk_tree_view_column_add_attribute(title_column, title_renderer, "text",
                                     gtk_tree::TreeAdapter::COL_TITLE);
  gtk_tree_view_column_set_title(
      title_column, l10n_util::GetStringUTF8(
          IDS_COOKIES_DOMAIN_COLUMN_HEADER).c_str());
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_), title_column);
  g_signal_connect(tree_, "key-press-event",
                   G_CALLBACK(OnTreeViewKeyPressThunk), this);
  g_signal_connect(tree_, "row-expanded",
                   G_CALLBACK(OnTreeViewRowExpandedThunk), this);

  selection_ = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_));
  gtk_tree_selection_set_mode(selection_, GTK_SELECTION_SINGLE);
  g_signal_connect(selection_, "changed",
                   G_CALLBACK(OnTreeViewSelectionChangeThunk), this);

  cookie_display_ = gtk_chrome_cookie_view_new(FALSE);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                     cookie_display_, FALSE, FALSE, 0);

  // Populate the view.
  cookies_tree_adapter_->Init();
  SetInitialTreeState();
  EnableControls();
}

void CookiesView::SetInitialTreeState() {
  if (cookies_tree_model_->GetChildCount(cookies_tree_model_->GetRoot()))
    gtk_tree::SelectAndFocusRowNum(0, GTK_TREE_VIEW(tree_));
}

void CookiesView::EnableControls() {
  GtkTreeIter iter;
  bool selected = gtk_tree_selection_get_selected(selection_, NULL, &iter);
  gtk_widget_set_sensitive(remove_button_, selected);
  gtk_widget_set_sensitive(
      remove_all_button_,
      cookies_tree_model_->GetChildCount(cookies_tree_model_->GetRoot()));

  const gchar* filter_text = gtk_entry_get_text(GTK_ENTRY(filter_entry_));
  gtk_widget_set_sensitive(filter_clear_button_, filter_text && *filter_text);

  if (selected) {
    CookieTreeNode::DetailedInfo detailed_info =
        static_cast<CookieTreeNode*>(
            cookies_tree_adapter_->GetNode(&iter))->GetDetailedInfo();
    if (detailed_info.node_type == CookieTreeNode::DetailedInfo::TYPE_COOKIE) {
      gtk_chrome_cookie_view_display_cookie(
          GTK_CHROME_COOKIE_VIEW(cookie_display_),
          detailed_info.cookie->Domain(),
          *detailed_info.cookie);
    } else if (detailed_info.node_type ==
               CookieTreeNode::DetailedInfo::TYPE_DATABASE) {
      gtk_chrome_cookie_view_display_database(
          GTK_CHROME_COOKIE_VIEW(cookie_display_),
          *detailed_info.database_info);
    } else if (detailed_info.node_type ==
               CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE) {
      gtk_chrome_cookie_view_display_local_storage(
          GTK_CHROME_COOKIE_VIEW(cookie_display_),
          *detailed_info.local_storage_info);
    } else if (detailed_info.node_type ==
               CookieTreeNode::DetailedInfo::TYPE_APPCACHE) {
      gtk_chrome_cookie_view_display_app_cache(
          GTK_CHROME_COOKIE_VIEW(cookie_display_),
          *detailed_info.appcache_info);
    } else if (detailed_info.node_type ==
               CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB) {
      gtk_chrome_cookie_view_display_indexed_db(
          GTK_CHROME_COOKIE_VIEW(cookie_display_),
          *detailed_info.indexed_db_info);
    } else {
      gtk_chrome_cookie_view_clear(GTK_CHROME_COOKIE_VIEW(cookie_display_));
    }
  } else {
    gtk_chrome_cookie_view_clear(GTK_CHROME_COOKIE_VIEW(cookie_display_));
  }
}

void CookiesView::RemoveSelectedItems() {
  GtkTreeIter iter;
  bool selected = gtk_tree_selection_get_selected(selection_, NULL, &iter);
  if (selected) {
    GtkTreePath* path = gtk_tree_model_get_path(
        GTK_TREE_MODEL(cookies_tree_adapter_->tree_store()),
        &iter);
    CookieTreeNode* node = static_cast<CookieTreeNode*>(
        cookies_tree_adapter_->GetNode(&iter));
    cookies_tree_model_->DeleteCookieNode(node);
    // After removing a node, try to select the "next" node.
    // We call gtk_tree_model_get_iter to check if there is still a node at the
    // pointed to by path.  If not, we try to select the previous node in that
    // subtree.  If that subtree is empty, we then try to select the parent.
    if (gtk_tree_model_get_iter(
        GTK_TREE_MODEL(cookies_tree_adapter_->tree_store()),
        &iter,
        path)) {
      gtk_tree_selection_select_iter(selection_, &iter);
    } else if (gtk_tree_path_prev(path)) {
      gtk_tree_selection_select_path(selection_, path);
    } else if (gtk_tree_path_up(path)) {
      gtk_tree_selection_select_path(selection_, path);
    }
    gtk_tree_path_free(path);
  }
}

void CookiesView::OnAnyModelUpdateStart() {
  g_signal_handlers_block_by_func(
      selection_, reinterpret_cast<gpointer>(OnTreeViewSelectionChangeThunk),
      this);
}

void CookiesView::OnAnyModelUpdate() {
  g_signal_handlers_unblock_by_func(
      selection_, reinterpret_cast<gpointer>(OnTreeViewSelectionChangeThunk),
      this);
  EnableControls();
}

void CookiesView::OnResponse(GtkWidget* dialog, int response_id) {
  if (response_id == RESPONSE_REMOVE) {
    RemoveSelectedItems();
  } else if (response_id == RESPONSE_REMOVE_ALL) {
    cookies_tree_model_->DeleteAllStoredObjects();
  } else {
    gtk_widget_destroy(dialog);
  }
}

void CookiesView::OnWindowDestroy(GtkWidget* widget) {
  instance_ = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void CookiesView::OnTreeViewSelectionChange(GtkWidget* selection) {
  EnableControls();
}

gboolean CookiesView::OnTreeViewKeyPress(GtkWidget* tree_view,
                                         GdkEventKey* key) {
  if (key->keyval == GDK_Delete) {
    RemoveSelectedItems();
    return TRUE;
  }
  return FALSE;
}

void CookiesView::OnTreeViewRowExpanded(GtkWidget* tree_view,
                                        GtkTreeIter* iter,
                                        GtkTreePath* path) {
  // When a row in the tree is expanded, expand all the children too.
  g_signal_handlers_block_by_func(
      tree_view, reinterpret_cast<gpointer>(OnTreeViewRowExpandedThunk), this);
  gtk_tree_view_expand_row(GTK_TREE_VIEW(tree_view), path, TRUE);
  g_signal_handlers_unblock_by_func(
      tree_view, reinterpret_cast<gpointer>(OnTreeViewRowExpandedThunk), this);
}

void CookiesView::UpdateFilterResults() {
  const gchar* text = gtk_entry_get_text(GTK_ENTRY(filter_entry_));
  if (text) {
    cookies_tree_model_->UpdateSearchResults(UTF8ToWide(text));
    SetInitialTreeState();
  }
}

void CookiesView::OnFilterEntryActivated(GtkWidget* entry) {
  filter_update_factory_.RevokeAll();
  UpdateFilterResults();
}

void CookiesView::OnFilterEntryChanged(GtkWidget* editable) {
  filter_update_factory_.RevokeAll();
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      filter_update_factory_.NewRunnableMethod(
          &CookiesView::UpdateFilterResults), kSearchFilterDelayMs);
  EnableControls();
}

void CookiesView::OnFilterClearButtonClicked(GtkWidget* button) {
  gtk_entry_set_text(GTK_ENTRY(filter_entry_), "");
  filter_update_factory_.RevokeAll();
  UpdateFilterResults();
}
