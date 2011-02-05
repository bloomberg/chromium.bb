// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/possible_url_model.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/accessible_widget_helper_gtk.h"
#include "chrome/browser/ui/gtk/gtk_tree.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/options/url_picker_dialog_gtk.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/base/net_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/gtk_util.h"

namespace {

// Column ids for |history_list_store_|.
enum {
  COL_FAVICON,
  COL_TITLE,
  COL_DISPLAY_URL,
  COL_COUNT,
};

}  // anonymous namespace

UrlPickerDialogGtk::UrlPickerDialogGtk(UrlPickerCallback* callback,
                                       Profile* profile,
                                       GtkWindow* parent)
    : profile_(profile),
      callback_(callback) {
  std::string dialog_name = l10n_util::GetStringUTF8(IDS_ASI_ADD_TITLE);
  dialog_ = gtk_dialog_new_with_buttons(
      dialog_name.c_str(),
      parent,
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL,
      NULL);
  accessible_widget_helper_.reset(new AccessibleWidgetHelper(
      dialog_, profile));
  accessible_widget_helper_->SendOpenWindowNotification(dialog_name);

  add_button_ = gtk_dialog_add_button(GTK_DIALOG(dialog_),
                                      GTK_STOCK_ADD, GTK_RESPONSE_OK);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_OK);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  // URL entry.
  GtkWidget* url_hbox = gtk_hbox_new(FALSE, gtk_util::kLabelSpacing);
  GtkWidget* url_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_ASI_URL).c_str());
  gtk_box_pack_start(GTK_BOX(url_hbox), url_label,
                     FALSE, FALSE, 0);
  url_entry_ = gtk_entry_new();
  accessible_widget_helper_->SetWidgetName(url_entry_, IDS_ASI_URL);
  gtk_entry_set_activates_default(GTK_ENTRY(url_entry_), TRUE);
  g_signal_connect(url_entry_, "changed",
                   G_CALLBACK(OnUrlEntryChangedThunk), this);
  gtk_box_pack_start(GTK_BOX(url_hbox), url_entry_,
                     TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox), url_hbox,
                     FALSE, FALSE, 0);

  // Recent history description label.
  GtkWidget* history_vbox = gtk_vbox_new(FALSE, gtk_util::kLabelSpacing);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), history_vbox);
  GtkWidget* history_label = gtk_util::CreateBoldLabel(
      l10n_util::GetStringUTF8(IDS_ASI_DESCRIPTION));
  gtk_box_pack_start(GTK_BOX(history_vbox), history_label, FALSE, FALSE, 0);

  // Recent history list.
  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(history_vbox), scroll_window);

  history_list_store_ = gtk_list_store_new(COL_COUNT,
                                           GDK_TYPE_PIXBUF,
                                           G_TYPE_STRING,
                                           G_TYPE_STRING);
  history_list_sort_ = gtk_tree_model_sort_new_with_model(
      GTK_TREE_MODEL(history_list_store_));
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(history_list_sort_),
                                  COL_TITLE, CompareTitle, this, NULL);
  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(history_list_sort_),
                                  COL_DISPLAY_URL, CompareURL, this, NULL);
  history_tree_ = gtk_tree_view_new_with_model(history_list_sort_);
  accessible_widget_helper_->SetWidgetName(
      history_tree_, IDS_ASI_DESCRIPTION);
  g_object_unref(history_list_store_);
  g_object_unref(history_list_sort_);
  gtk_container_add(GTK_CONTAINER(scroll_window), history_tree_);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(history_tree_),
                                    TRUE);
  g_signal_connect(history_tree_, "row-activated",
                   G_CALLBACK(OnHistoryRowActivatedThunk), this);

  history_selection_ = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(history_tree_));
  gtk_tree_selection_set_mode(history_selection_,
                              GTK_SELECTION_SINGLE);
  g_signal_connect(history_selection_, "changed",
                   G_CALLBACK(OnHistorySelectionChangedThunk), this);

  // History list columns.
  GtkTreeViewColumn* column = gtk_tree_view_column_new();
  GtkCellRenderer* renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(column, renderer, FALSE);
  gtk_tree_view_column_add_attribute(column, renderer, "pixbuf", COL_FAVICON);
  renderer = gtk_cell_renderer_text_new();
  gtk_tree_view_column_pack_start(column, renderer, TRUE);
  gtk_tree_view_column_add_attribute(column, renderer, "text", COL_TITLE);
  gtk_tree_view_append_column(GTK_TREE_VIEW(history_tree_),
                              column);
  gtk_tree_view_column_set_title(
      column, l10n_util::GetStringUTF8(IDS_ASI_PAGE_COLUMN).c_str());
  gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_resizable(column, TRUE);
  gtk_tree_view_column_set_sort_column_id(column, COL_TITLE);

  GtkTreeViewColumn* url_column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_ASI_URL_COLUMN).c_str(),
      gtk_cell_renderer_text_new(),
      "text", COL_DISPLAY_URL,
      NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(history_tree_), url_column);
  gtk_tree_view_column_set_sort_column_id(url_column, COL_DISPLAY_URL);

  // Loading data, showing dialog.
  url_table_model_.reset(new PossibleURLModel());
  url_table_adapter_.reset(new gtk_tree::TableAdapter(this, history_list_store_,
                                                      url_table_model_.get()));
  url_table_model_->Reload(profile_);

  EnableControls();

  // Set the size of the dialog.
  gtk_widget_realize(dialog_);
  gtk_util::SetWindowSizeFromResources(GTK_WINDOW(dialog_),
                                       IDS_URLPICKER_DIALOG_WIDTH_CHARS,
                                       IDS_URLPICKER_DIALOG_HEIGHT_LINES,
                                       true);

  // Set the width of the first column as well.
  int width;
  gtk_util::GetWidgetSizeFromResources(
      dialog_,
      IDS_URLPICKER_DIALOG_LEFT_COLUMN_WIDTH_CHARS, 0,
      &width, NULL);
  gtk_tree_view_column_set_fixed_width(column, width);

  gtk_util::ShowDialogWithLocalizedSize(dialog_,
      IDS_URLPICKER_DIALOG_WIDTH_CHARS,
      IDS_URLPICKER_DIALOG_HEIGHT_LINES,
      false);

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroyThunk), this);
}

