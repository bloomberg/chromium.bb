// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/options/content_exceptions_window_gtk.h"

#include <set>

#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/options/content_exception_editor.h"
#include "gfx/gtk_util.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Singletons for each possible exception window.
ContentExceptionsWindowGtk* instances[CONTENT_SETTINGS_NUM_TYPES] = { NULL };

}  // namespace

// static
void ContentExceptionsWindowGtk::ShowExceptionsWindow(
    GtkWindow* parent,
    HostContentSettingsMap* map,
    HostContentSettingsMap* off_the_record_map,
    ContentSettingsType type) {
  DCHECK(map);
  DCHECK(type < CONTENT_SETTINGS_NUM_TYPES);
  // Geolocation exceptions are handled by SimpleContentExceptionsWindow.
  DCHECK_NE(type, CONTENT_SETTINGS_TYPE_GEOLOCATION);
  // Notification exceptions are handled by SimpleContentExceptionsWindow.
  DCHECK_NE(type, CONTENT_SETTINGS_TYPE_NOTIFICATIONS);

  if (!instances[type]) {
    // Create the options window.
    instances[type] =
        new ContentExceptionsWindowGtk(parent, map, off_the_record_map, type);
  } else {
    gtk_util::PresentWindow(instances[type]->dialog_, 0);
  }
}

ContentExceptionsWindowGtk::~ContentExceptionsWindowGtk() {
}

ContentExceptionsWindowGtk::ContentExceptionsWindowGtk(
    GtkWindow* parent,
    HostContentSettingsMap* map,
    HostContentSettingsMap* off_the_record_map,
    ContentSettingsType type)
    : allow_off_the_record_(off_the_record_map) {
  // Build the list backing that GTK uses, along with an adapter which will
  // sort stuff without changing the underlying backing store.
  list_store_ = gtk_list_store_new(COL_COUNT, G_TYPE_STRING, G_TYPE_STRING,
                                   PANGO_TYPE_STYLE);
  sort_list_store_ = gtk_tree_model_sort_new_with_model(
      GTK_TREE_MODEL(list_store_));
  treeview_ = gtk_tree_view_new_with_model(GTK_TREE_MODEL(sort_list_store_));
  g_object_unref(list_store_);
  g_object_unref(sort_list_store_);

  // Set up the properties of the treeview
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview_), TRUE);
  g_signal_connect(treeview_, "row-activated",
                   G_CALLBACK(OnTreeViewRowActivateThunk), this);

  GtkTreeViewColumn* pattern_column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_EXCEPTIONS_PATTERN_HEADER).c_str(),
      gtk_cell_renderer_text_new(),
      "text", COL_PATTERN,
      "style", COL_OTR,
      NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_), pattern_column);
  gtk_tree_view_column_set_sort_column_id(pattern_column, COL_PATTERN);

  GtkTreeViewColumn* action_column = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_EXCEPTIONS_ACTION_HEADER).c_str(),
      gtk_cell_renderer_text_new(),
      "text", COL_ACTION,
      "style", COL_OTR,
      NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_), action_column);
  gtk_tree_view_column_set_sort_column_id(action_column, COL_ACTION);

  treeview_selection_ = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(treeview_));
  gtk_tree_selection_set_mode(treeview_selection_, GTK_SELECTION_MULTIPLE);
  g_signal_connect(treeview_selection_, "changed",
                   G_CALLBACK(OnTreeSelectionChangedThunk), this);

  // Bind |list_store_| to our C++ model.
  model_.reset(new ContentExceptionsTableModel(map, off_the_record_map, type));
  model_adapter_.reset(new gtk_tree::TableAdapter(this, list_store_,
                                                  model_.get()));
  // Force a reload of everything to copy data into |list_store_|.
  model_adapter_->OnModelChanged();

  dialog_ = gtk_dialog_new_with_buttons(
      GetWindowTitle().c_str(),
      parent,
      static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_NO_SEPARATOR),
      GTK_STOCK_CLOSE,
      GTK_RESPONSE_CLOSE,
      NULL);
  gtk_window_set_default_size(GTK_WINDOW(dialog_), 500, -1);
  // Allow browser windows to go in front of the options dialog in metacity.
  gtk_window_set_type_hint(GTK_WINDOW(dialog_), GDK_WINDOW_TYPE_HINT_NORMAL);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);

  GtkWidget* hbox = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), hbox);

  GtkWidget* treeview_box = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  // Create a scrolled window to wrap the treeview widget.
  GtkWidget* scrolled = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(scrolled), treeview_);
  gtk_box_pack_start(GTK_BOX(treeview_box), scrolled, TRUE, TRUE, 0);

  // If we also have an OTR profile, inform the user that OTR exceptions are
  // displayed in italics.
  if (allow_off_the_record_) {
    GtkWidget* incognito_label = gtk_label_new(
        l10n_util::GetStringUTF8(IDS_EXCEPTIONS_OTR_IN_ITALICS).c_str());
    PangoAttrList* attributes = pango_attr_list_new();
    pango_attr_list_insert(attributes,
                           pango_attr_style_new(PANGO_STYLE_ITALIC));
    gtk_label_set_attributes(GTK_LABEL(incognito_label), attributes);
    pango_attr_list_unref(attributes);
    gtk_misc_set_alignment(GTK_MISC(incognito_label), 0, 0);
    gtk_box_pack_start(GTK_BOX(treeview_box), incognito_label, FALSE, FALSE, 0);
  }

  gtk_box_pack_start(GTK_BOX(hbox), treeview_box, TRUE, TRUE, 0);

  GtkWidget* button_box = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  GtkWidget* add_button = gtk_util::BuildDialogButton(dialog_,
                                                      IDS_EXCEPTIONS_ADD_BUTTON,
                                                      GTK_STOCK_ADD);
  g_signal_connect(add_button, "clicked", G_CALLBACK(AddThunk), this);
  gtk_box_pack_start(GTK_BOX(button_box), add_button, FALSE, FALSE, 0);

  edit_button_ = gtk_util::BuildDialogButton(dialog_,
                                             IDS_EXCEPTIONS_EDIT_BUTTON,
                                             GTK_STOCK_EDIT);
  g_signal_connect(edit_button_, "clicked", G_CALLBACK(EditThunk), this);
  gtk_box_pack_start(GTK_BOX(button_box), edit_button_, FALSE, FALSE, 0);

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
      IDS_CONTENT_EXCEPTION_DIALOG_WIDTH_CHARS,
      -1,
      true);

  g_signal_connect(dialog_, "response", G_CALLBACK(gtk_widget_destroy), NULL);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnWindowDestroyThunk), this);
}

