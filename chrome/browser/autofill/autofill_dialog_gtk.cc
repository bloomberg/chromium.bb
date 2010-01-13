// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_dialog.h"

#include <gtk/gtk.h>

#include <vector>

#include "app/l10n_util.h"
#include "base/message_loop.h"
#include "chrome/browser/gtk/options/options_layout_gtk.h"
#include "chrome/common/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

// Column IDs for |addresses_model_|.
enum {
  ADDRESSES_COL_LABEL,
  ADDRESSES_COL_COUNT,
};

// Column IDs for |creditcards_model_|.
enum {
  CREDITCARDS_COL_LABEL,
  CREDITCARDS_COL_COUNT,
};

// The contents of the AutoFill dialog.  This dialog allows users to add, edit
// and remove AutoFill profiles.
class AutoFillDialog {
 public:
  explicit AutoFillDialog(std::vector<AutoFillProfile>* profiles,
                          std::vector<FormGroup>* credit_cards);
  ~AutoFillDialog() {}

  // Shows the AutoFill dialog.
  void Show();

 private:
  // 'destroy' signal handler.  We DeleteSoon the global singleton dialog object
  // from here.
  static void OnDestroy(GtkWidget* widget, AutoFillDialog* autofill_dialog);

  // 'changed' signal handler which notifies us when the user has selected a
  // different Address profile.
  static void OnAddressesSelectionChanged(GtkTreeSelection *selection,
                                          AutoFillDialog* dialog);

  // 'changed' signal handler which notifies us when the user has selected a
  // different CreditCard profile.
  static void OnCreditCardSelectionChanged(GtkTreeSelection *selection,
                                           AutoFillDialog* dialog);

  // 'clicked' signal handler which is sent when a user clicks on one of the
  // Add/Edit/Remove Address buttons.
  static void OnAddAddressClicked(GtkButton* button, AutoFillDialog* dialog);
  static void OnEditAddressClicked(GtkButton* button, AutoFillDialog* dialog);
  static void OnRemoveAddressClicked(GtkButton* button, AutoFillDialog* dialog);

  // 'clicked' signal handler which is sent when a user clicks on one of the
  // Add/Edit/Remove Credit Card buttons.
  static void OnAddCreditCardClicked(GtkButton* button, AutoFillDialog* dialog);
  static void OnEditCreditCardClicked(GtkButton* button,
                                      AutoFillDialog* dialog);
  static void OnRemoveCreditCardClicked(GtkButton* button,
                                        AutoFillDialog* dialog);

  // Initialize the group widgets, return their container.
  GtkWidget* InitAddressesGroup();
  GtkWidget* InitCreditCardsGroup();

  // The list of current AutoFill profiles.  Owned by AutoFillManager.
  std::vector<AutoFillProfile>* profiles_;

  // The list of current AutoFill credit cards.  Owned by AutoFillManager.
  std::vector<FormGroup>* credit_cards_;

  // The AutoFill dialog.
  GtkWidget* dialog_;

  // Widgets of the Addresses group.
  GtkWidget* addresses_tree_;
  GtkListStore* addresses_model_;
  GtkTreeSelection* addresses_selection_;
  // TODO(jhawkins): We might not need to store these widgets.  The add button
  // should never need to be desensitized, but we should desensitize the edit
  // and remove buttons if there is no current selection.
  GtkWidget* addresses_add_button_;
  GtkWidget* addresses_edit_button_;
  GtkWidget* addresses_remove_button_;

  // Widgets of the Credit Cards group.
  GtkWidget* creditcards_tree_;
  GtkListStore* creditcards_model_;
  GtkTreeSelection* creditcards_selection_;
  // TODO(jhawkins): We might not need to store these widgets.  The add button
  // should never need to be desensitized, but we should desensitize the edit
  // and remove buttons if there is no current selection.
  GtkWidget* creditcards_add_button_;
  GtkWidget* creditcards_edit_button_;
  GtkWidget* creditcards_remove_button_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillDialog);
};

// The singleton AutoFill dialog object.
static AutoFillDialog* dialog = NULL;

