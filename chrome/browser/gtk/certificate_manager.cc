// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/certificate_manager.h"

#include <cert.h>

#include <gtk/gtk.h>

#include "app/l10n_util.h"
#include "chrome/browser/gtk/certificate_viewer.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "grit/generated_resources.h"

namespace {

////////////////////////////////////////////////////////////////////////////////
// CertificatePage class definition.

class CertificatePage {
 public:
  // The categories of certificates that can be displayed.
  enum CertType {
    USER_CERTS = 0,
    EMAIL_CERTS,
    SERVER_CERTS,
    CA_CERTS,
    UNKNOWN_CERTS,
    NUM_CERT_TYPES
  };

  explicit CertificatePage(CertType type);

  // Get the top-level widget of this page.
  GtkWidget* widget() {
    return vbox_;
  }

 private:
  // Columns of the tree store.
  enum {
    CERT_NAME,
    CERT_SECURITY_DEVICE,
    CERT_SERIAL_NUMBER,
    CERT_EXPIRES_ON,
    CERT_ADDRESS,
    CERT_POINTER,
    CERT_STORE_NUM_COLUMNS
  };

  // Gtk event callbacks.
  static void OnSelectionChanged(GtkTreeSelection* selection,
                                 CertificatePage* page);
  static void OnViewClicked(GtkButton *button, CertificatePage* page);

  // The top-level widget of this page.
  GtkWidget* vbox_;

  GtkTreeStore* store_;
  GtkTreeSelection* selection_;

  GtkWidget* view_button_;
};

////////////////////////////////////////////////////////////////////////////////
// CertificatePage implementation.

CertificatePage::CertificatePage(CertType type) {
  vbox_ = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(vbox_),
                                 gtk_util::kContentAreaBorder);

  static const int kDescriptionIds[] = {
    IDS_CERT_MANAGER_USER_TREE_DESCRIPTION,
    IDS_CERT_MANAGER_OTHER_PEOPLE_TREE_DESCRIPTION,
    IDS_CERT_MANAGER_SERVER_TREE_DESCRIPTION,
    IDS_CERT_MANAGER_AUTHORITIES_TREE_DESCRIPTION,
    IDS_CERT_MANAGER_UNKNOWN_TREE_DESCRIPTION,
  };
  DCHECK_EQ(arraysize(kDescriptionIds),
            static_cast<size_t>(NUM_CERT_TYPES));
  GtkWidget* description_label = gtk_label_new(l10n_util::GetStringUTF8(
      kDescriptionIds[type]).c_str());
  gtk_misc_set_alignment(GTK_MISC(description_label), 0, 0.5);
  gtk_box_pack_start(GTK_BOX(vbox_), description_label, FALSE, FALSE, 0);

  store_ = gtk_tree_store_new(CERT_STORE_NUM_COLUMNS,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_POINTER);
  GtkWidget* tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store_));
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree), TRUE);
  selection_ = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree));
  gtk_tree_selection_set_mode(selection_, GTK_SELECTION_SINGLE);
  g_signal_connect(selection_, "changed", G_CALLBACK(OnSelectionChanged), this);

  gtk_tree_view_append_column(
      GTK_TREE_VIEW(tree),
      gtk_tree_view_column_new_with_attributes(
          l10n_util::GetStringUTF8(IDS_CERT_MANAGER_NAME_COLUMN_LABEL).c_str(),
          gtk_cell_renderer_text_new(),
          "text", CERT_NAME,
          NULL));

  if (type == USER_CERTS || type == CA_CERTS || type == UNKNOWN_CERTS)
    gtk_tree_view_append_column(
        GTK_TREE_VIEW(tree),
        gtk_tree_view_column_new_with_attributes(
            l10n_util::GetStringUTF8(
                IDS_CERT_MANAGER_DEVICE_COLUMN_LABEL).c_str(),
            gtk_cell_renderer_text_new(),
            "text", CERT_SECURITY_DEVICE,
            NULL));

  if (type == USER_CERTS)
    gtk_tree_view_append_column(
        GTK_TREE_VIEW(tree),
        gtk_tree_view_column_new_with_attributes(
            l10n_util::GetStringUTF8(
                IDS_CERT_MANAGER_SERIAL_NUMBER_COLUMN_LABEL).c_str(),
            gtk_cell_renderer_text_new(),
            "text", CERT_SERIAL_NUMBER,
            NULL));

  if (type == USER_CERTS || type == EMAIL_CERTS || type == SERVER_CERTS)
    gtk_tree_view_append_column(
        GTK_TREE_VIEW(tree),
        gtk_tree_view_column_new_with_attributes(
            l10n_util::GetStringUTF8(
                IDS_CERT_MANAGER_EXPIRES_COLUMN_LABEL).c_str(),
            gtk_cell_renderer_text_new(),
            "text", CERT_EXPIRES_ON,
            NULL));

  if (type == EMAIL_CERTS)
    gtk_tree_view_append_column(
        GTK_TREE_VIEW(tree),
        gtk_tree_view_column_new_with_attributes(
            l10n_util::GetStringUTF8(
                IDS_CERT_MANAGER_EMAIL_ADDRESS_COLUMN_LABEL).c_str(),
            gtk_cell_renderer_text_new(),
            "text", CERT_ADDRESS,
            NULL));

  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(
      GTK_SCROLLED_WINDOW(scroll_window), GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(scroll_window), tree);
  gtk_box_pack_start(GTK_BOX(vbox_), scroll_window, TRUE, TRUE, 0);

  GtkWidget* button_box = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(vbox_), button_box, FALSE, FALSE, 0);

  view_button_ = gtk_button_new_with_mnemonic(
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(
              IDS_CERT_MANAGER_VIEW_CERT_BUTTON)).c_str());
  gtk_widget_set_sensitive(view_button_, FALSE);
  g_signal_connect(view_button_, "clicked",
                   G_CALLBACK(OnViewClicked), this);
  gtk_box_pack_start(GTK_BOX(button_box), view_button_, FALSE, FALSE, 0);

  // TODO(mattm): Add buttons for import, export, delete, etc

  // TODO(mattm): Populate the tree.
}

