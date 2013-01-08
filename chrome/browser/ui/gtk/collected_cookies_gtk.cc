// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/collected_cookies_gtk.h"

#include <string>
#include "chrome/browser/api/infobars/infobar_service.h"
#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/browsing_data_server_bound_cert_helper.h"
#include "chrome/browser/browsing_data/cookies_tree_model.h"
#include "chrome/browser/browsing_data/local_data_container.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/content_settings/local_shared_objects_container.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/collected_cookies_infobar_delegate.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_chrome_cookie_view.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "net/cookies/canonical_cookie.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
// Width and height of the cookie tree view.
const int kTreeViewWidth = 450;
const int kTreeViewHeight = 150;

// The page numbers of the pages in the notebook.
const gint kAllowedPageNumber = 0;
const gint kBlockedPageNumber = 1;

// Padding within the banner box.
const int kBannerPadding = 3;

// Returns the text to display in the info bar when content settings were
// created.
const std::string GetInfobarLabel(ContentSetting setting,
                                  bool multiple_domains_added,
                                  const string16& domain_name) {
  if (multiple_domains_added) {
    switch (setting) {
      case CONTENT_SETTING_BLOCK:
        return l10n_util::GetStringUTF8(
            IDS_COLLECTED_COOKIES_MULTIPLE_BLOCK_RULES_CREATED);

      case CONTENT_SETTING_ALLOW:
        return l10n_util::GetStringUTF8(
            IDS_COLLECTED_COOKIES_MULTIPLE_ALLOW_RULES_CREATED);

      case CONTENT_SETTING_SESSION_ONLY:
        return l10n_util::GetStringUTF8(
            IDS_COLLECTED_COOKIES_MULTIPLE_SESSION_RULES_CREATED);

      default:
        NOTREACHED();
        return std::string();
    }
  }

  switch (setting) {
    case CONTENT_SETTING_BLOCK:
      return l10n_util::GetStringFUTF8(
          IDS_COLLECTED_COOKIES_BLOCK_RULE_CREATED, domain_name);

    case CONTENT_SETTING_ALLOW:
      return l10n_util::GetStringFUTF8(
          IDS_COLLECTED_COOKIES_ALLOW_RULE_CREATED, domain_name);

    case CONTENT_SETTING_SESSION_ONLY:
      return l10n_util::GetStringFUTF8(
          IDS_COLLECTED_COOKIES_SESSION_RULE_CREATED, domain_name);

    default:
      NOTREACHED();
      return std::string();
  }
}

}  // namespace

namespace chrome {

// Declared in browser_dialogs.h so others don't have to depend on our header.
void ShowCollectedCookiesDialog(content::WebContents* web_contents) {
  // Deletes itself on close.
  new CollectedCookiesGtk(
      web_contents->GetView()->GetTopLevelNativeWindow(),
      web_contents);
}

}  // namespace chrome

CollectedCookiesGtk::CollectedCookiesGtk(GtkWindow* parent,
                                         content::WebContents* web_contents)
    : web_contents_(web_contents),
      status_changed_(false) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  registrar_.Add(this, chrome::NOTIFICATION_COLLECTED_COOKIES_SHOWN,
                 content::Source<TabSpecificContentSettings>(content_settings));

  Init();
}

