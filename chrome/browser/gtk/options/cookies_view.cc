// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/cookies_view.h"

#include <gdk/gdkkeysyms.h>
#include <set>
#include <string>

#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "base/i18n/time_formatting.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/cookies_tree_model.h"
#include "chrome/common/gtk_util.h"
#include "grit/generated_resources.h"

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

void InitCookieDetailStyle(GtkWidget* entry, GtkStyle* label_style,
                           GtkStyle* dialog_style) {
  gtk_widget_modify_fg(entry, GTK_STATE_NORMAL,
                       &label_style->fg[GTK_STATE_NORMAL]);
  gtk_widget_modify_fg(entry, GTK_STATE_INSENSITIVE,
                       &label_style->fg[GTK_STATE_INSENSITIVE]);
  // GTK_NO_WINDOW widgets like GtkLabel don't draw their own background, so we
  // combine the normal or insensitive foreground of the label style with the
  // normal background of the window style to achieve the "normal label" and
  // "insensitive label" colors.
  gtk_widget_modify_base(entry, GTK_STATE_NORMAL,
                         &dialog_style->bg[GTK_STATE_NORMAL]);
  gtk_widget_modify_base(entry, GTK_STATE_INSENSITIVE,
                         &dialog_style->bg[GTK_STATE_NORMAL]);
}

}  // namespace

CookiesView::~CookiesView() {
}

// static
void CookiesView::Show(Profile* profile) {
  DCHECK(profile);

  // If there's already an existing editor window, activate it.
  if (instance_) {
    gtk_window_present(GTK_WINDOW(instance_->dialog_));
  } else {
    instance_ = new CookiesView(profile);
    instance_->InitStylesAndShow();
  }
}

CookiesView::CookiesView(Profile* profile)
    : profile_(profile),
      filter_update_factory_(this) {
  Init();
}

void CookiesView::Init() {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_COOKIES_WINDOW_TITLE).c_str(),
      NULL,
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_CLOSE,
      GTK_RESPONSE_CLOSE,
      NULL);
  gtk_util::SetWindowIcon(GTK_WINDOW(dialog_));

  remove_button_ = gtk_util::AddButtonToDialog(
      dialog_,
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
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
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
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
  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroy), this);

  // Filtering controls.
  GtkWidget* filter_hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  filter_entry_ = gtk_entry_new();
  g_signal_connect(G_OBJECT(filter_entry_), "activate",
                   G_CALLBACK(OnFilterEntryActivated), this);
  g_signal_connect(G_OBJECT(filter_entry_), "changed",
                   G_CALLBACK(OnFilterEntryChanged), this);
  gtk_box_pack_start(GTK_BOX(filter_hbox), filter_entry_,
                     TRUE, TRUE, 0);
  filter_clear_button_ = gtk_button_new_with_mnemonic(
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(IDS_COOKIES_CLEAR_SEARCH_LABEL)).c_str());
  g_signal_connect(G_OBJECT(filter_clear_button_), "clicked",
                   G_CALLBACK(OnFilterClearButtonClicked), this);
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

  cookies_tree_model_.reset(new CookiesTreeModel(profile_));
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
  g_signal_connect(G_OBJECT(tree_), "key-press-event",
                   G_CALLBACK(OnTreeViewKeyPress), this);
  g_signal_connect(G_OBJECT(tree_), "row-expanded",
                   G_CALLBACK(OnTreeViewRowExpanded), this);

  selection_ = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_));
  gtk_tree_selection_set_mode(selection_, GTK_SELECTION_SINGLE);
  g_signal_connect(G_OBJECT(selection_), "changed",
                   G_CALLBACK(OnSelectionChanged), this);

  // Cookie details.
  GtkWidget* details_frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(details_frame), GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(cookie_list_vbox), details_frame,
                     FALSE, FALSE, 0);
  cookie_details_table_ = gtk_table_new(7, 2, FALSE);
  gtk_container_add(GTK_CONTAINER(details_frame), cookie_details_table_);
  gtk_table_set_col_spacing(GTK_TABLE(cookie_details_table_), 0,
                            gtk_util::kLabelSpacing);

  int row = 0;
  InitCookieDetailRow(row++, IDS_COOKIES_COOKIE_NAME_LABEL,
                      &cookie_name_entry_);
  InitCookieDetailRow(row++, IDS_COOKIES_COOKIE_CONTENT_LABEL,
                      &cookie_content_entry_);
  InitCookieDetailRow(row++, IDS_COOKIES_COOKIE_DOMAIN_LABEL,
                      &cookie_domain_entry_);
  InitCookieDetailRow(row++, IDS_COOKIES_COOKIE_PATH_LABEL,
                      &cookie_path_entry_);
  InitCookieDetailRow(row++, IDS_COOKIES_COOKIE_SENDFOR_LABEL,
                      &cookie_send_for_entry_);
  InitCookieDetailRow(row++, IDS_COOKIES_COOKIE_CREATED_LABEL,
                      &cookie_created_entry_);
  InitCookieDetailRow(row++, IDS_COOKIES_COOKIE_EXPIRES_LABEL,
                      &cookie_expires_entry_);

  // Populate the view.
  cookies_tree_adapter_->Init();
  SetInitialTreeState();
  EnableControls();
}