// static
void CertificatePage::OnSelectionChanged(GtkTreeSelection* selection,
                                         CertificatePage* page) {
  CERTCertificate* cert = NULL;
  GtkTreeIter iter;
  GtkTreeModel* model;
  if (gtk_tree_selection_get_selected(page->selection_, &model, &iter))
    gtk_tree_model_get(model, &iter, CERT_POINTER, &cert, -1);

  gtk_widget_set_sensitive(page->view_button_, cert ? TRUE : FALSE);
}

// static
void CertificatePage::OnViewClicked(GtkButton *button, CertificatePage* page) {
  GtkTreeIter iter;
  GtkTreeModel* model;
  if (!gtk_tree_selection_get_selected(page->selection_, &model, &iter))
    return;

  CERTCertificate* cert = NULL;
  gtk_tree_model_get(model, &iter, CERT_POINTER, &cert, -1);
  if (cert)
    ShowCertificateViewer(GTK_WINDOW(gtk_widget_get_toplevel(page->widget())),
                          cert);
}

////////////////////////////////////////////////////////////////////////////////
// CertificateManager class definition.

class CertificateManager {
 public:
  explicit CertificateManager(gfx::NativeWindow parent);

  void Show();

 private:
  CertificatePage user_page_;
  CertificatePage email_page_;
  CertificatePage server_page_;
  CertificatePage ca_page_;
  CertificatePage unknown_page_;

  GtkWidget* dialog_;
};

////////////////////////////////////////////////////////////////////////////////
// CertificateManager implementation.

void OnDestroy(GtkDialog* dialog, CertificateManager* cert_manager) {
  delete cert_manager;
}

CertificateManager::CertificateManager(gfx::NativeWindow parent)
    : user_page_(CertificatePage::USER_CERTS),
      email_page_(CertificatePage::EMAIL_CERTS),
      server_page_(CertificatePage::SERVER_CERTS),
      ca_page_(CertificatePage::CA_CERTS),
      unknown_page_(CertificatePage::UNKNOWN_CERTS) {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_CERTIFICATE_MANAGER_TITLE).c_str(),
      parent,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      GTK_STOCK_CLOSE,
      GTK_RESPONSE_CLOSE,
      NULL);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);
  gtk_window_set_default_size(GTK_WINDOW(dialog_), 600, 440);

  GtkWidget* notebook = gtk_notebook_new();
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), notebook);

  // TODO(mattm): Remember which page user viewed last.
  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook),
      user_page_.widget(),
      gtk_label_new_with_mnemonic(
          l10n_util::GetStringUTF8(
              IDS_CERT_MANAGER_PERSONAL_CERTS_TAB_LABEL).c_str()));

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook),
      email_page_.widget(),
      gtk_label_new_with_mnemonic(
          l10n_util::GetStringUTF8(
              IDS_CERT_MANAGER_OTHER_PEOPLES_CERTS_TAB_LABEL).c_str()));

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook),
      server_page_.widget(),
      gtk_label_new_with_mnemonic(
          l10n_util::GetStringUTF8(
              IDS_CERT_MANAGER_SERVER_CERTS_TAB_LABEL).c_str()));

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook),
      ca_page_.widget(),
      gtk_label_new_with_mnemonic(
          l10n_util::GetStringUTF8(
              IDS_CERT_MANAGER_CERT_AUTHORITIES_TAB_LABEL).c_str()));

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook),
      unknown_page_.widget(),
      gtk_label_new_with_mnemonic(
          l10n_util::GetStringUTF8(
              IDS_CERT_MANAGER_UNKNOWN_TAB_LABEL).c_str()));

  g_signal_connect(dialog_, "response", G_CALLBACK(gtk_widget_destroy), NULL);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnDestroy), this);
}

void CertificateManager::Show() {
  gtk_widget_show_all(dialog_);
}

} // namespace

void ShowCertificateManager(gfx::NativeWindow parent) {
  (new CertificateManager(parent))->Show();
}
