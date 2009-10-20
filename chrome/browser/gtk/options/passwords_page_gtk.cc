// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/passwords_page_gtk.h"

#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/common/gtk_tree.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/pref_service.h"
#include "chrome/common/url_constants.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"

namespace {

// Initial width of the first column.
const int kSiteColumnInitialSize = 265;

// Column ids for |password_list_store_|.
enum {
  COL_SITE,
  COL_USERNAME,
  COL_COUNT,
};

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// PasswordsPageGtk, public:

PasswordsPageGtk::PasswordsPageGtk(Profile* profile)
    : populater(this), password_showing_(false), profile_(profile) {

  remove_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_PASSWORDS_PAGE_VIEW_REMOVE_BUTTON).c_str());
  gtk_widget_set_sensitive(remove_button_, FALSE);
  g_signal_connect(G_OBJECT(remove_button_), "clicked",
                   G_CALLBACK(OnRemoveButtonClicked), this);
  remove_all_button_ = gtk_button_new_with_label(l10n_util::GetStringUTF8(
          IDS_PASSWORDS_PAGE_VIEW_REMOVE_ALL_BUTTON).c_str());
  gtk_widget_set_sensitive(remove_all_button_, FALSE);
  g_signal_connect(G_OBJECT(remove_all_button_), "clicked",
                   G_CALLBACK(OnRemoveAllButtonClicked), this);

  show_password_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON).c_str());
  GtkRequisition hide_size, show_size;
  // Get the size request of the button with the "hide password" text.
  gtk_widget_size_request(show_password_button_, &hide_size);
  gtk_button_set_label(GTK_BUTTON(show_password_button_),
      l10n_util::GetStringUTF8(IDS_PASSWORDS_PAGE_VIEW_SHOW_BUTTON).c_str());
  // Get the size request of the button with the "show password" text.
  gtk_widget_size_request(show_password_button_, &show_size);
  // Determine the maximum width and height.
  if (hide_size.width > show_size.width)
    show_size.width = hide_size.width;
  if (hide_size.height > show_size.height)
    show_size.height = hide_size.height;
  // Force the button to be large enough for both labels.
  gtk_widget_set_size_request(show_password_button_, show_size.width,
                              show_size.height);
  gtk_widget_set_sensitive(show_password_button_, FALSE);
  g_signal_connect(G_OBJECT(show_password_button_), "clicked",
                   G_CALLBACK(OnShowPasswordButtonClicked), this);

  password_ = gtk_label_new("");

  GtkWidget* buttons = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(buttons), remove_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(buttons), remove_all_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(buttons), show_password_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(buttons), password_, FALSE, FALSE, 0);

  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);

  // Sets password_tree_ among other things.
  InitPasswordTree();
  gtk_container_add(GTK_CONTAINER(scroll_window), password_tree_);

  page_ = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(page_),
                                 gtk_util::kContentAreaBorder);
  gtk_box_pack_end(GTK_BOX(page_), buttons, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(page_), scroll_window, TRUE, TRUE, 0);
}

PasswordsPageGtk::~PasswordsPageGtk() {
}

///////////////////////////////////////////////////////////////////////////////
// PasswordsPageGtk, private:

void PasswordsPageGtk::InitPasswordTree() {
  password_list_store_ = gtk_list_store_new(COL_COUNT,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING);
  password_list_sort_ = gtk_tree_model_sort_new_with_model(
      GTK_TREE_MODEL(password_list_store_));
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(password_list_sort_),
                                  COL_SITE, CompareSite, this, NULL);
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(password_list_sort_),
                                  COL_USERNAME, CompareUsername, this, NULL);
  password_tree_ = gtk_tree_view_new_with_model(password_list_sort_);
  g_object_unref(password_list_store_);
  g_object_unref(password_list_sort_);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(password_tree_), TRUE);

  password_selection_ = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(password_tree_));
  gtk_tree_selection_set_mode(password_selection_,
                              GTK_SELECTION_SINGLE);
  g_signal_connect(G_OBJECT(password_selection_), "changed",
                   G_CALLBACK(OnPasswordSelectionChanged), this);

  GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN).c_str(),
      gtk_cell_renderer_text_new(),
      "text", COL_SITE,
      NULL);
  gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_column_set_fixed_width(column, kSiteColumnInitialSize);
  gtk_tree_view_column_set_sort_column_id(column, COL_SITE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(password_tree_), column);

  column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_PASSWORDS_PAGE_VIEW_USERNAME_COLUMN).c_str(),
      gtk_cell_renderer_text_new(),
      "text", COL_USERNAME,
      NULL);
  gtk_tree_view_column_set_sort_column_id(column, COL_USERNAME);
  gtk_tree_view_append_column(GTK_TREE_VIEW(password_tree_), column);
  populater.populate();
}