void CookiesView::InitStylesAndShow() {
  // Realize a label so that its style gets initialized.
  gtk_widget_realize(description_label_);
  gtk_widget_realize(dialog_);
  GtkStyle* label_style = gtk_widget_get_style(description_label_);
  GtkStyle* dialog_style = gtk_widget_get_style(dialog_);

  InitCookieDetailStyle(cookie_name_entry_, label_style, dialog_style);
  InitCookieDetailStyle(cookie_content_entry_, label_style, dialog_style);
  InitCookieDetailStyle(cookie_domain_entry_, label_style, dialog_style);
  InitCookieDetailStyle(cookie_path_entry_, label_style, dialog_style);
  InitCookieDetailStyle(cookie_send_for_entry_, label_style, dialog_style);
  InitCookieDetailStyle(cookie_created_entry_, label_style, dialog_style);
  InitCookieDetailStyle(cookie_expires_entry_, label_style, dialog_style);

  gtk_widget_show_all(dialog_);
}

void CookiesView::InitCookieDetailRow(int row, int label_id,
                                      GtkWidget** entry) {
  GtkWidget* name_label = gtk_label_new(
      l10n_util::GetStringUTF8(label_id).c_str());
  gtk_misc_set_alignment(GTK_MISC(name_label), 1, 0.5);
  gtk_table_attach(GTK_TABLE(cookie_details_table_), name_label,
                   0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);

  *entry = gtk_entry_new();

  gtk_entry_set_editable(GTK_ENTRY(*entry), FALSE);
  gtk_entry_set_has_frame(GTK_ENTRY(*entry), FALSE);
  gtk_table_attach_defaults(GTK_TABLE(cookie_details_table_), *entry,
                            1, 2, row, row + 1);
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
      PopulateCookieDetails(detailed_info.cookie->first,
                            detailed_info.cookie->second);
    } else {
      ClearCookieDetails();
    }
  } else {
    ClearCookieDetails();
  }
}

void CookiesView::SetCookieDetailsSensitivity(gboolean enabled) {
  gtk_widget_set_sensitive(cookie_name_entry_, enabled);
  gtk_widget_set_sensitive(cookie_content_entry_, enabled);
  gtk_widget_set_sensitive(cookie_domain_entry_, enabled);
  gtk_widget_set_sensitive(cookie_path_entry_, enabled);
  gtk_widget_set_sensitive(cookie_send_for_entry_, enabled);
  gtk_widget_set_sensitive(cookie_created_entry_, enabled);
  gtk_widget_set_sensitive(cookie_expires_entry_, enabled);
}

void CookiesView::PopulateCookieDetails(
    const std::string& domain,
    const net::CookieMonster::CanonicalCookie& cookie) {
  gtk_entry_set_text(GTK_ENTRY(cookie_name_entry_), cookie.Name().c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_content_entry_), cookie.Value().c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_domain_entry_), domain.c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_path_entry_), cookie.Path().c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_created_entry_),
                     WideToUTF8(base::TimeFormatFriendlyDateAndTime(
                         cookie.CreationDate())).c_str());
  if (cookie.DoesExpire()) {
    gtk_entry_set_text(GTK_ENTRY(cookie_expires_entry_),
                       WideToUTF8(base::TimeFormatFriendlyDateAndTime(
                           cookie.ExpiryDate())).c_str());
  } else {
    gtk_entry_set_text(GTK_ENTRY(cookie_expires_entry_),
                       l10n_util::GetStringUTF8(
                           IDS_COOKIES_COOKIE_EXPIRES_SESSION).c_str());
  }
  gtk_entry_set_text(
      GTK_ENTRY(cookie_send_for_entry_),
      l10n_util::GetStringUTF8(cookie.IsSecure() ?
                               IDS_COOKIES_COOKIE_SENDFOR_SECURE :
                               IDS_COOKIES_COOKIE_SENDFOR_ANY).c_str());
  SetCookieDetailsSensitivity(TRUE);
}