void CollectedCookiesGtk::Init() {
  dialog_ = gtk_vbox_new(FALSE, ui::kContentAreaSpacing);
  gtk_box_set_spacing(GTK_BOX(dialog_), ui::kContentAreaSpacing);

  GtkWidget* label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_COLLECTED_COOKIES_DIALOG_TITLE).c_str());
  gtk_box_pack_start(GTK_BOX(dialog_), label, TRUE, TRUE, 0);

  notebook_ = gtk_notebook_new();
  gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook_), GTK_POS_TOP);
  gtk_box_pack_start(GTK_BOX(dialog_), notebook_, TRUE, TRUE, 0);

  GtkWidget* allowed_pane = CreateAllowedPane();
  label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_COLLECTED_COOKIES_ALLOWED_COOKIES_TAB_LABEL)
          .c_str());
  gtk_widget_show(label);
  gtk_notebook_insert_page(GTK_NOTEBOOK(notebook_), allowed_pane, label,
                           kAllowedPageNumber);

  GtkWidget* blocked_pane = CreateBlockedPane();
  label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_COLLECTED_COOKIES_BLOCKED_COOKIES_TAB_LABEL)
          .c_str());
  gtk_widget_show(label);
  gtk_notebook_insert_page(GTK_NOTEBOOK(notebook_), blocked_pane, label,
                           kBlockedPageNumber);
  // Hook up the signal only after all pages are inserted, otherwise not
  // all member variables used in OnSwitchPage() will be properly initialized.
  g_signal_connect(notebook_, "switch-page", G_CALLBACK(OnSwitchPageThunk),
                   this);

  // Cookie info view.
  cookie_info_view_ = gtk_chrome_cookie_view_new(false);
  gtk_box_pack_start(GTK_BOX(dialog_), cookie_info_view_, TRUE, TRUE, 0);
  gtk_chrome_cookie_view_clear(GTK_CHROME_COOKIE_VIEW(cookie_info_view_));
  gtk_widget_show_all(cookie_info_view_);

  // Infobar.
  infobar_ = gtk_frame_new(NULL);
  GtkWidget* infobar_contents = gtk_hbox_new(FALSE, kBannerPadding);
  gtk_container_set_border_width(GTK_CONTAINER(infobar_contents),
                                 kBannerPadding);
  gtk_container_add(GTK_CONTAINER(infobar_), infobar_contents);
  GtkWidget* info_image =
      gtk_image_new_from_stock(GTK_STOCK_DIALOG_INFO,
                               GTK_ICON_SIZE_SMALL_TOOLBAR);
  gtk_box_pack_start(GTK_BOX(infobar_contents), info_image, FALSE, FALSE, 0);
  infobar_label_ = gtk_label_new(NULL);
  gtk_box_pack_start(
      GTK_BOX(infobar_contents), infobar_label_, FALSE, FALSE, 0);
  gtk_widget_show_all(infobar_);
  gtk_widget_set_no_show_all(infobar_, TRUE);
  gtk_widget_hide(infobar_);
  gtk_box_pack_start(GTK_BOX(dialog_), infobar_, TRUE, TRUE, 0);

  // Close button.
  GtkWidget* button_box = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_END);
  gtk_box_set_spacing(GTK_BOX(button_box), ui::kControlSpacing);
  gtk_box_pack_end(GTK_BOX(dialog_), button_box, FALSE, TRUE, 0);
  close_button_ = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
  gtk_button_set_label(GTK_BUTTON(close_button_),
                       l10n_util::GetStringUTF8(IDS_CLOSE).c_str());
  g_signal_connect(close_button_, "clicked", G_CALLBACK(OnCloseThunk), this);
  gtk_box_pack_end(GTK_BOX(button_box), close_button_, FALSE, TRUE, 0);

  // Show the dialog.
  allowed_cookies_tree_adapter_->Init();
  blocked_cookies_tree_adapter_->Init();
  EnableControls();
  ShowCookieInfo(gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook_)));
  window_ = new ConstrainedWindowGtk(web_contents_, this);
}

GtkWidget* CollectedCookiesGtk::CreateAllowedPane() {
  GtkWidget* cookie_list_vbox = gtk_vbox_new(FALSE, ui::kControlSpacing);

  GtkWidget* label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_COLLECTED_COOKIES_ALLOWED_COOKIES_LABEL).
          c_str());
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
  gtk_box_pack_start(GTK_BOX(cookie_list_vbox), label, FALSE, FALSE,
                     ui::kControlSpacing);

  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(cookie_list_vbox), scroll_window, TRUE, TRUE, 0);

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents_);

  const LocalSharedObjectsContainer& allowed_data =
      content_settings->allowed_local_shared_objects();
  allowed_cookies_tree_model_ = allowed_data.CreateCookiesTreeModel();

  allowed_cookies_tree_adapter_.reset(
      new gtk_tree::TreeAdapter(this, allowed_cookies_tree_model_.get()));
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
  allowed_selection_ =
      gtk_tree_view_get_selection(GTK_TREE_VIEW(allowed_tree_));
  gtk_tree_selection_set_mode(allowed_selection_, GTK_SELECTION_MULTIPLE);
  g_signal_connect(allowed_selection_, "changed",
                   G_CALLBACK(OnTreeViewSelectionChangeThunk), this);

  GtkWidget* button_box = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_START);
  gtk_box_set_spacing(GTK_BOX(button_box), ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(cookie_list_vbox), button_box, FALSE, FALSE,
                     ui::kControlSpacing);
  block_allowed_cookie_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_COLLECTED_COOKIES_BLOCK_BUTTON).c_str());
  g_signal_connect(block_allowed_cookie_button_, "clicked",
                   G_CALLBACK(OnBlockAllowedButtonClickedThunk), this);
  gtk_container_add(GTK_CONTAINER(button_box), block_allowed_cookie_button_);

  // Wrap the vbox inside an hbox so that we can specify padding along the
  // horizontal axis.
  GtkWidget* box = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), cookie_list_vbox, TRUE, TRUE,
                     ui::kControlSpacing);
  return box;
}