PasswordStore* PasswordsPageGtk::GetPasswordStore() {
    return profile_->GetPasswordStore(Profile::EXPLICIT_ACCESS);
}

void PasswordsPageGtk::SetPasswordList(
    const std::vector<webkit_glue::PasswordForm*>& result) {
  std::wstring languages =
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
  gtk_list_store_clear(password_list_store_);
  password_list_.resize(result.size());
  for (size_t i = 0; i < result.size(); ++i) {
    password_list_[i] = *result[i];
    std::wstring formatted = net::FormatUrl(result[i]->origin, languages,
                                            false, UnescapeRule::NONE,
                                            NULL, NULL);
    std::string site = WideToUTF8(formatted);
    std::string user = UTF16ToUTF8(result[i]->username_value);
    GtkTreeIter iter;
    gtk_list_store_insert_with_values(password_list_store_, &iter, (gint) i,
                                      COL_SITE, site.c_str(),
                                      COL_USERNAME, user.c_str(), -1);
  }
  gtk_widget_set_sensitive(remove_all_button_, result.size() > 0);
}

// static
void PasswordsPageGtk::OnRemoveButtonClicked(GtkButton* widget,
                                             PasswordsPageGtk* page) {
  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(page->password_selection_,
                                       NULL, &iter)) {
    NOTREACHED();
    return;
  }

  GtkTreePath* path = gtk_tree_model_get_path(
      GTK_TREE_MODEL(page->password_list_sort_), &iter);
  gint index = gtk_tree::GetTreeSortChildRowNumForPath(
      page->password_list_sort_, path);
  gtk_tree_path_free(path);

  GtkTreeIter child_iter;
  gtk_tree_model_sort_convert_iter_to_child_iter(
      GTK_TREE_MODEL_SORT(page->password_list_sort_), &child_iter, &iter);

  // Remove from GTK list, DB, and vector.
  gtk_list_store_remove(page->password_list_store_, &child_iter);
  page->GetPasswordStore()->RemoveLogin(page->password_list_[index]);
  page->password_list_.erase(page->password_list_.begin() + index);

  gtk_widget_set_sensitive(page->remove_all_button_,
                           page->password_list_.size() > 0);
}

// static
void PasswordsPageGtk::OnRemoveAllButtonClicked(GtkButton* widget,
                                                PasswordsPageGtk* page) {
  GtkWindow* window = GTK_WINDOW(gtk_widget_get_toplevel(page->page_));
  GtkWidget* confirm = gtk_message_dialog_new(window,
                                              GTK_DIALOG_DESTROY_WITH_PARENT,
                                              GTK_MESSAGE_QUESTION,
                                              GTK_BUTTONS_YES_NO,
                                              "%s",
                                              l10n_util::GetStringUTF8(
          IDS_PASSWORDS_PAGE_VIEW_TEXT_DELETE_ALL_PASSWORDS).c_str());
  gtk_window_set_modal(GTK_WINDOW(confirm), TRUE);
  gtk_window_set_title(GTK_WINDOW(confirm), l10n_util::GetStringUTF8(
          IDS_PASSWORDS_PAGE_VIEW_CAPTION_DELETE_ALL_PASSWORDS).c_str());
  g_signal_connect(confirm, "response", G_CALLBACK(OnRemoveAllConfirmResponse),
                   page);
  gtk_widget_show_all(confirm);
}