void CookiesView::ClearCookieDetails() {
  std::string no_cookie =
      l10n_util::GetStringUTF8(IDS_COOKIES_COOKIE_NONESELECTED);
  gtk_entry_set_text(GTK_ENTRY(cookie_name_entry_), no_cookie.c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_content_entry_), no_cookie.c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_domain_entry_), no_cookie.c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_path_entry_), no_cookie.c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_created_entry_), no_cookie.c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_expires_entry_), no_cookie.c_str());
  gtk_entry_set_text(GTK_ENTRY(cookie_send_for_entry_), no_cookie.c_str());
  SetCookieDetailsSensitivity(FALSE);
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
      G_OBJECT(selection_), reinterpret_cast<gpointer>(OnSelectionChanged),
      this);
}

void CookiesView::OnAnyModelUpdate() {
  g_signal_handlers_unblock_by_func(
      G_OBJECT(selection_), reinterpret_cast<gpointer>(OnSelectionChanged),
      this);
  EnableControls();
}

// static
void CookiesView::OnResponse(GtkDialog* dialog, int response_id,
                             CookiesView* window) {
  if (response_id == RESPONSE_REMOVE) {
    window->RemoveSelectedItems();
  } else if (response_id == RESPONSE_REMOVE_ALL) {
    window->cookies_tree_model_->DeleteAllCookies();
  } else {
    gtk_widget_destroy(window->dialog_);
  }
}

// static
void CookiesView::OnWindowDestroy(GtkWidget* widget, CookiesView* window) {
  instance_ = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, window);
}

// static
void CookiesView::OnSelectionChanged(GtkTreeSelection *selection,
                                     CookiesView* window) {
  window->EnableControls();
}

// static
gboolean CookiesView::OnTreeViewKeyPress(
    GtkWidget* tree_view, GdkEventKey* key, CookiesView* window) {
  if (key->keyval == GDK_Delete) {
    window->RemoveSelectedItems();
    return TRUE;
  }
  return FALSE;
}

// static
void CookiesView::OnTreeViewRowExpanded(GtkTreeView* tree_view,
                                        GtkTreeIter* iter,
                                        GtkTreePath* path,
                                        gpointer user_data) {
  // When a row in the tree is expanded, expand all the children too.
  g_signal_handlers_block_by_func(
      G_OBJECT(tree_view), reinterpret_cast<gpointer>(OnTreeViewRowExpanded),
      user_data);
  gtk_tree_view_expand_row(tree_view, path, TRUE);
  g_signal_handlers_unblock_by_func(
      G_OBJECT(tree_view), reinterpret_cast<gpointer>(OnTreeViewRowExpanded),
      user_data);
}

void CookiesView::UpdateFilterResults() {
  const gchar* text = gtk_entry_get_text(GTK_ENTRY(filter_entry_));
  if (text) {
    cookies_tree_model_->UpdateSearchResults(UTF8ToWide(text));
    SetInitialTreeState();
  }
}

// static
void CookiesView::OnFilterEntryActivated(GtkEntry* entry, CookiesView* window) {
  window->filter_update_factory_.RevokeAll();
  window->UpdateFilterResults();
}

// static
void CookiesView::OnFilterEntryChanged(GtkEditable* editable,
                                       CookiesView* window) {
  window->filter_update_factory_.RevokeAll();
  MessageLoop::current()->PostDelayedTask(FROM_HERE,
      window->filter_update_factory_.NewRunnableMethod(
          &CookiesView::UpdateFilterResults), kSearchFilterDelayMs);
  window->EnableControls();
}

// static
void CookiesView::OnFilterClearButtonClicked(GtkButton* button,
                                             CookiesView* window) {
  gtk_entry_set_text(GTK_ENTRY(window->filter_entry_), "");
  window->filter_update_factory_.RevokeAll();
  window->UpdateFilterResults();
}