GtkWidget* CollectedCookiesGtk::CreateBlockedPane() {
  PrefService* prefs = Profile::FromBrowserContext(
      web_contents_->GetBrowserContext())->GetPrefs();

  GtkWidget* cookie_list_vbox = gtk_vbox_new(FALSE, ui::kControlSpacing);

  GtkWidget* label = gtk_label_new(
      l10n_util::GetStringUTF8(
          prefs->GetBoolean(prefs::kBlockThirdPartyCookies) ?
              IDS_COLLECTED_COOKIES_BLOCKED_THIRD_PARTY_BLOCKING_ENABLED :
              IDS_COLLECTED_COOKIES_BLOCKED_COOKIES_LABEL).c_str());
  gtk_widget_set_size_request(label, kTreeViewWidth, -1);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
  gtk_box_pack_start(GTK_BOX(cookie_list_vbox), label, TRUE, TRUE,
                     ui::kControlSpacing);

  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_box_pack_start(GTK_BOX(cookie_list_vbox), scroll_window, TRUE, TRUE, 0);

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents_);

  const LocalSharedObjectsContainer& blocked_data =
      content_settings->blocked_local_shared_objects();
  blocked_cookies_tree_model_ = blocked_data.CreateCookiesTreeModel();

  blocked_cookies_tree_adapter_.reset(
      new gtk_tree::TreeAdapter(this, blocked_cookies_tree_model_.get()));
  blocked_tree_ = gtk_tree_view_new_with_model(
      GTK_TREE_MODEL(blocked_cookies_tree_adapter_->tree_store()));
  gtk_widget_set_size_request(blocked_tree_, kTreeViewWidth, kTreeViewHeight);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(blocked_tree_), FALSE);
  gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(blocked_tree_), TRUE);
  gtk_container_add(GTK_CONTAINER(scroll_window), blocked_tree_);

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
  gtk_tree_view_append_column(GTK_TREE_VIEW(blocked_tree_), title_column);
  g_signal_connect(blocked_tree_, "row-expanded",
                   G_CALLBACK(OnTreeViewRowExpandedThunk), this);
  blocked_selection_ =
      gtk_tree_view_get_selection(GTK_TREE_VIEW(blocked_tree_));
  gtk_tree_selection_set_mode(blocked_selection_, GTK_SELECTION_MULTIPLE);
  g_signal_connect(blocked_selection_, "changed",
                   G_CALLBACK(OnTreeViewSelectionChangeThunk), this);

  GtkWidget* button_box = gtk_hbutton_box_new();
  gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_START);
  gtk_box_set_spacing(GTK_BOX(button_box), ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(cookie_list_vbox), button_box, FALSE, FALSE,
                     ui::kControlSpacing);
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

  // Wrap the vbox inside an hbox so that we can specify padding along the
  // horizontal axis.
  GtkWidget* box = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(box), cookie_list_vbox, TRUE, TRUE,
                     ui::kControlSpacing);
  return box;
}

void CollectedCookiesGtk::ShowCookieInfo(gint current_page) {
  if (current_page == kAllowedPageNumber) {
    ShowSelectionInfo(allowed_selection_, allowed_cookies_tree_adapter_.get());
  } else if (current_page == kBlockedPageNumber) {
    ShowSelectionInfo(blocked_selection_, blocked_cookies_tree_adapter_.get());
  }
}

void CollectedCookiesGtk::ShowSelectionInfo(GtkTreeSelection* selection,
                                            gtk_tree::TreeAdapter* adapter) {
  // Check if one "cookie" node is selected.  Don't allow more than one.
  GtkTreeModel* model;
  GList* paths = gtk_tree_selection_get_selected_rows(selection, &model);
  if (g_list_length(paths) == 1) {
    GList* item = g_list_first(paths);
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter,
                            reinterpret_cast<GtkTreePath*>(item->data));
    CookieTreeNode* node =
        static_cast<CookieTreeNode*>(adapter->GetNode(&iter));
    const CookieTreeNode::DetailedInfo detailed_info = node->GetDetailedInfo();
    if (detailed_info.node_type == CookieTreeNode::DetailedInfo::TYPE_COOKIE) {
      gtk_chrome_cookie_view_display_cookie(
          GTK_CHROME_COOKIE_VIEW(cookie_info_view_),
          detailed_info.cookie->Domain(),
          *detailed_info.cookie);
    } else {
      gtk_chrome_cookie_view_clear(GTK_CHROME_COOKIE_VIEW(cookie_info_view_));
    }
  } else {
    gtk_chrome_cookie_view_clear(GTK_CHROME_COOKIE_VIEW(cookie_info_view_));
  }

  g_list_foreach(paths, reinterpret_cast<GFunc>(gtk_tree_path_free), NULL);
  g_list_free(paths);
}

