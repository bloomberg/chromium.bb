// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/collected_cookies_gtk.h"

#include "app/l10n_util.h"
#include "chrome/browser/cookies_tree_model.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"

namespace {
class CookiesTreeAdapter : public gtk_tree::TreeAdapter {
 public:
  // Column ids for the tree store.
  enum {
    COOKIES_COL_ICON,
    COOKIES_COL_TITLE,
    COOKIES_COL_NODE_PTR,
    COOKIES_COL_SETTING,
    COOKIES_COL_STYLE,
    COOKIES_COL_COUNT,
  };

  CookiesTreeAdapter(Delegate* delegate,
                     TreeModel* tree_model,
                     HostContentSettingsMap* host_content_settings_map)
      : TreeAdapter(delegate, tree_model),
        host_content_settings_map_(host_content_settings_map) {
    g_object_unref(tree_store_);
    tree_store_ = gtk_tree_store_new(COOKIES_COL_COUNT,
                                     GDK_TYPE_PIXBUF,
                                     G_TYPE_STRING,
                                     G_TYPE_POINTER,
                                     G_TYPE_STRING,
                                     PANGO_TYPE_STYLE);
  }

  virtual ~CookiesTreeAdapter() {}

 private:
  virtual void FillRow(GtkTreeIter* iter, TreeModelNode* node) {
    GdkPixbuf* pixbuf = NULL;
    int icon_index = tree_model_->GetIconIndex(node);
    if (icon_index >= 0 && icon_index < static_cast<int>(pixbufs_.size()))
      pixbuf = pixbufs_[icon_index];
    else
      pixbuf = GtkThemeProvider::GetFolderIcon(true);
    std::string setting;
    CookieTreeNode* cookie_node = static_cast<CookieTreeNode*>(node);
    if (cookie_node->GetDetailedInfo().node_type ==
        CookieTreeNode::DetailedInfo::TYPE_ORIGIN) {
      CookieTreeOriginNode* origin_node = static_cast<CookieTreeOriginNode*>(
          cookie_node);
      setting = origin_node->GetCurrentContentSettingTitle(
          host_content_settings_map_);
    }
    PangoStyle style = PANGO_STYLE_ITALIC;
    gtk_tree_store_set(tree_store_, iter,
                       COOKIES_COL_ICON, pixbuf,
                       COOKIES_COL_TITLE, WideToUTF8(node->GetTitle()).c_str(),
                       COOKIES_COL_NODE_PTR, node,
                       COOKIES_COL_SETTING, setting.c_str(),
                       COOKIES_COL_STYLE, style,
                       -1);
  }

  HostContentSettingsMap* host_content_settings_map_;

