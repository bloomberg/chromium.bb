// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/exceptions_page_gtk.h"

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

// Column ids for |exception_list_store_|.
enum {
  COL_SITE,
  COL_COUNT,
};

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////
// ExceptionsPageGtk, public:

ExceptionsPageGtk::ExceptionsPageGtk(Profile* profile)
    : populater(this), profile_(profile) {

  remove_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_EXCEPTIONS_PAGE_VIEW_REMOVE_BUTTON).c_str());
  gtk_widget_set_sensitive(remove_button_, FALSE);
  g_signal_connect(G_OBJECT(remove_button_), "clicked",
                   G_CALLBACK(OnRemoveButtonClicked), this);
  remove_all_button_ = gtk_button_new_with_label(l10n_util::GetStringUTF8(
          IDS_EXCEPTIONS_PAGE_VIEW_REMOVE_ALL_BUTTON).c_str());
  gtk_widget_set_sensitive(remove_all_button_, FALSE);
  g_signal_connect(G_OBJECT(remove_all_button_), "clicked",
                   G_CALLBACK(OnRemoveAllButtonClicked), this);

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

ExceptionsPageGtk::~ExceptionsPageGtk() {
}

///////////////////////////////////////////////////////////////////////////////
// ExceptionsPageGtk, private:

void ExceptionsPageGtk::InitExceptionTree() {
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
  g_signal_connect(G_OBJECT(exception_selection_), "changed",
                   G_CALLBACK(OnExceptionSelectionChanged), this);

  GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_PASSWORDS_PAGE_VIEW_SITE_COLUMN).c_str(),
      gtk_cell_renderer_text_new(),
      "text", COL_SITE,
      NULL);
  gtk_tree_view_column_set_sort_column_id(column, COL_SITE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(exception_tree_), column);

  populater.populate();
}

PasswordStore* ExceptionsPageGtk::GetPasswordStore() {
    return profile_->GetPasswordStore(Profile::EXPLICIT_ACCESS);
}

void ExceptionsPageGtk::SetExceptionList(
    const std::vector<webkit_glue::PasswordForm*>& result) {
  std::wstring languages =
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
  gtk_list_store_clear(exception_list_store_);
  exception_list_.resize(result.size());
  for (size_t i = 0; i < result.size(); ++i) {
    exception_list_[i] = *result[i];
    std::wstring formatted = net::FormatUrl(result[i]->origin, languages,
        false, UnescapeRule::NONE, NULL, NULL, NULL);
    std::string site = WideToUTF8(formatted);
    GtkTreeIter iter;
    gtk_list_store_insert_with_values(exception_list_store_, &iter, (gint) i,
                                      COL_SITE, site.c_str(), -1);
  }
  gtk_widget_set_sensitive(remove_all_button_, result.size() > 0);
}

// static
void ExceptionsPageGtk::OnRemoveButtonClicked(GtkButton* widget,
                                             ExceptionsPageGtk* page) {
  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(page->exception_selection_,
                                       NULL, &iter)) {
    NOTREACHED();
    return;
  }

  GtkTreePath* path = gtk_tree_model_get_path(
      GTK_TREE_MODEL(page->exception_list_sort_), &iter);
  gint index = gtk_tree::GetTreeSortChildRowNumForPath(
      page->exception_list_sort_, path);
  gtk_tree_path_free(path);

  GtkTreeIter child_iter;
  gtk_tree_model_sort_convert_iter_to_child_iter(
      GTK_TREE_MODEL_SORT(page->exception_list_sort_), &child_iter, &iter);

  // Remove from GTK list, DB, and vector.
  gtk_list_store_remove(page->exception_list_store_, &child_iter);
  page->GetPasswordStore()->RemoveLogin(page->exception_list_[index]);
  page->exception_list_.erase(page->exception_list_.begin() + index);

  gtk_widget_set_sensitive(page->remove_all_button_,
                           page->exception_list_.size() > 0);
}

// static
void ExceptionsPageGtk::OnRemoveAllButtonClicked(GtkButton* widget,
                                                ExceptionsPageGtk* page) {
  // Remove from GTK list, DB, and vector.
  PasswordStore* store = page->GetPasswordStore();
  gtk_list_store_clear(page->exception_list_store_);
  for (size_t i = 0; i < page->exception_list_.size(); ++i) {
    store->RemoveLogin(page->exception_list_[i]);
  }
  page->exception_list_.clear();
  gtk_widget_set_sensitive(page->remove_all_button_, FALSE);
}

// static
void ExceptionsPageGtk::OnExceptionSelectionChanged(GtkTreeSelection* selection,
                                                  ExceptionsPageGtk* page) {
  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
    gtk_widget_set_sensitive(page->remove_button_, FALSE);
    return;
  }
  gtk_widget_set_sensitive(page->remove_button_, TRUE);
}

// static
gint ExceptionsPageGtk::CompareSite(GtkTreeModel* model,
                                   GtkTreeIter* a, GtkTreeIter* b,
                                   gpointer window) {
  int row1 = gtk_tree::GetRowNumForIter(model, a);
  int row2 = gtk_tree::GetRowNumForIter(model, b);
  ExceptionsPageGtk* page = reinterpret_cast<ExceptionsPageGtk*>(window);
  return page->exception_list_[row1].origin.spec().compare(
         page->exception_list_[row2].origin.spec());
}

void ExceptionsPageGtk::ExceptionListPopulater::populate() {
  DCHECK(!pending_login_query_);
  PasswordStore* store = page_->GetPasswordStore();
  pending_login_query_ = store->GetBlacklistLogins(this);
}

void ExceptionsPageGtk::ExceptionListPopulater::OnPasswordStoreRequestDone(
    int handle, const std::vector<webkit_glue::PasswordForm*>& result) {
  DCHECK_EQ(pending_login_query_, handle);
  pending_login_query_ = 0;
  page_->SetExceptionList(result);
}
