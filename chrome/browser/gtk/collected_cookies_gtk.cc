// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/collected_cookies_gtk.h"

#include "app/gtk_util.h"
#include "app/l10n_util.h"
#include "chrome/browser/cookies_tree_model.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"

namespace {
// Height of the cookie tree view.
const int kTreeViewHeight = 150;
}  // namespace

CollectedCookiesGtk::CollectedCookiesGtk(GtkWindow* parent,
                                         TabContents* tab_contents)
    : tab_contents_(tab_contents) {
  TabSpecificContentSettings* content_settings =
      tab_contents->GetTabSpecificContentSettings();
  registrar_.Add(this, NotificationType::COLLECTED_COOKIES_SHOWN,
                 Source<TabSpecificContentSettings>(content_settings));

  Init();
}

void CollectedCookiesGtk::Init() {
  dialog_ = gtk_vbox_new(FALSE, gtk_util::kContentAreaSpacing);
  gtk_box_set_spacing(GTK_BOX(dialog_), gtk_util::kContentAreaSpacing);

  GtkWidget* label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_COLLECTED_COOKIES_DIALOG_TITLE).c_str());
  gtk_box_pack_start(GTK_BOX(dialog_), label, TRUE, TRUE, 0);

  // Allowed Cookie list.
  GtkWidget* cookie_list_vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(dialog_), cookie_list_vbox, TRUE, TRUE, 0);

  label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_COLLECTED_COOKIES_ALLOWED_COOKIES_LABEL).
          c_str());
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
  gtk_box_pack_start(GTK_BOX(cookie_list_vbox), label, FALSE, FALSE, 0);

  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(cookie_list_vbox), scroll_window, TRUE, TRUE, 0);

  TabSpecificContentSettings* content_settings =
      tab_contents_->GetTabSpecificContentSettings();

  allowed_cookies_tree_model_.reset(
      content_settings->GetAllowedCookiesTreeModel());
  allowed_cookies_tree_adapter_.reset(
      new gtk_tree::TreeAdapter(this, allowed_cookies_tree_model_.get()));
  allowed_tree_ = gtk_tree_view_new_with_model(
      GTK_TREE_MODEL(allowed_cookies_tree_adapter_->tree_store()));
  gtk_widget_set_size_request(allowed_tree_, -1, kTreeViewHeight);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(allowed_tree_), FALSE);
  gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(allowed_tree_), TRUE);
  gtk_container_add(GTK_CONTAINER(scroll_window), allowed_tree_);

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
  gtk_tree_view_append_column(GTK_TREE_VIEW(allowed_tree_), title_column);
  g_signal_connect(allowed_tree_, "row-expanded",
                   G_CALLBACK(OnTreeViewRowExpandedThunk), this);

  // Blocked Cookie list.
  cookie_list_vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(dialog_), cookie_list_vbox, TRUE, TRUE, 0);

  label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_COLLECTED_COOKIES_BLOCKED_COOKIES_LABEL).
          c_str());
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
  gtk_box_pack_start(GTK_BOX(cookie_list_vbox), label, FALSE, FALSE, 0);

  scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(cookie_list_vbox), scroll_window, TRUE, TRUE, 0);

  blocked_cookies_tree_model_.reset(
      content_settings->GetBlockedCookiesTreeModel());
  blocked_cookies_tree_adapter_.reset(
      new gtk_tree::TreeAdapter(this, blocked_cookies_tree_model_.get()));
  blocked_tree_ = gtk_tree_view_new_with_model(
      GTK_TREE_MODEL(blocked_cookies_tree_adapter_->tree_store()));
  gtk_widget_set_size_request(blocked_tree_, -1, kTreeViewHeight);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(blocked_tree_), FALSE);
  gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(blocked_tree_), TRUE);
  gtk_container_add(GTK_CONTAINER(scroll_window), blocked_tree_);

  title_column = gtk_tree_view_column_new();
  pixbuf_renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(title_column, pixbuf_renderer, FALSE);
  gtk_tree_view_column_add_attribute(title_column, pixbuf_renderer, "pixbuf",
                                     gtk_tree::TreeAdapter::COL_ICON);
  title_renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(title_column, title_renderer, TRUE);
  gtk_tree_view_column_add_attribute(title_column, title_renderer, "text",
                                     gtk_tree::TreeAdapter::COL_TITLE);
  gtk_tree_view_column_set_title(
      title_column, l10n_util::GetStringUTF8(
          IDS_COOKIES_DOMAIN_COLUMN_HEADER).c_str());
  gtk_tree_view_append_column(GTK_TREE_VIEW(blocked_tree_), title_column);
  g_signal_connect(blocked_tree_, "row-expanded",
                   G_CALLBACK(OnTreeViewRowExpandedThunk), this);

  // Close button.
  GtkWidget* button_box = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
  gtk_box_set_spacing(GTK_BOX(button_box), gtk_util::kControlSpacing);
  gtk_box_pack_end(GTK_BOX(dialog_), button_box, FALSE, TRUE, 0);
  GtkWidget* close = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
  gtk_button_set_label(GTK_BUTTON(close),
                       l10n_util::GetStringUTF8(IDS_CLOSE).c_str());
  g_signal_connect(close, "clicked", G_CALLBACK(OnCloseThunk), this);
  gtk_box_pack_end(GTK_BOX(button_box), close, FALSE, TRUE, 0);

  // Show the dialog.
  allowed_cookies_tree_adapter_->Init();
  blocked_cookies_tree_adapter_->Init();
  window_ = tab_contents_->CreateConstrainedDialog(this);
}

CollectedCookiesGtk::~CollectedCookiesGtk() {
  gtk_widget_destroy(dialog_);
}

GtkWidget* CollectedCookiesGtk::GetWidgetRoot() {
  return dialog_;
}

void CollectedCookiesGtk::DeleteDelegate() {
  delete this;
}

void CollectedCookiesGtk::Observe(NotificationType type,
                                  const NotificationSource& source,
                                  const NotificationDetails& details) {
  DCHECK(type == NotificationType::COLLECTED_COOKIES_SHOWN);
  DCHECK_EQ(Source<TabSpecificContentSettings>(source).ptr(),
            tab_contents_->GetTabSpecificContentSettings());
  window_->CloseConstrainedWindow();
}

void CollectedCookiesGtk::OnClose(GtkWidget* close_button) {
  window_->CloseConstrainedWindow();
}

void CollectedCookiesGtk::OnTreeViewRowExpanded(GtkWidget* tree_view,
                                        GtkTreeIter* iter,
                                        GtkTreePath* path) {
  // When a row in the tree is expanded, expand all the children too.
  g_signal_handlers_block_by_func(
      tree_view, reinterpret_cast<gpointer>(OnTreeViewRowExpandedThunk), this);
  gtk_tree_view_expand_row(GTK_TREE_VIEW(tree_view), path, TRUE);
  g_signal_handlers_unblock_by_func(
      tree_view, reinterpret_cast<gpointer>(OnTreeViewRowExpandedThunk), this);
}