void ContentExceptionsWindowGtk::SetColumnValues(int row, GtkTreeIter* iter) {
  string16 pattern = model_->GetText(row, IDS_EXCEPTIONS_PATTERN_HEADER);
  gtk_list_store_set(list_store_, iter, COL_PATTERN,
                     UTF16ToUTF8(pattern).c_str(), -1);

  string16 action = model_->GetText(row, IDS_EXCEPTIONS_ACTION_HEADER);
  gtk_list_store_set(list_store_, iter, COL_ACTION,
                     UTF16ToUTF8(action).c_str(), -1);

  bool is_off_the_record = model_->entry_is_off_the_record(row);
  PangoStyle style =
      is_off_the_record ? PANGO_STYLE_ITALIC : PANGO_STYLE_NORMAL;
  gtk_list_store_set(list_store_, iter, COL_OTR, style, -1);
}

void ContentExceptionsWindowGtk::AcceptExceptionEdit(
    const ContentSettingsPattern& pattern,
    ContentSetting setting,
    bool is_off_the_record,
    int index,
    bool is_new) {
  DCHECK(!is_off_the_record || allow_off_the_record_);

  if (!is_new)
    model_->RemoveException(index);

  model_->AddException(pattern, setting, is_off_the_record);

  int new_index = model_->IndexOfExceptionByPattern(pattern, is_off_the_record);
  DCHECK_NE(-1, new_index);

  gtk_tree::SelectAndFocusRowNum(new_index, GTK_TREE_VIEW(treeview_));

  UpdateButtonState();
}

void ContentExceptionsWindowGtk::UpdateButtonState() {
  int num_selected = gtk_tree_selection_count_selected_rows(
      treeview_selection_);
  int row_count = gtk_tree_model_iter_n_children(
      GTK_TREE_MODEL(sort_list_store_), NULL);

  // TODO(erg): http://crbug.com/34177 , support editing of more than one entry
  // at a time.
  gtk_widget_set_sensitive(edit_button_, num_selected == 1);
  gtk_widget_set_sensitive(remove_button_, num_selected >= 1);
  gtk_widget_set_sensitive(remove_all_button_, row_count > 0);
}

