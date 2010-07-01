// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/certificate_manager.h"

#include <cert.h>
#include <gtk/gtk.h>
#include <pk11pub.h>

#include <map>
#include <string>

#include "app/gtk_signal.h"
#include "app/l10n_util.h"
#include "app/l10n_util_collator.h"
#include "base/gtk_util.h"
#include "base/i18n/time_formatting.h"
#include "base/nss_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/gtk/certificate_viewer.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/pref_member.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/third_party/mozilla_security_manager/nsNSSCertHelper.h"
#include "chrome/third_party/mozilla_security_manager/nsNSSCertificate.h"
#include "grit/generated_resources.h"

// PSM = Mozilla's Personal Security Manager.
namespace psm = mozilla_security_manager;

namespace {

// Convert a char* return value from NSS into a std::string and free the NSS
// memory.  If the arg is NULL, an empty string will be returned instead.
std::string Stringize(char* nss_text) {
  std::string s;
  if (nss_text) {
    s = nss_text;
    PORT_Free(nss_text);
  }
  return s;
}

////////////////////////////////////////////////////////////////////////////////
// CertificatePage class definition.

class CertificatePage {
 public:
  explicit CertificatePage(psm::CertType type);

  void PopulateTree(CERTCertList* cert_list);

  // Get the top-level widget of this page.
  GtkWidget* widget() { return vbox_; }

 private:
  // Columns of the tree store.
  enum {
    CERT_NAME,
    CERT_SECURITY_DEVICE,
    CERT_SERIAL_NUMBER,
    CERT_EXPIRES_ON,
    CERT_EXPIRES_ON_INT,
    CERT_ADDRESS,
    CERT_POINTER,
    CERT_STORE_NUM_COLUMNS
  };

  gint LocaleSortFunc(GtkTreeModel* model, GtkTreeIter* a, GtkTreeIter* b,
                      int col);

  // Gtk event callbacks.
  CHROMEG_CALLBACK_2(CertificatePage, gint, SortNameFunc, GtkTreeModel*,
                     GtkTreeIter*, GtkTreeIter*);
  CHROMEG_CALLBACK_2(CertificatePage, gint, SortDeviceFunc, GtkTreeModel*,
                     GtkTreeIter*, GtkTreeIter*);
  CHROMEG_CALLBACK_0(CertificatePage, void, OnSelectionChanged,
                     GtkTreeSelection*);
  CHROMEGTK_CALLBACK_0(CertificatePage, void, OnViewClicked);

  psm::CertType type_;

  // The top-level widget of this page.
  GtkWidget* vbox_;

  GtkWidget* tree_;
  GtkTreeStore* store_;
  GtkTreeSelection* selection_;
  scoped_ptr<icu::Collator> collator_;

  GtkWidget* view_button_;
};

////////////////////////////////////////////////////////////////////////////////
// CertificatePage implementation.

CertificatePage::CertificatePage(psm::CertType type) : type_(type) {
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
            static_cast<size_t>(psm::NUM_CERT_TYPES));
  GtkWidget* description_label = gtk_label_new(l10n_util::GetStringUTF8(
      kDescriptionIds[type]).c_str());
  gtk_util::LeftAlignMisc(description_label);
  gtk_box_pack_start(GTK_BOX(vbox_), description_label, FALSE, FALSE, 0);