// static
void PasswordsPageGtk::OnRemoveAllConfirmResponse(GtkDialog* confirm,
                                                  gint response,
                                                  PasswordsPageGtk* page) {
  bool confirmed = false;
  switch (response) {
    case GTK_RESPONSE_YES:
      confirmed = true;
      break;
    default:
      break;
  }
  gtk_widget_destroy(GTK_WIDGET(confirm));
  if (!confirmed)
    return;

  // Remove from GTK list, DB, and vector.
  PasswordStore* store = page->GetPasswordStore();
  gtk_list_store_clear(page->password_list_store_);
  for (size_t i = 0; i < page->password_list_.size(); ++i) {
    store->RemoveLogin(page->password_list_[i]);
  }
  page->password_list_.clear();
  gtk_widget_set_sensitive(page->remove_all_button_, FALSE);
}

// static
void PasswordsPageGtk::OnShowPasswordButtonClicked(GtkButton* widget,
                                                   PasswordsPageGtk* page) {
  page->password_showing_ = !page->password_showing_;
  if (!page->password_showing_) {
    // Hide the password.
    gtk_label_set_text(GTK_LABEL(page->password_), "");
    gtk_button_set_label(GTK_BUTTON(page->show_password_button_),
        l10n_util::GetStringUTF8(IDS_PASSWORDS_PAGE_VIEW_SHOW_BUTTON).c_str());
    return;
  }
  // Show the password.
  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(page->password_selection_,
                                       NULL, &iter)) {
    NOTREACHED();
    return;
  }
  GtkTreePath* path = gtk_tree_model_get_path(
      GTK_TREE_MODEL(page->password_list_sort_), &iter);
  gint index = gtk_tree::GetTreeSortChildRowNumForPath(
      page->password_list_sort_, path);
  gtk_tree_path_free(path);
  std::string pass = UTF16ToUTF8(page->password_list_[index].password_value);
  gtk_label_set_text(GTK_LABEL(page->password_), pass.c_str());
  gtk_button_set_label(GTK_BUTTON(page->show_password_button_),
      l10n_util::GetStringUTF8(IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON).c_str());
}

// static
void PasswordsPageGtk::OnPasswordSelectionChanged(GtkTreeSelection* selection,
                                                  PasswordsPageGtk* page) {
  // No matter how the selection changed, we want to hide the old password.
  gtk_label_set_text(GTK_LABEL(page->password_), "");
  gtk_button_set_label(GTK_BUTTON(page->show_password_button_),
      l10n_util::GetStringUTF8(IDS_PASSWORDS_PAGE_VIEW_SHOW_BUTTON).c_str());
  page->password_showing_ = false;

  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
    gtk_widget_set_sensitive(page->show_password_button_, FALSE);
    gtk_widget_set_sensitive(page->remove_button_, FALSE);
    return;
  }
  gtk_widget_set_sensitive(page->show_password_button_, TRUE);
  gtk_widget_set_sensitive(page->remove_button_, TRUE);
}

// static
gint PasswordsPageGtk::CompareSite(GtkTreeModel* model,
                                   GtkTreeIter* a, GtkTreeIter* b,
                                   gpointer window) {
  int row1 = gtk_tree::GetRowNumForIter(model, a);
  int row2 = gtk_tree::GetRowNumForIter(model, b);
  PasswordsPageGtk* page = reinterpret_cast<PasswordsPageGtk*>(window);
  return page->password_list_[row1].origin.spec().compare(
         page->password_list_[row2].origin.spec());
}

// static
gint PasswordsPageGtk::CompareUsername(GtkTreeModel* model,
                                       GtkTreeIter* a, GtkTreeIter* b,
                                       gpointer window) {
  int row1 = gtk_tree::GetRowNumForIter(model, a);
  int row2 = gtk_tree::GetRowNumForIter(model, b);
  PasswordsPageGtk* page = reinterpret_cast<PasswordsPageGtk*>(window);
  return page->password_list_[row1].username_value.compare(
         page->password_list_[row2].username_value);
}

void PasswordsPageGtk::PasswordListPopulater::populate() {
  DCHECK(!pending_login_query_);
  PasswordStore* store = page_->GetPasswordStore();
  pending_login_query_ = store->GetAutofillableLogins(this);
}

void PasswordsPageGtk::PasswordListPopulater::OnPasswordStoreRequestDone(
    int handle, const std::vector<webkit_glue::PasswordForm*>& result) {
  DCHECK_EQ(pending_login_query_, handle);
  pending_login_query_ = 0;
  page_->SetPasswordList(result);
}