CollectedCookiesGtk::~CollectedCookiesGtk() {
  gtk_widget_destroy(dialog_);
}

GtkWidget* CollectedCookiesGtk::GetWidgetRoot() {
  return dialog_;
}

GtkWidget* CollectedCookiesGtk::GetFocusWidget() {
  return close_button_;
}

void CollectedCookiesGtk::DeleteDelegate() {
  delete this;
}

bool CollectedCookiesGtk::SelectionContainsHostNode(
    GtkTreeSelection* selection, gtk_tree::TreeAdapter* adapter) {
  // Check whether at least one "origin" node is selected.
  GtkTreeModel* model;
  GList* paths =
      gtk_tree_selection_get_selected_rows(selection, &model);
  bool contains_host_node = false;
  for (GList* item = paths; item; item = item->next) {
    GtkTreeIter iter;
    gtk_tree_model_get_iter(
        model, &iter, reinterpret_cast<GtkTreePath*>(item->data));
    CookieTreeNode* node =
        static_cast<CookieTreeNode*>(adapter->GetNode(&iter));
    if (node->GetDetailedInfo().node_type !=
        CookieTreeNode::DetailedInfo::TYPE_HOST)
      continue;
    CookieTreeHostNode* host_node = static_cast<CookieTreeHostNode*>(
        node);
    if (!host_node->CanCreateContentException())
      continue;
    contains_host_node = true;
  }
  g_list_foreach(paths, reinterpret_cast<GFunc>(gtk_tree_path_free), NULL);
  g_list_free(paths);
  return contains_host_node;
}

void CollectedCookiesGtk::EnableControls() {
  // Update button states.
  bool enable_for_allowed_cookies =
      SelectionContainsHostNode(allowed_selection_,
                                  allowed_cookies_tree_adapter_.get());
  gtk_widget_set_sensitive(block_allowed_cookie_button_,
                           enable_for_allowed_cookies);

  bool enable_for_blocked_cookies =
      SelectionContainsHostNode(blocked_selection_,
                                  blocked_cookies_tree_adapter_.get());
  gtk_widget_set_sensitive(allow_blocked_cookie_button_,
                           enable_for_blocked_cookies);
  gtk_widget_set_sensitive(for_session_blocked_cookie_button_,
                           enable_for_blocked_cookies);
}

void CollectedCookiesGtk::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_COLLECTED_COOKIES_SHOWN);
  window_->CloseWebContentsModalDialog();
}

void CollectedCookiesGtk::OnClose(GtkWidget* close_button) {
  if (status_changed_) {
    CollectedCookiesInfoBarDelegate::Create(
        InfoBarService::FromWebContents(web_contents_));
  }
  window_->CloseWebContentsModalDialog();
}

void CollectedCookiesGtk::AddExceptions(GtkTreeSelection* selection,
                                        gtk_tree::TreeAdapter* adapter,
                                        ContentSetting setting) {
  GtkTreeModel* model;
  GList* paths =
      gtk_tree_selection_get_selected_rows(selection, &model);
  string16 last_domain_name;
  bool multiple_domains_added = false;
  for (GList* item = paths; item; item = item->next) {
    GtkTreeIter iter;
    gtk_tree_model_get_iter(
        model, &iter, reinterpret_cast<GtkTreePath*>(item->data));
    CookieTreeNode* node =
        static_cast<CookieTreeNode*>(adapter->GetNode(&iter));
    if (node->GetDetailedInfo().node_type !=
        CookieTreeNode::DetailedInfo::TYPE_HOST)
      continue;
    CookieTreeHostNode* host_node = static_cast<CookieTreeHostNode*>(
        node);
    if (host_node->CanCreateContentException()) {
      if (!last_domain_name.empty())
        multiple_domains_added = true;
      last_domain_name = host_node->GetTitle();
      Profile* profile =
          Profile::FromBrowserContext(web_contents_->GetBrowserContext());
      host_node->CreateContentException(
          CookieSettings::Factory::GetForProfile(profile), setting);
    }
  }
  g_list_foreach(paths, reinterpret_cast<GFunc>(gtk_tree_path_free), NULL);
  g_list_free(paths);
  if (last_domain_name.empty()) {
    gtk_widget_hide(infobar_);
  } else {
    gtk_label_set_text(
        GTK_LABEL(infobar_label_), GetInfobarLabel(
            setting, multiple_domains_added, last_domain_name).c_str());
    gtk_widget_show(infobar_);
  }
  status_changed_ = true;
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

void CollectedCookiesGtk::OnSwitchPage(GtkWidget* notebook,
                                       gpointer page,
                                       guint page_num) {
  EnableControls();
  ShowCookieInfo(page_num);
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
  ShowCookieInfo(gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook_)));
}