  store_ = gtk_tree_store_new(CERT_STORE_NUM_COLUMNS,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_STRING,
                              G_TYPE_INT64,
                              G_TYPE_STRING,
                              G_TYPE_POINTER);
  tree_ = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store_));
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_), TRUE);
  selection_ = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_));
  gtk_tree_selection_set_mode(selection_, GTK_SELECTION_SINGLE);
  g_signal_connect(selection_, "changed", G_CALLBACK(OnSelectionChangedThunk),
                   this);

  GtkTreeViewColumn* name_col = gtk_tree_view_column_new_with_attributes(
      l10n_util::GetStringUTF8(IDS_CERT_MANAGER_NAME_COLUMN_LABEL).c_str(),
      gtk_cell_renderer_text_new(),
      "text", CERT_NAME,
      NULL);
  gtk_tree_view_column_set_sort_column_id(name_col, CERT_NAME);
  gtk_tree_view_append_column(GTK_TREE_VIEW(tree_), name_col);

  if (type == psm::USER_CERT || type == psm::CA_CERT ||
      type == psm::UNKNOWN_CERT) {
    GtkTreeViewColumn* device_col = gtk_tree_view_column_new_with_attributes(
        l10n_util::GetStringUTF8(
            IDS_CERT_MANAGER_DEVICE_COLUMN_LABEL).c_str(),
        gtk_cell_renderer_text_new(),
        "text", CERT_SECURITY_DEVICE,
        NULL);
    gtk_tree_view_column_set_sort_column_id(device_col, CERT_SECURITY_DEVICE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_), device_col);
  }

  if (type == psm::USER_CERT) {
    GtkTreeViewColumn* serial_col = gtk_tree_view_column_new_with_attributes(
        l10n_util::GetStringUTF8(
            IDS_CERT_MANAGER_SERIAL_NUMBER_COLUMN_LABEL).c_str(),
        gtk_cell_renderer_text_new(),
        "text", CERT_SERIAL_NUMBER,
        NULL);
    gtk_tree_view_column_set_sort_column_id(serial_col, CERT_SERIAL_NUMBER);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_), serial_col);
  }

  if (type == psm::USER_CERT || type == psm::EMAIL_CERT ||
      type == psm::SERVER_CERT) {
    GtkTreeViewColumn* expires_col = gtk_tree_view_column_new_with_attributes(
        l10n_util::GetStringUTF8(
            IDS_CERT_MANAGER_EXPIRES_COLUMN_LABEL).c_str(),
        gtk_cell_renderer_text_new(),
        "text", CERT_EXPIRES_ON,
        NULL);
    gtk_tree_view_column_set_sort_column_id(expires_col, CERT_EXPIRES_ON_INT);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_), expires_col);
  }

  if (type == psm::EMAIL_CERT) {
    GtkTreeViewColumn* addr_col = gtk_tree_view_column_new_with_attributes(
        l10n_util::GetStringUTF8(
            IDS_CERT_MANAGER_EMAIL_ADDRESS_COLUMN_LABEL).c_str(),
        gtk_cell_renderer_text_new(),
        "text", CERT_ADDRESS,
        NULL);
    gtk_tree_view_column_set_sort_column_id(addr_col, CERT_ADDRESS);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_), addr_col);
  }

  UErrorCode error = U_ZERO_ERROR;
  collator_.reset(
      icu::Collator::createInstance(
          icu::Locale(g_browser_process->GetApplicationLocale().c_str()),
          error));
  if (U_FAILURE(error))
    collator_.reset(NULL);
  if (collator_ != NULL) {
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store_), CERT_NAME,
                                    SortNameFuncThunk, this, NULL);
    gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(store_),
                                    CERT_SECURITY_DEVICE, SortDeviceFuncThunk,
                                    this, NULL);
  }

  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store_), CERT_NAME,
                                       GTK_SORT_ASCENDING);

  GtkWidget* scroll_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll_window),
                                 GTK_POLICY_AUTOMATIC,
                                 GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(
      GTK_SCROLLED_WINDOW(scroll_window), GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(scroll_window), tree_);
  gtk_box_pack_start(GTK_BOX(vbox_), scroll_window, TRUE, TRUE, 0);

  GtkWidget* button_box = gtk_hbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(vbox_), button_box, FALSE, FALSE, 0);

  view_button_ = gtk_button_new_with_mnemonic(
      gtk_util::ConvertAcceleratorsFromWindowsStyle(
          l10n_util::GetStringUTF8(
              IDS_CERT_MANAGER_VIEW_CERT_BUTTON)).c_str());
  gtk_widget_set_sensitive(view_button_, FALSE);
  g_signal_connect(view_button_, "clicked",
                   G_CALLBACK(OnViewClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(button_box), view_button_, FALSE, FALSE, 0);

  // TODO(mattm): Add buttons for import, export, delete, etc
}

