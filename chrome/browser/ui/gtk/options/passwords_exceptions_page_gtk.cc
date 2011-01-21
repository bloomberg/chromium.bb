// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/options/passwords_exceptions_page_gtk.h"

#include <string>

#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/gtk_tree.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "gfx/gtk_util.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Column ids for |exception_list_store_|.
enum {
  COL_SITE,
  COL_COUNT,
};

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// PasswordsExceptionsPageGtk, public:

PasswordsExceptionsPageGtk::PasswordsExceptionsPageGtk(Profile* profile)
    : populater(this), profile_(profile) {

  remove_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_PASSWORDS_PAGE_VIEW_REMOVE_BUTTON).c_str());
  gtk_widget_set_sensitive(remove_button_, FALSE);
  g_signal_connect(remove_button_, "clicked",
                   G_CALLBACK(OnRemoveButtonClickedThunk), this);
  remove_all_button_ = gtk_button_new_with_label(l10n_util::GetStringUTF8(
          IDS_PASSWORDS_PAGE_VIEW_REMOVE_ALL_BUTTON).c_str());
  gtk_widget_set_sensitive(remove_all_button_, FALSE);
  g_signal_connect(remove_all_button_, "clicked",
                   G_CALLBACK(OnRemoveAllButtonClickedThunk), this);

  GtkWidget* buttons = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(buttons), remove_button_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(buttons), remove_all_button_, FALSE, FALSE, 0);

  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);

  // Sets exception_tree_ among other things.
  InitExceptionTree();
  gtk_container_add(GTK_CONTAINER(scroll_window), exception_tree_);

  page_ = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(page_),
                                 gtk_util::kContentAreaBorder);
  gtk_box_pack_end(GTK_BOX(page_), buttons, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(page_), scroll_window, TRUE, TRUE, 0);
}

PasswordsExceptionsPageGtk::~PasswordsExceptionsPageGtk() {
  STLDeleteElements(&exception_list_);
}

///////////////////////////////////////////////////////////////////////////////
// PasswordsExceptionsPageGtk, private:

void PasswordsExceptionsPageGtk::InitExceptionTree() {
  exception_list_store_ = gtk_list_store_new(COL_COUNT, G_TYPE_STRING);
  exception_list_sort_ = gtk_tree_model_sort_new_with_model(
      GTK_TREE_MODEL(exception_list_store_));
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(exception_list_sort_),
                                  COL_SITE, CompareSite, this, NULL);
  exception_tree_ = gtk_tree_view_new_with_model(exception_list_sort_);
  g_object_unref(exception_list_store_);
  g_object_unref(exception_list_sort_);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(exception_tree_), TRUE);

  exception_selection_ = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(exception_tree_));
  gtk_tree_selection_set_mode(exception_selection_,
                              GTK_SELECTION_SINGLE);
  g_signal_connect(exception_selection_, "changed",
                   G_CALLBACK(OnExceptionSelectionChangedThunk), this);

  GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN).c_str(),
      gtk_cell_renderer_text_new(),
      "text", COL_SITE,
      NULL);
  gtk_tree_view_column_set_sort_column_id(column, COL_SITE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(exception_tree_), column);

  populater.populate();
}

PasswordStore* PasswordsExceptionsPageGtk::GetPasswordStore() {
    return profile_->GetPasswordStore(Profile::EXPLICIT_ACCESS);
}

void PasswordsExceptionsPageGtk::SetExceptionList(
    const std::vector<webkit_glue::PasswordForm*>& result) {
  std::string languages =
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
  gtk_list_store_clear(exception_list_store_);
  STLDeleteElements(&exception_list_);
  exception_list_ = result;
  for (size_t i = 0; i < result.size(); ++i) {
    GtkTreeIter iter;
    gtk_list_store_insert_with_values(exception_list_store_, &iter, (gint) i,
        COL_SITE,
        UTF16ToUTF8(net::FormatUrl(result[i]->origin, languages)).c_str(), -1);
  }
  gtk_widget_set_sensitive(remove_all_button_, result.size() > 0);
}

void PasswordsExceptionsPageGtk::OnRemoveButtonClicked(GtkWidget* widget) {
  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(exception_selection_,
                                       NULL, &iter)) {
    NOTREACHED();
    return;
  }

  GtkTreePath* path = gtk_tree_model_get_path(
      GTK_TREE_MODEL(exception_list_sort_), &iter);
  gint index = gtk_tree::GetTreeSortChildRowNumForPath(
      exception_list_sort_, path);
  gtk_tree_path_free(path);

  GtkTreeIter child_iter;
  gtk_tree_model_sort_convert_iter_to_child_iter(
      GTK_TREE_MODEL_SORT(exception_list_sort_), &child_iter, &iter);

  // Remove from GTK list, DB, and vector.
  gtk_list_store_remove(exception_list_store_, &child_iter);
  GetPasswordStore()->RemoveLogin(*exception_list_[index]);
  delete exception_list_[index];
  exception_list_.erase(exception_list_.begin() + index);

  gtk_widget_set_sensitive(remove_all_button_, exception_list_.size() > 0);
}

void PasswordsExceptionsPageGtk::OnRemoveAllButtonClicked(GtkWidget* widget) {
  // Remove from GTK list, DB, and vector.
  PasswordStore* store = GetPasswordStore();
  gtk_list_store_clear(exception_list_store_);
  for (size_t i = 0; i < exception_list_.size(); ++i)
    store->RemoveLogin(*exception_list_[i]);
  STLDeleteElements(&exception_list_);
  gtk_widget_set_sensitive(remove_all_button_, FALSE);
}

void PasswordsExceptionsPageGtk::OnExceptionSelectionChanged(
    GtkTreeSelection* selection) {
  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
    gtk_widget_set_sensitive(remove_button_, FALSE);
    return;
  }
  gtk_widget_set_sensitive(remove_button_, TRUE);
}

// static
gint PasswordsExceptionsPageGtk::CompareSite(GtkTreeModel* model,
                                   GtkTreeIter* a, GtkTreeIter* b,
                                   gpointer window) {
  int row1 = gtk_tree::GetRowNumForIter(model, a);
  int row2 = gtk_tree::GetRowNumForIter(model, b);
  PasswordsExceptionsPageGtk* page =
      reinterpret_cast<PasswordsExceptionsPageGtk*>(window);
  return page->exception_list_[row1]->origin.spec().compare(
         page->exception_list_[row2]->origin.spec());
}

void PasswordsExceptionsPageGtk::ExceptionListPopulater::populate() {
  DCHECK(!pending_login_query_);
  PasswordStore* store = page_->GetPasswordStore();
  if (store != NULL)
    pending_login_query_ = store->GetBlacklistLogins(this);
  else
    LOG(ERROR) << "No password store! Cannot display exceptions.";
}

void
PasswordsExceptionsPageGtk::ExceptionListPopulater::OnPasswordStoreRequestDone(
    int handle, const std::vector<webkit_glue::PasswordForm*>& result) {
  DCHECK_EQ(pending_login_query_, handle);
  pending_login_query_ = 0;
  page_->SetExceptionList(result);
}