AutoFillDialog::AutoFillDialog(std::vector<AutoFillProfile>* profiles,
                               std::vector<FormGroup>* credit_cards)
    : profiles_(profiles),
      credit_cards_(credit_cards) {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_DIALOG_TITLE).c_str(),
      // AutoFill dialog is shared between all browser windows.
      NULL,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_CLOSE,
      GTK_RESPONSE_CLOSE,
      NULL);

  gtk_widget_realize(dialog_);
  gtk_util::SetWindowWidthFromResources(GTK_WINDOW(dialog_),
                                        IDS_AUTOFILL_DIALOG_WIDTH_CHARS,
                                        true);

  // Allow browser windows to go in front of the AutoFill dialog in Metacity.
  gtk_window_set_type_hint(GTK_WINDOW(dialog_), GDK_WINDOW_TYPE_HINT_NORMAL);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);
  g_signal_connect(dialog_, "response", G_CALLBACK(gtk_widget_destroy), NULL);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnDestroy), this);

  // TODO(jhawkins): Refactor OptionsLayoutBuilderGtk out of gtk/options.
  OptionsLayoutBuilderGtk options_builder;
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_ADDRESSES_GROUP_NAME),
      InitAddressesGroup(), true);
  options_builder.AddOptionGroup(
      l10n_util::GetStringUTF8(IDS_AUTOFILL_CREDITCARDS_GROUP_NAME),
      InitCreditCardsGroup(), true);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox),
                    options_builder.get_page_widget());

  gtk_widget_show_all(dialog_);
}

void AutoFillDialog::Show() {
  gtk_window_present_with_time(GTK_WINDOW(dialog_),
                               gtk_get_current_event_time());
}

// static
void AutoFillDialog::OnDestroy(GtkWidget* widget,
                               AutoFillDialog* autofill_dialog) {
  dialog = NULL;
  MessageLoop::current()->DeleteSoon(FROM_HERE, autofill_dialog);
}

// static
void AutoFillDialog::OnAddressesSelectionChanged(GtkTreeSelection *selection,
                                                 AutoFillDialog* dialog) {
}

// static
void AutoFillDialog::OnCreditCardSelectionChanged(GtkTreeSelection *selection,
                                                  AutoFillDialog* dialog) {
}

// static
void AutoFillDialog::OnAddAddressClicked(GtkButton* button,
                                         AutoFillDialog* dialog) {
  // TODO(jhawkins): Open the EditAddress dialog with an empty profile.
}

// static
void AutoFillDialog::OnEditAddressClicked(GtkButton* button,
                                          AutoFillDialog* dialog) {
  // TODO(jhawkins): Implement the EditAddress dialog.
}

// static
void AutoFillDialog::OnRemoveAddressClicked(GtkButton* button,
                                            AutoFillDialog* dialog) {
  // TODO(jhawkins): Remove the selected profile from |profiles_|.
}

// static
void AutoFillDialog::OnAddCreditCardClicked(GtkButton* button,
                                            AutoFillDialog* dialog) {
  // TODO(jhawkins): Open the EditCreditCard dialog with an empty profile.
}

// static
void AutoFillDialog::OnEditCreditCardClicked(GtkButton* button,
                                             AutoFillDialog* dialog) {
  // TODO(jhawkins): Implement the EditAddress dialog.
}

// static
void AutoFillDialog::OnRemoveCreditCardClicked(GtkButton* button,
                                               AutoFillDialog* dialog) {
  // TODO(jhawkins): Remove the selected profile from |profiles_|.
}

GtkWidget* AutoFillDialog::InitAddressesGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  // Addresses container hbox.
  GtkWidget* addresses_container = gtk_hbox_new(FALSE,
                                                gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(vbox), addresses_container, TRUE, TRUE, 0);

  // Addresses container scroll window.
  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(addresses_container), scroll_window);

  addresses_model_ = gtk_list_store_new(ADDRESSES_COL_COUNT,
                                        G_TYPE_STRING);
  addresses_tree_ = gtk_tree_view_new_with_model(
      GTK_TREE_MODEL(addresses_model_));

  // Release |addresses_model_| so that |addresses_tree_| owns the model.
  g_object_unref(addresses_model_);

  gtk_container_add(GTK_CONTAINER(scroll_window), addresses_tree_);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(addresses_tree_), FALSE);

  // Addresses column.
  GtkTreeViewColumn* column;
  GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes("label",
                                                    renderer,
                                                    "text", ADDRESSES_COL_LABEL,
                                                    NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(addresses_tree_), column);

  // Addresses selection.
  addresses_selection_ = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(addresses_tree_));
  gtk_tree_selection_set_mode(addresses_selection_, GTK_SELECTION_SINGLE);
  g_signal_connect(G_OBJECT(addresses_selection_), "changed",
                   G_CALLBACK(OnAddressesSelectionChanged), this);

  GtkWidget* addresses_buttons = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_end(GTK_BOX(addresses_container), addresses_buttons,
                   FALSE, FALSE, 0);

  // Add Address button.
  addresses_add_button_ = gtk_button_new_with_label(
          l10n_util::GetStringUTF8(IDS_AUTOFILL_ADD_BUTTON).c_str());
  g_signal_connect(G_OBJECT(addresses_add_button_), "clicked",
                   G_CALLBACK(OnAddAddressClicked), this);
  gtk_box_pack_start(GTK_BOX(addresses_buttons), addresses_add_button_,
                     FALSE, FALSE, 0);

  // Edit Address button.
  addresses_edit_button_ = gtk_button_new_with_label(
        l10n_util::GetStringUTF8(IDS_AUTOFILL_EDIT_BUTTON).c_str());
  g_signal_connect(G_OBJECT(addresses_edit_button_), "clicked",
                   G_CALLBACK(OnEditAddressClicked), this);
  gtk_box_pack_start(GTK_BOX(addresses_buttons), addresses_edit_button_,
                     FALSE, FALSE, 0);

  // Remove Address button.
  addresses_remove_button_ = gtk_button_new_with_label(
          l10n_util::GetStringUTF8(IDS_AUTOFILL_REMOVE_BUTTON).c_str());
  g_signal_connect(G_OBJECT(addresses_remove_button_), "clicked",
                   G_CALLBACK(OnRemoveAddressClicked), this);
  gtk_box_pack_start(GTK_BOX(addresses_buttons), addresses_remove_button_,
                     FALSE, FALSE, 0);

  return vbox;
}