UrlPickerDialogGtk::~UrlPickerDialogGtk() {
  delete callback_;
}

void UrlPickerDialogGtk::AddURL() {
  callback_->Run(URLFixerUpper::FixupURL(
      gtk_entry_get_text(GTK_ENTRY(url_entry_)), std::string()));
}

void UrlPickerDialogGtk::EnableControls() {
  const gchar* text = gtk_entry_get_text(GTK_ENTRY(url_entry_));
  gtk_widget_set_sensitive(add_button_, text && *text);
}

std::string UrlPickerDialogGtk::GetURLForPath(GtkTreePath* path) const {
  gint row = gtk_tree::GetTreeSortChildRowNumForPath(history_list_sort_, path);
  if (row < 0) {
    NOTREACHED();
    return std::string();
  }
  std::string languages =
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages);
  // Because this gets parsed by FixupURL(), it's safe to omit the scheme or
  // trailing slash, and unescape most characters, but we need to not drop any
  // username/password, or unescape anything that changes the meaning.
  return UTF16ToUTF8(net::FormatUrl(url_table_model_->GetURL(row),
      languages, net::kFormatUrlOmitAll & ~net::kFormatUrlOmitUsernamePassword,
      UnescapeRule::SPACES, NULL, NULL, NULL));
}

void UrlPickerDialogGtk::SetColumnValues(int row, GtkTreeIter* iter) {
  SkBitmap bitmap = url_table_model_->GetIcon(row);
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&bitmap);
  string16 title = url_table_model_->GetText(row, IDS_ASI_PAGE_COLUMN);
  string16 url = url_table_model_->GetText(row, IDS_ASI_URL_COLUMN);
  gtk_list_store_set(history_list_store_, iter,
                     COL_FAVICON, pixbuf,
                     COL_TITLE, UTF16ToUTF8(title).c_str(),
                     COL_DISPLAY_URL, UTF16ToUTF8(url).c_str(),
                     -1);
  g_object_unref(pixbuf);
}

// static
gint UrlPickerDialogGtk::CompareTitle(GtkTreeModel* model,
                                      GtkTreeIter* a,
                                      GtkTreeIter* b,
                                      gpointer window) {
  int row1 = gtk_tree::GetRowNumForIter(model, a);
  int row2 = gtk_tree::GetRowNumForIter(model, b);
  return reinterpret_cast<UrlPickerDialogGtk*>(window)->url_table_model_->
      CompareValues(row1, row2, IDS_ASI_PAGE_COLUMN);
}

// static
gint UrlPickerDialogGtk::CompareURL(GtkTreeModel* model,
                                    GtkTreeIter* a,
                                    GtkTreeIter* b,
                                    gpointer window) {
  int row1 = gtk_tree::GetRowNumForIter(model, a);
  int row2 = gtk_tree::GetRowNumForIter(model, b);
  return reinterpret_cast<UrlPickerDialogGtk*>(window)->url_table_model_->
      CompareValues(row1, row2, IDS_ASI_URL_COLUMN);
}

void UrlPickerDialogGtk::OnUrlEntryChanged(GtkWidget* editable) {
  EnableControls();
}

void UrlPickerDialogGtk::OnHistorySelectionChanged(
    GtkTreeSelection* selection) {
  GtkTreeIter iter;
  if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
    // The user has unselected the history element, nothing to do.
    return;
  }
  GtkTreePath* path = gtk_tree_model_get_path(
      GTK_TREE_MODEL(history_list_sort_), &iter);
  gtk_entry_set_text(GTK_ENTRY(url_entry_),
                     GetURLForPath(path).c_str());
  gtk_tree_path_free(path);
}

void UrlPickerDialogGtk::OnHistoryRowActivated(GtkWidget* tree_view,
                                               GtkTreePath* path,
                                               GtkTreeViewColumn* column) {
  callback_->Run(URLFixerUpper::FixupURL(GetURLForPath(path), std::string()));
  gtk_widget_destroy(dialog_);
}

void UrlPickerDialogGtk::OnResponse(GtkWidget* dialog, int response_id) {
  if (response_id == GTK_RESPONSE_OK)
    AddURL();
  gtk_widget_destroy(dialog_);
}

void UrlPickerDialogGtk::OnWindowDestroy(GtkWidget* widget) {
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}