  DISALLOW_COPY_AND_ASSIGN(CookiesTreeAdapter);
};

// Widht and height of the cookie tree view.
const int kTreeViewWidth = 450;
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
  HostContentSettingsMap* host_content_settings_map =
      tab_contents_->profile()->GetHostContentSettingsMap();

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
      new CookiesTreeAdapter(this,
                             allowed_cookies_tree_model_.get(),
                             host_content_settings_map));
  allowed_tree_ = gtk_tree_view_new_with_model(
      GTK_TREE_MODEL(allowed_cookies_tree_adapter_->tree_store()));
  gtk_widget_set_size_request(allowed_tree_, kTreeViewWidth, kTreeViewHeight);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(allowed_tree_), FALSE);
  gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(allowed_tree_), TRUE);
  gtk_container_add(GTK_CONTAINER(scroll_window), allowed_tree_);

  GtkTreeViewColumn* title_column = gtk_tree_view_column_new();
  GtkCellRenderer* pixbuf_renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(title_column, pixbuf_renderer, FALSE);
  gtk_tree_view_column_add_attribute(title_column, pixbuf_renderer, "pixbuf",
                                     CookiesTreeAdapter::COOKIES_COL_ICON);
  GtkCellRenderer* title_renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(title_column, title_renderer, TRUE);
  gtk_tree_view_column_add_attribute(title_column, title_renderer, "text",
                                     CookiesTreeAdapter::COOKIES_COL_TITLE);
  gtk_tree_view_column_set_title(
      title_column, l10n_util::GetStringUTF8(
          IDS_COOKIES_DOMAIN_COLUMN_HEADER).c_str());
  gtk_tree_view_column_set_expand(title_column, TRUE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(allowed_tree_), title_column);
  GtkTreeViewColumn* setting_column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_EXCEPTIONS_ACTION_HEADER).c_str(),
      gtk_cell_renderer_text_new(),
      "text", CookiesTreeAdapter::COOKIES_COL_SETTING,
      "style", CookiesTreeAdapter::COOKIES_COL_STYLE,
      NULL);
  gtk_tree_view_column_set_alignment(setting_column, 1);
  gtk_tree_view_append_column(GTK_TREE_VIEW(allowed_tree_), setting_column);
  g_signal_connect(allowed_tree_, "row-expanded",
                   G_CALLBACK(OnTreeViewRowExpandedThunk), this);
  allowed_selection_ =
      gtk_tree_view_get_selection(GTK_TREE_VIEW(allowed_tree_));
  gtk_tree_selection_set_mode(allowed_selection_, GTK_SELECTION_MULTIPLE);
  g_signal_connect(allowed_selection_, "changed",
                   G_CALLBACK(OnTreeViewSelectionChangeThunk), this);

  GtkWidget* button_box = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_START);
  gtk_box_set_spacing(GTK_BOX(button_box), gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(dialog_), button_box, FALSE, FALSE, 0);
  block_allowed_cookie_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_COLLECTED_COOKIES_BLOCK_BUTTON).c_str());
  g_signal_connect(block_allowed_cookie_button_, "clicked",
                   G_CALLBACK(OnBlockAllowedButtonClickedThunk), this);
  gtk_container_add(GTK_CONTAINER(button_box), block_allowed_cookie_button_);

  GtkWidget* separator = gtk_hseparator_new();
  gtk_box_pack_start(GTK_BOX(dialog_), separator, TRUE, TRUE, 0);

  // Blocked Cookie list.
  cookie_list_vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(dialog_), cookie_list_vbox, TRUE, TRUE, 0);

  if (host_content_settings_map->BlockThirdPartyCookies()) {
    label = gtk_label_new(
        l10n_util::GetStringUTF8(
            IDS_COLLECTED_COOKIES_BLOCKED_THIRD_PARTY_BLOCKING_ENABLED).
                c_str());
  } else {
    label = gtk_label_new(
        l10n_util::GetStringUTF8(IDS_COLLECTED_COOKIES_BLOCKED_COOKIES_LABEL).
            c_str());
  }
  gtk_widget_set_size_request(label, kTreeViewWidth, -1);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
  gtk_box_pack_start(GTK_BOX(cookie_list_vbox), label, TRUE, TRUE, 0);

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
      new CookiesTreeAdapter(this,
                             blocked_cookies_tree_model_.get(),
                             host_content_settings_map));
  blocked_tree_ = gtk_tree_view_new_with_model(
      GTK_TREE_MODEL(blocked_cookies_tree_adapter_->tree_store()));
  gtk_widget_set_size_request(blocked_tree_, kTreeViewWidth, kTreeViewHeight);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(blocked_tree_), FALSE);
  gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(blocked_tree_), TRUE);
  gtk_container_add(GTK_CONTAINER(scroll_window), blocked_tree_);

  title_column = gtk_tree_view_column_new();
  pixbuf_renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(title_column, pixbuf_renderer, FALSE);
  gtk_tree_view_column_add_attribute(title_column, pixbuf_renderer, "pixbuf",
                                     CookiesTreeAdapter::COL_ICON);
  title_renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(title_column, title_renderer, TRUE);
  gtk_tree_view_column_add_attribute(title_column, title_renderer, "text",
                                     CookiesTreeAdapter::COL_TITLE);
  gtk_tree_view_column_set_title(
      title_column, l10n_util::GetStringUTF8(
          IDS_COOKIES_DOMAIN_COLUMN_HEADER).c_str());
  gtk_tree_view_column_set_expand(title_column, TRUE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(blocked_tree_), title_column);
  setting_column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_EXCEPTIONS_ACTION_HEADER).c_str(),
      gtk_cell_renderer_text_new(),
      "text", CookiesTreeAdapter::COOKIES_COL_SETTING,
      "style", CookiesTreeAdapter::COOKIES_COL_STYLE,
      NULL);
  gtk_tree_view_column_set_alignment(setting_column, 1);
  gtk_tree_view_append_column(GTK_TREE_VIEW(blocked_tree_), setting_column);
  g_signal_connect(blocked_tree_, "row-expanded",
                   G_CALLBACK(OnTreeViewRowExpandedThunk), this);
  blocked_selection_ =
      gtk_tree_view_get_selection(GTK_TREE_VIEW(blocked_tree_));
  gtk_tree_selection_set_mode(blocked_selection_, GTK_SELECTION_MULTIPLE);
  g_signal_connect(blocked_selection_, "changed",
                   G_CALLBACK(OnTreeViewSelectionChangeThunk), this);

  button_box = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_START);
  gtk_box_set_spacing(GTK_BOX(button_box), gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(dialog_), button_box, FALSE, FALSE, 0);
  allow_blocked_cookie_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_COLLECTED_COOKIES_ALLOW_BUTTON).c_str());
  g_signal_connect(allow_blocked_cookie_button_, "clicked",
                   G_CALLBACK(OnAllowBlockedButtonClickedThunk), this);
  gtk_container_add(GTK_CONTAINER(button_box), allow_blocked_cookie_button_);
  for_session_blocked_cookie_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_COLLECTED_COOKIES_SESSION_ONLY_BUTTON).
          c_str());
  g_signal_connect(for_session_blocked_cookie_button_, "clicked",
                   G_CALLBACK(OnForSessionBlockedButtonClickedThunk), this);
  gtk_container_add(GTK_CONTAINER(button_box),
                    for_session_blocked_cookie_button_);

  // Close button.
  button_box = gtk_hbutton_box_new();
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
  EnableControls();
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

