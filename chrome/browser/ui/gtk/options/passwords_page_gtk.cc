// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/options/passwords_page_gtk.h"

#include <string>

#include "base/stl_util-inl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/gtk_tree.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/app_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/gtk_util.h"

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
  allow_show_passwords_.Init(prefs::kPasswordManagerAllowShowPasswords,
                             profile->GetPrefs(),
                             this);

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

  // We start with the "hide password" text but change it in the realize event.
  show_password_button_ = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON).c_str());
  gtk_widget_set_no_show_all(show_password_button_, true);
  gtk_widget_set_sensitive(show_password_button_, FALSE);
  g_signal_connect(show_password_button_, "clicked",
                   G_CALLBACK(OnShowPasswordButtonClickedThunk), this);
  g_signal_connect(show_password_button_, "realize",
                   G_CALLBACK(OnShowPasswordButtonRealizedThunk), this);

  password_ = gtk_label_new("");
  gtk_label_set_selectable(GTK_LABEL(password_), TRUE);
  gtk_widget_set_no_show_all(password_, true);

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

  // Initialize UI state based on current preference values.
  OnPrefChanged(prefs::kPasswordManagerAllowShowPasswords);
}

PasswordsPageGtk::~PasswordsPageGtk() {
  STLDeleteElements(&password_list_);
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
  g_signal_connect(password_selection_, "changed",
                   G_CALLBACK(OnPasswordSelectionChangedThunk), this);

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
  std::string languages =
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
  gtk_list_store_clear(password_list_store_);
  STLDeleteElements(&password_list_);
  password_list_ = result;
  for (size_t i = 0; i < result.size(); ++i) {
    GtkTreeIter iter;
    gtk_list_store_insert_with_values(password_list_store_, &iter, (gint) i,
        COL_SITE,
        UTF16ToUTF8(net::FormatUrl(result[i]->origin, languages)).c_str(),
        COL_USERNAME, UTF16ToUTF8(result[i]->username_value).c_str(), -1);
  }
  gtk_widget_set_sensitive(remove_all_button_, result.size() > 0);
}

void PasswordsPageGtk::HidePassword() {
  password_showing_ = false;
  gtk_label_set_text(GTK_LABEL(password_), "");
  gtk_button_set_label(GTK_BUTTON(show_password_button_),
      l10n_util::GetStringUTF8(IDS_PASSWORDS_PAGE_VIEW_SHOW_BUTTON).c_str());
}

void PasswordsPageGtk::Observe(NotificationType type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  DCHECK_EQ(NotificationType::PREF_CHANGED, type.value);
  const std::string* pref_name = Details<std::string>(details).ptr();
  OnPrefChanged(*pref_name);
}

void PasswordsPageGtk::OnPrefChanged(const std::string& pref_name) {
  if (pref_name == prefs::kPasswordManagerAllowShowPasswords) {
    if (allow_show_passwords_.GetValue()) {
      gtk_widget_show(show_password_button_);
      gtk_widget_show(password_);
    } else {
      HidePassword();
      gtk_widget_hide(show_password_button_);
      gtk_widget_hide(password_);
    }
  } else {
    NOTREACHED();
  }
}

void PasswordsPageGtk::OnRemoveButtonClicked(GtkWidget* widget) {
  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(password_selection_,
                                       NULL, &iter)) {
    NOTREACHED();
    return;
  }

  GtkTreePath* path = gtk_tree_model_get_path(
      GTK_TREE_MODEL(password_list_sort_), &iter);
  gint index = gtk_tree::GetTreeSortChildRowNumForPath(
      password_list_sort_, path);
  gtk_tree_path_free(path);

  GtkTreeIter child_iter;
  gtk_tree_model_sort_convert_iter_to_child_iter(
      GTK_TREE_MODEL_SORT(password_list_sort_), &child_iter, &iter);

  // Remove from GTK list, DB, and vector.
  gtk_list_store_remove(password_list_store_, &child_iter);
  GetPasswordStore()->RemoveLogin(*password_list_[index]);
  delete password_list_[index];
  password_list_.erase(password_list_.begin() + index);

  gtk_widget_set_sensitive(remove_all_button_, password_list_.size() > 0);
}