GtkWidget* AutoFillDialog::InitCreditCardsGroup() {
  GtkWidget* vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  // Credit Cards container hbox.
  GtkWidget* creditcards_container = gtk_hbox_new(FALSE,
                                                gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(vbox), creditcards_container, TRUE, TRUE, 0);

  // Credit Cards container scroll window.
  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scroll_window),
                                      GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(creditcards_container), scroll_window);

  creditcards_model_ = gtk_list_store_new(CREDITCARDS_COL_COUNT,
                                          G_TYPE_STRING);
  creditcards_tree_ = gtk_tree_view_new_with_model(
      GTK_TREE_MODEL(creditcards_model_));

  // Release |creditcards_model_| so that |creditcards_tree_| owns the model.
  g_object_unref(creditcards_model_);

  gtk_container_add(GTK_CONTAINER(scroll_window), creditcards_tree_);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(creditcards_tree_), FALSE);

  // Credit Cards column.
  GtkTreeViewColumn* column;
  GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
  column = gtk_tree_view_column_new_with_attributes(
      "label",
      renderer,
      "text", CREDITCARDS_COL_LABEL,
      NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(creditcards_tree_), column);

  // Credit Cards selection.
  creditcards_selection_ = gtk_tree_view_get_selection(
      GTK_TREE_VIEW(creditcards_tree_));
  gtk_tree_selection_set_mode(creditcards_selection_, GTK_SELECTION_SINGLE);
  g_signal_connect(G_OBJECT(creditcards_selection_), "changed",
                   G_CALLBACK(OnCreditCardSelectionChanged), this);

  GtkWidget* creditcards_buttons = gtk_vbox_new(FALSE,
                                                gtk_util::kControlSpacing);
  gtk_box_pack_end(GTK_BOX(creditcards_container), creditcards_buttons,
                   FALSE, FALSE, 0);

  // Address Add button.
  creditcards_add_button_ = gtk_button_new_with_label(
          l10n_util::GetStringUTF8(IDS_AUTOFILL_ADD_BUTTON).c_str());
  g_signal_connect(G_OBJECT(creditcards_add_button_), "clicked",
                   G_CALLBACK(OnAddCreditCardClicked), this);
  gtk_box_pack_start(GTK_BOX(creditcards_buttons), creditcards_add_button_,
                     FALSE, FALSE, 0);

  // Credit Cards Edit button.
  creditcards_edit_button_ = gtk_button_new_with_label(
        l10n_util::GetStringUTF8(IDS_AUTOFILL_EDIT_BUTTON).c_str());
  g_signal_connect(G_OBJECT(creditcards_edit_button_), "clicked",
                   G_CALLBACK(OnEditCreditCardClicked), this);
  gtk_box_pack_start(GTK_BOX(creditcards_buttons), creditcards_edit_button_,
                     FALSE, FALSE, 0);

  // Address Remove button.
  creditcards_remove_button_ = gtk_button_new_with_label(
          l10n_util::GetStringUTF8(IDS_AUTOFILL_REMOVE_BUTTON).c_str());
  g_signal_connect(G_OBJECT(creditcards_remove_button_), "clicked",
                   G_CALLBACK(OnRemoveCreditCardClicked), this);
  gtk_box_pack_start(GTK_BOX(creditcards_buttons), creditcards_remove_button_,
                     FALSE, FALSE, 0);

  return vbox;
}

///////////////////////////////////////////////////////////////////////////////
// Factory/finder method:

void ShowAutoFillDialog(std::vector<AutoFillProfile>* profiles,
                        std::vector<FormGroup>* credit_cards) {
  if (!dialog) {
    dialog = new AutoFillDialog(profiles, credit_cards);
  }
  dialog->Show();
}