void CertificatePage::PopulateTree(CERTCertList* cert_list) {
  DCHECK(gtk_tree_model_get_flags(GTK_TREE_MODEL(store_)) &
         GTK_TREE_MODEL_ITERS_PERSIST);

  typedef std::map<std::string, GtkTreeIter> OrgTreeMap;
  OrgTreeMap org_tree_map;

  CERTCertListNode* node;
  for (node = CERT_LIST_HEAD(cert_list);
       !CERT_LIST_END(node, cert_list);
       node = CERT_LIST_NEXT(node)) {
    CERTCertificate* cert = node->cert;
    psm::CertType type = psm::GetCertType(cert);
    if (type == type_) {
      std::string org = Stringize(CERT_GetOrgName(&cert->subject));
      if (org.empty())
        org = Stringize(CERT_GetCommonName(&cert->subject));
      OrgTreeMap::iterator org_tree_map_iter = org_tree_map.find(org);
      if (org_tree_map_iter == org_tree_map.end()) {
        GtkTreeIter iter;
        gtk_tree_store_append(store_, &iter, NULL);
        gtk_tree_store_set(store_, &iter, CERT_NAME, org.c_str(), -1);
        org_tree_map_iter = org_tree_map.insert(std::make_pair(org,
                                                               iter)).first;
      }
      std::string name = psm::ProcessIDN(
          Stringize(CERT_GetCommonName(&cert->subject)));
      if (name.empty() && cert->nickname) {
        name = cert->nickname;
        // Hack copied from mozilla: Cut off text before first :, which seems to
        // just be the token name.
        size_t colon_pos = name.find(':');
        if (colon_pos != std::string::npos)
          name = name.substr(colon_pos + 1);
      }
      GtkTreeIter iter;
      gtk_tree_store_append(store_, &iter, &org_tree_map_iter->second);
      gtk_tree_store_set(store_, &iter,
                         CERT_NAME, name.c_str(),
                         CERT_SECURITY_DEVICE,
                         psm::GetCertTokenName(cert).c_str(),
                         CERT_SERIAL_NUMBER,
                         Stringize(CERT_Hexify(
                             &cert->serialNumber, TRUE)).c_str(),
                         CERT_ADDRESS, cert->emailAddr,
                         CERT_POINTER, cert,
                         -1);

      PRTime issued, expires;
      if (CERT_GetCertTimes(cert, &issued, &expires) == SECSuccess) {
        gtk_tree_store_set(store_, &iter,
                           CERT_EXPIRES_ON,
                           WideToUTF8(base::TimeFormatShortDateNumeric(
                               base::PRTimeToBaseTime(expires))).c_str(),
                           CERT_EXPIRES_ON_INT, expires,
                           -1);
      }
    }
  }

  gtk_tree_view_expand_all(GTK_TREE_VIEW(tree_));
}

gint CertificatePage::LocaleSortFunc(GtkTreeModel* model,
                                     GtkTreeIter* a,
                                     GtkTreeIter* b,
                                     int col) {
  gchar* value1 = NULL;
  gchar* value2 = NULL;
  gtk_tree_model_get(model, a, col, &value1, -1);
  gtk_tree_model_get(model, b, col, &value2, -1);
  if (!value1 || !value2) {
    if (value1)
      return 1;
    if (value2)
      return -1;
    return 0;
  }

  return l10n_util::CompareStringWithCollator(collator_.get(),
                                              UTF8ToWide(value1),
                                              UTF8ToWide(value2));
}

gint CertificatePage::SortNameFunc(GtkTreeModel* model, GtkTreeIter* a,
                                   GtkTreeIter* b) {
  return LocaleSortFunc(model, a, b, CERT_NAME);
}

gint CertificatePage::SortDeviceFunc(GtkTreeModel* model, GtkTreeIter* a,
                                     GtkTreeIter* b) {
  return LocaleSortFunc(model, a, b, CERT_SECURITY_DEVICE);
}

void CertificatePage::OnSelectionChanged(GtkTreeSelection* selection) {
  CERTCertificate* cert = NULL;
  GtkTreeIter iter;
  GtkTreeModel* model;
  if (gtk_tree_selection_get_selected(selection_, &model, &iter))
    gtk_tree_model_get(model, &iter, CERT_POINTER, &cert, -1);

  gtk_widget_set_sensitive(view_button_, cert ? TRUE : FALSE);
}

void CertificatePage::OnViewClicked(GtkWidget* button) {
  GtkTreeIter iter;
  GtkTreeModel* model;
  if (!gtk_tree_selection_get_selected(selection_, &model, &iter))
    return;

  CERTCertificate* cert = NULL;
  gtk_tree_model_get(model, &iter, CERT_POINTER, &cert, -1);
  if (cert)
    ShowCertificateViewer(GTK_WINDOW(gtk_widget_get_toplevel(widget())), cert);
}

////////////////////////////////////////////////////////////////////////////////
// CertificateManager class definition.

class CertificateManager {
 public:
  explicit CertificateManager(gfx::NativeWindow parent, Profile* profile);
  ~CertificateManager();

  // Shows the Tab corresponding to the specified |page|.
  void ShowCertificatePage(CertificateManagerPage page);

 private:
  CHROMEGTK_CALLBACK_2(CertificateManager, void, OnSwitchPage,
                       GtkNotebookPage*, guint);

  CERTCertList* cert_list_;

  CertificatePage user_page_;
  CertificatePage email_page_;
  CertificatePage server_page_;
  CertificatePage ca_page_;
  CertificatePage unknown_page_;

  GtkWidget* dialog_;

  GtkWidget* notebook_;

  // The last page the user was on when they opened the CertificateManager
  // window.
  IntegerPrefMember last_selected_page_;