bool CollectedCookiesGtk::SelectionContainsOriginNode(
    GtkTreeSelection* selection, gtk_tree::TreeAdapter* adapter) {
  // Check whether at least one "origin" node is selected.
  GtkTreeModel* model;
  GList* paths =
      gtk_tree_selection_get_selected_rows(selection, &model);
  bool contains_origin_node = false;
  for (GList* item = paths; item; item = item->next) {
    GtkTreeIter iter;
    gtk_tree_model_get_iter(
        model, &iter, reinterpret_cast<GtkTreePath*>(item->data));
    CookieTreeNode* node =
        static_cast<CookieTreeNode*>(adapter->GetNode(&iter));
    if (node->GetDetailedInfo().node_type !=
        CookieTreeNode::DetailedInfo::TYPE_ORIGIN)
      continue;
    CookieTreeOriginNode* origin_node = static_cast<CookieTreeOriginNode*>(
        node);
    if (!origin_node->CanCreateContentException())
      continue;
    contains_origin_node = true;
  }
  g_list_foreach(paths, reinterpret_cast<GFunc>(gtk_tree_path_free), NULL);
  g_list_free(paths);
  return contains_origin_node;
}

void CollectedCookiesGtk::EnableControls() {
  // Update button states.
  bool enable_for_allowed_cookies =
      SelectionContainsOriginNode(allowed_selection_,
                                  allowed_cookies_tree_adapter_.get());
  gtk_widget_set_sensitive(block_allowed_cookie_button_,
                           enable_for_allowed_cookies);

  bool enable_for_blocked_cookies =
      SelectionContainsOriginNode(blocked_selection_,
                                  blocked_cookies_tree_adapter_.get());
  gtk_widget_set_sensitive(allow_blocked_cookie_button_,
                           enable_for_blocked_cookies);
  gtk_widget_set_sensitive(for_session_blocked_cookie_button_,
                           enable_for_blocked_cookies);
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

void CollectedCookiesGtk::AddExceptions(GtkTreeSelection* selection,
                                        gtk_tree::TreeAdapter* adapter,
                                        ContentSetting setting) {
  GtkTreeModel* model;
  GList* paths =
      gtk_tree_selection_get_selected_rows(selection, &model);
  for (GList* item = paths; item; item = item->next) {
    GtkTreeIter iter;
    gtk_tree_model_get_iter(
        model, &iter, reinterpret_cast<GtkTreePath*>(item->data));
    CookieTreeOriginNode* node = static_cast<CookieTreeOriginNode*>(
        adapter->GetNode(&iter));
    if (node->CanCreateContentException()) {
      node->CreateContentException(
          tab_contents_->profile()->GetHostContentSettingsMap(), setting);
      adapter->TreeNodeChanged(adapter->tree_model(), adapter->GetNode(&iter));
    }
  }
  g_list_foreach(paths, reinterpret_cast<GFunc>(gtk_tree_path_free), NULL);
  g_list_free(paths);
}

void CollectedCookiesGtk::OnBlockAllowedButtonClicked(GtkWidget* button) {
  AddExceptions(allowed_selection_, allowed_cookies_tree_adapter_.get(),
                CONTENT_SETTING_BLOCK);
}

void CollectedCookiesGtk::OnAllowBlockedButtonClicked(GtkWidget* button) {
  AddExceptions(blocked_selection_, blocked_cookies_tree_adapter_.get(),
                CONTENT_SETTING_ALLOW);
}

void CollectedCookiesGtk::OnForSessionBlockedButtonClicked(GtkWidget* button) {
  AddExceptions(blocked_selection_, blocked_cookies_tree_adapter_.get(),
                CONTENT_SETTING_SESSION_ONLY);
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

void CollectedCookiesGtk::OnTreeViewSelectionChange(GtkWidget* selection) {
  EnableControls();
}