void PasswordsPageGtk::OnRemoveAllButtonClicked(GtkWidget* widget) {
  GtkWindow* window = GTK_WINDOW(gtk_widget_get_toplevel(page_));
  GtkWidget* confirm = gtk_message_dialog_new(
      window,
      static_cast<GtkDialogFlags>(
          GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
      GTK_MESSAGE_QUESTION,
      GTK_BUTTONS_YES_NO,
      "%s",
      l10n_util::GetStringUTF8(
          IDS_PASSWORDS_PAGE_VIEW_TEXT_DELETE_ALL_PASSWORDS).c_str());
  gtk_util::ApplyMessageDialogQuirks(confirm);
  gtk_window_set_title(GTK_WINDOW(confirm), l10n_util::GetStringUTF8(
          IDS_PASSWORDS_PAGE_VIEW_CAPTION_DELETE_ALL_PASSWORDS).c_str());
  g_signal_connect(confirm, "response",
                   G_CALLBACK(OnRemoveAllConfirmResponseThunk), this);
  gtk_widget_show_all(confirm);
}

void PasswordsPageGtk::OnRemoveAllConfirmResponse(GtkWidget* confirm,
                                                  gint response) {
  bool confirmed = false;
  switch (response) {
    case GTK_RESPONSE_YES:
      confirmed = true;
      break;
    default:
      break;
  }
  gtk_widget_destroy(confirm);
  if (!confirmed)
    return;

  // Remove from GTK list, DB, and vector.
  PasswordStore* store = GetPasswordStore();
  gtk_list_store_clear(password_list_store_);
  for (size_t i = 0; i < password_list_.size(); ++i)
    store->RemoveLogin(*password_list_[i]);
  STLDeleteElements(&password_list_);
  gtk_widget_set_sensitive(remove_all_button_, FALSE);
}

void PasswordsPageGtk::OnShowPasswordButtonClicked(GtkWidget* widget) {
  if (password_showing_ || !allow_show_passwords_.GetValue()) {
    // Hide the password.
    HidePassword();
    return;
  }
  // Show the password.
  password_showing_ = true;
  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(password_selection_,
                                       NULL, &iter)) {
    NOTREACHED();
    return;
  }
  GtkTreePath* path = gtk_tree_model_get_path(
      GTK_TREE_MODEL(password_list_sort_), &iter);
  gint index = gtk_tree::GetTreeSortChildRowNumForPath(
      password_list_sort_, path);
  gtk_tree_path_free(path);
  std::string pass = UTF16ToUTF8(password_list_[index]->password_value);
  gtk_label_set_text(GTK_LABEL(password_), pass.c_str());
  gtk_button_set_label(GTK_BUTTON(show_password_button_),
      l10n_util::GetStringUTF8(IDS_PASSWORDS_PAGE_VIEW_HIDE_BUTTON).c_str());
}

void PasswordsPageGtk::OnShowPasswordButtonRealized(GtkWidget* widget) {
  // We have just realized the "show password" button, so we can now accurately
  // find out how big it needs to be in order to accomodate both the "show" and
  // "hide" labels. (This requires font information to work correctly.) The
  // button starts with the "hide" label so we only have to change it once.
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
}

void PasswordsPageGtk::OnPasswordSelectionChanged(GtkTreeSelection* selection) {
  // No matter how the selection changed, we want to hide the old password.
  HidePassword();

  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
    gtk_widget_set_sensitive(show_password_button_, FALSE);
    gtk_widget_set_sensitive(remove_button_, FALSE);
    return;
  }
  gtk_widget_set_sensitive(show_password_button_, TRUE);
  gtk_widget_set_sensitive(remove_button_, TRUE);
}

// static
gint PasswordsPageGtk::CompareSite(GtkTreeModel* model,
                                   GtkTreeIter* a, GtkTreeIter* b,
                                   gpointer window) {
  int row1 = gtk_tree::GetRowNumForIter(model, a);
  int row2 = gtk_tree::GetRowNumForIter(model, b);
  PasswordsPageGtk* page = reinterpret_cast<PasswordsPageGtk*>(window);
  return page->password_list_[row1]->origin.spec().compare(
         page->password_list_[row2]->origin.spec());
}

// static
gint PasswordsPageGtk::CompareUsername(GtkTreeModel* model,
                                       GtkTreeIter* a, GtkTreeIter* b,
                                       gpointer window) {
  int row1 = gtk_tree::GetRowNumForIter(model, a);
  int row2 = gtk_tree::GetRowNumForIter(model, b);
  PasswordsPageGtk* page = reinterpret_cast<PasswordsPageGtk*>(window);
  return page->password_list_[row1]->username_value.compare(
         page->password_list_[row2]->username_value);
}

void PasswordsPageGtk::PasswordListPopulater::populate() {
  DCHECK(!pending_login_query_);
  PasswordStore* store = page_->GetPasswordStore();
  if (store != NULL)
    pending_login_query_ = store->GetAutofillableLogins(this);
  else
    LOG(ERROR) << "No password store! Cannot display passwords.";
}

void PasswordsPageGtk::PasswordListPopulater::OnPasswordStoreRequestDone(
    int handle, const std::vector<webkit_glue::PasswordForm*>& result) {
  DCHECK_EQ(pending_login_query_, handle);
  pending_login_query_ = 0;
  page_->SetPasswordList(result);
}