void ContentExceptionsWindowGtk::Add(GtkWidget* widget) {
  new ContentExceptionEditor(GTK_WINDOW(dialog_),
                             this, model_.get(), allow_off_the_record_, -1,
                             ContentSettingsPattern(),
                             CONTENT_SETTING_BLOCK, false);
}

void ContentExceptionsWindowGtk::Edit(GtkWidget* widget) {
  std::set<std::pair<int, int> > indices;
  GetSelectedModelIndices(&indices);
  DCHECK_GT(indices.size(), 0u);
  int index = indices.begin()->first;
  const HostContentSettingsMap::PatternSettingPair& entry =
      model_->entry_at(index);
  new ContentExceptionEditor(GTK_WINDOW(dialog_), this, model_.get(),
                             allow_off_the_record_, index,
                             entry.first, entry.second,
                             model_->entry_is_off_the_record(index));
}

void ContentExceptionsWindowGtk::Remove(GtkWidget* widget) {
  std::set<std::pair<int, int> > model_selected_indices;
  GetSelectedModelIndices(&model_selected_indices);

  int selected_row = gtk_tree_model_iter_n_children(
      GTK_TREE_MODEL(sort_list_store_), NULL);
  for (std::set<std::pair<int, int> >::reverse_iterator i =
           model_selected_indices.rbegin();
       i != model_selected_indices.rend(); ++i) {
    model_->RemoveException(i->first);
    selected_row = std::min(selected_row, i->second);
  }

  int row_count = model_->RowCount();
  if (row_count <= 0)
    return;
  if (selected_row >= row_count)
    selected_row = row_count - 1;
  gtk_tree::SelectAndFocusRowNum(selected_row,
                                 GTK_TREE_VIEW(treeview_));

  UpdateButtonState();
}

void ContentExceptionsWindowGtk::RemoveAll(GtkWidget* widget) {
  model_->RemoveAll();
  UpdateButtonState();
}

std::string ContentExceptionsWindowGtk::GetWindowTitle() const {
  switch (model_->content_type()) {
    case CONTENT_SETTINGS_TYPE_COOKIES:
      return l10n_util::GetStringUTF8(IDS_COOKIE_EXCEPTION_TITLE);
    case CONTENT_SETTINGS_TYPE_IMAGES:
      return l10n_util::GetStringUTF8(IDS_IMAGES_EXCEPTION_TITLE);
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
      return l10n_util::GetStringUTF8(IDS_JS_EXCEPTION_TITLE);
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      return l10n_util::GetStringUTF8(IDS_PLUGINS_EXCEPTION_TITLE);
    case CONTENT_SETTINGS_TYPE_POPUPS:
      return l10n_util::GetStringUTF8(IDS_POPUP_EXCEPTION_TITLE);
    default:
      NOTREACHED();
  }
  return std::string();
}

void ContentExceptionsWindowGtk::GetSelectedModelIndices(
    std::set<std::pair<int, int> >* indices) {
  GtkTreeModel* model;
  GList* paths = gtk_tree_selection_get_selected_rows(treeview_selection_,
                                                      &model);
  for (GList* item = paths; item; item = item->next) {
    GtkTreePath* sorted_path = reinterpret_cast<GtkTreePath*>(item->data);
    int sorted_row = gtk_tree::GetRowNumForPath(sorted_path);
    GtkTreePath* path = gtk_tree_model_sort_convert_path_to_child_path(
        GTK_TREE_MODEL_SORT(sort_list_store_), sorted_path);
    int row = gtk_tree::GetRowNumForPath(path);
    gtk_tree_path_free(path);
    indices->insert(std::make_pair(row, sorted_row));
  }

  g_list_foreach(paths, (GFunc)gtk_tree_path_free, NULL);
  g_list_free(paths);
}

void ContentExceptionsWindowGtk::OnTreeViewRowActivate(
    GtkWidget* sender,
    GtkTreePath* path,
    GtkTreeViewColumn* column) {
  Edit(sender);
}

void ContentExceptionsWindowGtk::OnWindowDestroy(GtkWidget* widget) {
  instances[model_->content_type()] = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void ContentExceptionsWindowGtk::OnTreeSelectionChanged(
    GtkWidget* selection) {
  UpdateButtonState();
}
