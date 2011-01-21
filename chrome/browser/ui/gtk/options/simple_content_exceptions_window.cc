// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/options/simple_content_exceptions_window.h"

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "gfx/gtk_util.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Singleton for exception window.
SimpleContentExceptionsWindow* instance = NULL;

enum {
  COL_ACTION = gtk_tree::TableAdapter::COL_LAST_ID,
  COL_COUNT
};

}  // namespace

// static
void SimpleContentExceptionsWindow::ShowExceptionsWindow(
    GtkWindow* parent, RemoveRowsTableModel* model, int title_message_id) {
  DCHECK(model);
  scoped_ptr<RemoveRowsTableModel> owned_model(model);

  if (!instance) {
    instance = new SimpleContentExceptionsWindow(
        parent, owned_model.release(), title_message_id);
  } else {
    gtk_util::PresentWindow(instance->dialog_, 0);
  }
}

SimpleContentExceptionsWindow::SimpleContentExceptionsWindow(
    GtkWindow* parent,
    RemoveRowsTableModel* model,
    int title_message_id)
      : ignore_selection_changes_(false) {
  // Build the model adapters that translate views and TableModels into
  // something GTK can use.
  list_store_ = gtk_list_store_new(COL_COUNT,
                                   G_TYPE_STRING,
                                   G_TYPE_BOOLEAN,
                                   G_TYPE_BOOLEAN,
                                   G_TYPE_INT,
                                   G_TYPE_INT,
                                   G_TYPE_BOOLEAN,
                                   G_TYPE_STRING);
  treeview_ = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store_));
  g_object_unref(list_store_);

  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_), TRUE);
  gtk_tree_view_set_row_separator_func(
      GTK_TREE_VIEW(treeview_),
      gtk_tree::TableAdapter::OnCheckRowIsSeparator,
      NULL,
      NULL);

  // Set up the properties of the treeview
  GtkTreeViewColumn* hostname_column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_EXCEPTIONS_HOSTNAME_HEADER).c_str(),
      gtk_cell_renderer_text_new(),
      "text", gtk_tree::TableAdapter::COL_TITLE,
      "weight", gtk_tree::TableAdapter::COL_WEIGHT,
      "weight-set", gtk_tree::TableAdapter::COL_WEIGHT_SET,
      NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_), hostname_column);

  GtkTreeViewColumn* action_column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_EXCEPTIONS_ACTION_HEADER).c_str(),
      gtk_cell_renderer_text_new(),
      "text", COL_ACTION,
      NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_), action_column);

  treeview_selection_ = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(treeview_));
  gtk_tree_selection_set_mode(treeview_selection_, GTK_SELECTION_MULTIPLE);
  gtk_tree_selection_set_select_function(
      treeview_selection_,
      gtk_tree::TableAdapter::OnSelectionFilter,
      NULL,
      NULL);
  g_signal_connect(treeview_selection_, "changed",
                   G_CALLBACK(OnTreeSelectionChangedThunk), this);

  // Bind |list_store_| to our C++ model.
  model_.reset(model);
  model_adapter_.reset(new gtk_tree::TableAdapter(this, list_store_,
                                                  model_.get()));
  // Force a reload of everything to copy data into |list_store_|.
  model_adapter_->OnModelChanged();

  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(title_message_id).c_str(),
      parent,
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CLOSE,
      GTK_RESPONSE_CLOSE,
      NULL);
  gtk_window_set_default_size(GTK_WINDOW(dialog_), 500, 400);
  // Allow browser windows to go in front of the options dialog in metacity.
  gtk_window_set_type_hint(GTK_WINDOW(dialog_), GDK_WINDOW_TYPE_HINT_NORMAL);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  GtkWidget* hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), hbox);

  // Create a scrolled window to wrap the treeview widget.
  GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(scrolled), treeview_);
  gtk_box_pack_start(GTK_BOX(hbox), scrolled, TRUE, TRUE, 0);

  GtkWidget* button_box = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  remove_button_ = gtk_util::BuildDialogButton(dialog_,
                                               IDS_EXCEPTIONS_REMOVE_BUTTON,
                                               GTK_STOCK_REMOVE);

  g_signal_connect(remove_button_, "clicked", G_CALLBACK(RemoveThunk), this);
  gtk_box_pack_start(GTK_BOX(button_box), remove_button_, FALSE, FALSE, 0);

  remove_all_button_ = gtk_util::BuildDialogButton(
      dialog_,
      IDS_EXCEPTIONS_REMOVEALL_BUTTON,
      GTK_STOCK_CLEAR);
  g_signal_connect(remove_all_button_, "clicked", G_CALLBACK(RemoveAllThunk),
                   this);
  gtk_box_pack_start(GTK_BOX(button_box), remove_all_button_, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), button_box, FALSE, FALSE, 0);

  UpdateButtonState();

  gtk_util::ShowDialogWithLocalizedSize(dialog_,
      IDS_SIMPLE_CONTENT_EXCEPTION_DIALOG_WIDTH_CHARS,
      IDS_SIMPLE_CONTENT_EXCEPTION_DIALOG_HEIGHT_LINES,
      true);

  g_signal_connect(dialog_, "response", G_CALLBACK(gtk_widget_destroy), NULL);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroyThunk), this);
}

SimpleContentExceptionsWindow::~SimpleContentExceptionsWindow() {}

void SimpleContentExceptionsWindow::SetColumnValues(int row,
                                                    GtkTreeIter* iter) {
  string16 hostname = model_->GetText(row, IDS_EXCEPTIONS_HOSTNAME_HEADER);
  gtk_list_store_set(list_store_, iter, gtk_tree::TableAdapter::COL_TITLE,
                     UTF16ToUTF8(hostname).c_str(), -1);

  string16 action = model_->GetText(row, IDS_EXCEPTIONS_ACTION_HEADER);
  gtk_list_store_set(list_store_, iter, COL_ACTION,
                     UTF16ToUTF8(action).c_str(), -1);
}

void SimpleContentExceptionsWindow::OnAnyModelUpdateStart() {
  ignore_selection_changes_ = true;
}

void SimpleContentExceptionsWindow::OnAnyModelUpdate() {
  ignore_selection_changes_ = false;
}

void SimpleContentExceptionsWindow::UpdateButtonState() {
  int row_count = gtk_tree_model_iter_n_children(
      GTK_TREE_MODEL(list_store_), NULL);

  RemoveRowsTableModel::Rows rows;
  std::set<int> indices;
  gtk_tree::GetSelectedIndices(treeview_selection_, &indices);
  model_adapter_->MapListStoreIndicesToModelRows(indices, &rows);
  gtk_widget_set_sensitive(remove_button_, model_->CanRemoveRows(rows));
  gtk_widget_set_sensitive(remove_all_button_, row_count > 0);
}

void SimpleContentExceptionsWindow::Remove(GtkWidget* widget) {
  RemoveRowsTableModel::Rows rows;
  std::set<int> indices;
  gtk_tree::GetSelectedIndices(treeview_selection_, &indices);
  model_adapter_->MapListStoreIndicesToModelRows(indices, &rows);
  model_->RemoveRows(rows);
  UpdateButtonState();
}

void SimpleContentExceptionsWindow::RemoveAll(GtkWidget* widget) {
  model_->RemoveAll();
  UpdateButtonState();
}

void SimpleContentExceptionsWindow::OnWindowDestroy(GtkWidget* widget) {
  instance = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void SimpleContentExceptionsWindow::OnTreeSelectionChanged(
    GtkWidget* selection) {
  if (!ignore_selection_changes_)
    UpdateButtonState();
}
