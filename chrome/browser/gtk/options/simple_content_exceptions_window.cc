// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/options/simple_content_exceptions_window.h"

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "gfx/gtk_util.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

namespace {

// Singleton for exception window.
SimpleContentExceptionsWindow* instance = NULL;

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
    int title_message_id) {
  // Build the model adapters that translate views and TableModels into
  // something GTK can use.
  list_store_ = gtk_list_store_new(COL_COUNT, G_TYPE_STRING, G_TYPE_STRING);
  treeview_ = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store_));
  g_object_unref(list_store_);

  // Set up the properties of the treeview
  GtkTreeViewColumn* hostname_column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_EXCEPTIONS_HOSTNAME_HEADER).c_str(),
      gtk_cell_renderer_text_new(),
      "text", COL_HOSTNAME,
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

void SimpleContentExceptionsWindow::SetColumnValues(int row,
                                                         GtkTreeIter* iter) {
  std::wstring hostname = model_->GetText(row, IDS_EXCEPTIONS_HOSTNAME_HEADER);
  gtk_list_store_set(list_store_, iter, COL_HOSTNAME,
                     WideToUTF8(hostname).c_str(), -1);

  std::wstring action = model_->GetText(row, IDS_EXCEPTIONS_ACTION_HEADER);
  gtk_list_store_set(list_store_, iter, COL_ACTION,
                     WideToUTF8(action).c_str(), -1);
}

void SimpleContentExceptionsWindow::UpdateButtonState() {
  int row_count = gtk_tree_model_iter_n_children(
      GTK_TREE_MODEL(list_store_), NULL);

  RemoveRowsTableModel::Rows rows;
  GetSelectedRows(&rows);
  gtk_widget_set_sensitive(remove_button_, model_->CanRemoveRows(rows));
  gtk_widget_set_sensitive(remove_all_button_, row_count > 0);
}

void SimpleContentExceptionsWindow::GetSelectedRows(
    RemoveRowsTableModel::Rows* rows) {
  std::set<int> indices;
  gtk_tree::GetSelectedIndices(treeview_selection_, &indices);
  for (std::set<int>::iterator i = indices.begin(); i != indices.end(); ++i)
    rows->insert(*i);
}

void SimpleContentExceptionsWindow::Remove(GtkWidget* widget) {
  RemoveRowsTableModel::Rows rows;
  GetSelectedRows(&rows);
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
  UpdateButtonState();
}