  DISALLOW_COPY_AND_ASSIGN(CertificateManager);
};

////////////////////////////////////////////////////////////////////////////////
// CertificateManager implementation.

void OnDestroy(GtkDialog* dialog, CertificateManager* cert_manager) {
  delete cert_manager;
}

CertificateManager::CertificateManager(gfx::NativeWindow parent,
                                       Profile* profile)
    : user_page_(psm::USER_CERT),
      email_page_(psm::EMAIL_CERT),
      server_page_(psm::SERVER_CERT),
      ca_page_(psm::CA_CERT),
      unknown_page_(psm::UNKNOWN_CERT) {
  // We don't need to observe changes in this value.
  last_selected_page_.Init(prefs::kCertificateManagerWindowLastTabIndex,
                           profile->GetPrefs(), NULL);

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

  notebook_ = gtk_notebook_new();
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog_)->vbox), notebook_);

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      user_page_.widget(),
      gtk_label_new_with_mnemonic(
          l10n_util::GetStringUTF8(
              IDS_CERT_MANAGER_PERSONAL_CERTS_TAB_LABEL).c_str()));

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      email_page_.widget(),
      gtk_label_new_with_mnemonic(
          l10n_util::GetStringUTF8(
              IDS_CERT_MANAGER_OTHER_PEOPLES_CERTS_TAB_LABEL).c_str()));

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      server_page_.widget(),
      gtk_label_new_with_mnemonic(
          l10n_util::GetStringUTF8(
              IDS_CERT_MANAGER_SERVER_CERTS_TAB_LABEL).c_str()));

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      ca_page_.widget(),
      gtk_label_new_with_mnemonic(
          l10n_util::GetStringUTF8(
              IDS_CERT_MANAGER_CERT_AUTHORITIES_TAB_LABEL).c_str()));

  gtk_notebook_append_page(
      GTK_NOTEBOOK(notebook_),
      unknown_page_.widget(),
      gtk_label_new_with_mnemonic(
          l10n_util::GetStringUTF8(
              IDS_CERT_MANAGER_UNKNOWN_TAB_LABEL).c_str()));

  cert_list_ = PK11_ListCerts(PK11CertListUnique, NULL);
  user_page_.PopulateTree(cert_list_);
  email_page_.PopulateTree(cert_list_);
  server_page_.PopulateTree(cert_list_);
  ca_page_.PopulateTree(cert_list_);
  unknown_page_.PopulateTree(cert_list_);

  // Need to show the notebook before connecting switch-page signal, otherwise
  // we'll immediately get a signal switching to page 0 and overwrite our
  // last_selected_page_ value.
  gtk_util::ShowDialogWithLocalizedSize(dialog_, -1, -1, true);

  g_signal_connect(notebook_, "switch-page",
                   G_CALLBACK(OnSwitchPageThunk), this);

  g_signal_connect(dialog_, "response", G_CALLBACK(gtk_widget_destroy), NULL);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnDestroy), this);
}

CertificateManager::~CertificateManager() {
  CERT_DestroyCertList(cert_list_);
}

void CertificateManager::OnSwitchPage(GtkWidget* notebook,
                                      GtkNotebookPage* page,
                                      guint page_num) {
  int index = static_cast<int>(page_num);
  DCHECK(index > CERT_MANAGER_PAGE_DEFAULT && index < CERT_MANAGER_PAGE_COUNT);
  last_selected_page_.SetValue(index);
}

void CertificateManager::ShowCertificatePage(CertificateManagerPage page) {
  // Bring options window to front if it already existed and isn't already
  // in front
  gtk_window_present_with_time(GTK_WINDOW(dialog_),
                               gtk_get_current_event_time());

  if (page == CERT_MANAGER_PAGE_DEFAULT) {
    // Remember the last visited page from local state.
    page = static_cast<CertificateManagerPage>(last_selected_page_.GetValue());
    if (page == CERT_MANAGER_PAGE_DEFAULT)
      page = CERT_MANAGER_PAGE_USER;
  }
  // If the page number is out of bounds, reset to the first tab.
  if (page < 0 || page >= gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook_)))
    page = CERT_MANAGER_PAGE_USER;

  gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_), page);
}

}  // namespace

namespace certificate_manager_util {

void RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterIntegerPref(prefs::kCertificateManagerWindowLastTabIndex, 0);
}

}  // namespace certificate_manager_util

void ShowCertificateManager(gfx::NativeWindow parent, Profile* profile,
                            CertificateManagerPage page) {
  base::EnsureNSSInit();
  CertificateManager* manager = new CertificateManager(parent, profile);
  manager->ShowCertificatePage(page);
}
