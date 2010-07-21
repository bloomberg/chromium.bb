// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl_client_certificate_selector.h"

#include <cert.h>
#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "app/l10n_util.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/nss_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/ssl/ssl_client_auth_handler.h"
#include "chrome/third_party/mozilla_security_manager/nsNSSCertHelper.h"
#include "chrome/third_party/mozilla_security_manager/nsNSSCertificate.h"
#include "chrome/third_party/mozilla_security_manager/nsUsageArrayHelper.h"
#include "gfx/native_widget_types.h"
#include "grit/generated_resources.h"
#include "net/base/x509_certificate.h"

// PSM = Mozilla's Personal Security Manager.
namespace psm = mozilla_security_manager;

namespace {

enum {
  RESPONSE_SHOW_CERT_INFO = 1,
};


///////////////////////////////////////////////////////////////////////////////
// SSLClientCertificateSelector

class SSLClientCertificateSelector {
 public:
  SSLClientCertificateSelector(gfx::NativeWindow parent,
                               net::SSLCertRequestInfo* cert_request_info,
                               SSLClientAuthHandler* delegate);

  void Show();

 private:
  void PopulateCerts();

  static std::string FormatComboBoxText(CERTCertificate* cert,
                                        const char* nickname);
  static std::string FormatDetailsText(CERTCertificate* cert);

  static void OnComboBoxChanged(GtkComboBox* combo_box,
                                SSLClientCertificateSelector* cert_selector);
  static void OnResponse(GtkDialog* dialog, gint response_id,
                         SSLClientCertificateSelector* cert_selector);
  static void OnDestroy(GtkDialog* dialog,
                        SSLClientCertificateSelector* cert_selector);

  scoped_refptr<SSLClientAuthHandler> delegate_;
  scoped_refptr<net::SSLCertRequestInfo> cert_request_info_;

  std::vector<std::string> details_strings_;

  GtkWidget* dialog_;
  GtkWidget* cert_combo_box_;
  GtkTextBuffer* cert_details_buffer_;
};

SSLClientCertificateSelector::SSLClientCertificateSelector(
    gfx::NativeWindow parent,
    net::SSLCertRequestInfo* cert_request_info,
    SSLClientAuthHandler* delegate)
    : delegate_(delegate),
      cert_request_info_(cert_request_info) {
  dialog_ = gtk_dialog_new_with_buttons(
      l10n_util::GetStringFUTF8(
          IDS_CERT_SELECTOR_DIALOG_TITLE,
          UTF8ToUTF16(cert_request_info->host_and_port)).c_str(),
      parent,
      // Non-modal.
      GTK_DIALOG_NO_SEPARATOR,
      l10n_util::GetStringUTF8(IDS_PAGEINFO_CERT_INFO_BUTTON).c_str(),
      RESPONSE_SHOW_CERT_INFO,
      GTK_STOCK_CANCEL,
      GTK_RESPONSE_CANCEL,
      GTK_STOCK_OK,
      GTK_RESPONSE_OK,
      NULL);
  gtk_box_set_spacing(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                      gtk_util::kContentAreaSpacing);
  gtk_dialog_set_default_response(GTK_DIALOG(dialog_), GTK_RESPONSE_OK);

  GtkWidget* site_vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox), site_vbox,
                     FALSE, FALSE, 0);

  GtkWidget* site_description_label = gtk_util::CreateBoldLabel(
      l10n_util::GetStringUTF8(IDS_CERT_SELECTOR_SITE_DESCRIPTION_LABEL));
  gtk_box_pack_start(GTK_BOX(site_vbox), site_description_label,
                     FALSE, FALSE, 0);

  GtkWidget* site_label = gtk_label_new(
      cert_request_info->host_and_port.c_str());
  gtk_util::LeftAlignMisc(site_label);
  gtk_box_pack_start(GTK_BOX(site_vbox), site_label, FALSE, FALSE, 0);

  GtkWidget* selector_vbox = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox), selector_vbox,
                     TRUE, TRUE, 0);

  GtkWidget* choose_description_label = gtk_util::CreateBoldLabel(
      l10n_util::GetStringUTF8(IDS_CERT_SELECTOR_CHOOSE_DESCRIPTION_LABEL));
  gtk_box_pack_start(GTK_BOX(selector_vbox), choose_description_label,
                     FALSE, FALSE, 0);


  cert_combo_box_ = gtk_combo_box_new_text();
  g_signal_connect(cert_combo_box_, "changed", G_CALLBACK(OnComboBoxChanged),
                   this);
  gtk_box_pack_start(GTK_BOX(selector_vbox), cert_combo_box_,
                     FALSE, FALSE, 0);

  GtkWidget* details_label = gtk_label_new(l10n_util::GetStringUTF8(
      IDS_CERT_SELECTOR_DETAILS_DESCRIPTION_LABEL).c_str());
  gtk_util::LeftAlignMisc(details_label);
  gtk_box_pack_start(GTK_BOX(selector_vbox), details_label, FALSE, FALSE, 0);

  // TODO(mattm): fix text view coloring (should have grey background).
  GtkWidget* cert_details_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(cert_details_view), FALSE);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(cert_details_view), GTK_WRAP_WORD);
  cert_details_buffer_ = gtk_text_view_get_buffer(
      GTK_TEXT_VIEW(cert_details_view));
  // We put the details in a frame instead of a scrolled window so that the
  // entirety will be visible without requiring scrolling or expanding the
  // dialog.  This does however mean the dialog will grow itself if you switch
  // to different cert that has longer details text.
  GtkWidget* details_frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(details_frame), GTK_SHADOW_ETCHED_IN);
  gtk_container_add(GTK_CONTAINER(details_frame), cert_details_view);
  gtk_box_pack_start(GTK_BOX(selector_vbox), details_frame, TRUE, TRUE, 0);

  PopulateCerts();

  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponse), this);
  g_signal_connect(dialog_, "destroy", G_CALLBACK(OnDestroy), this);
}

void SSLClientCertificateSelector::Show() {
  gtk_widget_show_all(dialog_);
}

void SSLClientCertificateSelector::PopulateCerts() {
  CERTCertList* cert_list = CERT_NewCertList();
  for (size_t i = 0; i < cert_request_info_->client_certs.size(); ++i) {
    CERT_AddCertToListTail(
        cert_list,
        CERT_DupCertificate(
            cert_request_info_->client_certs[i]->os_cert_handle()));
  }
  // Would like to use CERT_GetCertNicknameWithValidity on each cert
  // individually instead of having to build a CERTCertList for this, but that
  // function is not exported.
  CERTCertNicknames* nicknames = CERT_NicknameStringsFromCertList(
      cert_list,
      const_cast<char*>(l10n_util::GetStringUTF8(
          IDS_CERT_SELECTOR_CERT_EXPIRED).c_str()),
      const_cast<char*>(l10n_util::GetStringUTF8(
          IDS_CERT_SELECTOR_CERT_NOT_YET_VALID).c_str()));
  DCHECK_EQ(nicknames->numnicknames,
            static_cast<int>(cert_request_info_->client_certs.size()));

  for (size_t i = 0; i < cert_request_info_->client_certs.size(); ++i) {
    CERTCertificate* cert =
        cert_request_info_->client_certs[i]->os_cert_handle();

    details_strings_.push_back(FormatDetailsText(cert));

    gtk_combo_box_append_text(
        GTK_COMBO_BOX(cert_combo_box_),
        FormatComboBoxText(cert, nicknames->nicknames[i]).c_str());
  }

  CERT_FreeNicknames(nicknames);
  CERT_DestroyCertList(cert_list);

  // Auto-select the first cert.
  gtk_combo_box_set_active(GTK_COMBO_BOX(cert_combo_box_), 0);
}

// static
std::string SSLClientCertificateSelector::FormatComboBoxText(
    CERTCertificate* cert, const char* nickname) {
  std::string rv(nickname);
  char* serial_hex = CERT_Hexify(&cert->serialNumber, TRUE);
  rv += " [";
  rv += serial_hex;
  rv += ']';
  PORT_Free(serial_hex);
  return rv;
}

// static
std::string SSLClientCertificateSelector::FormatDetailsText(
    CERTCertificate* cert) {
  std::string rv;

  rv += l10n_util::GetStringFUTF8(IDS_CERT_SUBJECTNAME_FORMAT,
                                  UTF8ToUTF16(cert->subjectName));

  char* serial_hex = CERT_Hexify(&cert->serialNumber, TRUE);
  rv += "\n  ";
  rv += l10n_util::GetStringFUTF8(IDS_CERT_SERIAL_NUMBER_FORMAT,
                                  UTF8ToUTF16(serial_hex));
  PORT_Free(serial_hex);

  PRTime issued, expires;
  if (CERT_GetCertTimes(cert, &issued, &expires) == SECSuccess) {
    string16 issued_str = WideToUTF16(
        base::TimeFormatShortDateAndTime(base::PRTimeToBaseTime(issued)));
    string16 expires_str = WideToUTF16(
        base::TimeFormatShortDateAndTime(base::PRTimeToBaseTime(expires)));
    rv += "\n  ";
    rv += l10n_util::GetStringFUTF8(IDS_CERT_VALIDITY_RANGE_FORMAT,
                                    issued_str, expires_str);
  }

  std::vector<std::string> usages;
  psm::GetCertUsageStrings(cert, &usages);
  if (usages.size()) {
    rv += "\n  ";
    rv += l10n_util::GetStringFUTF8(IDS_CERT_X509_EXTENDED_KEY_USAGE_FORMAT,
                                    UTF8ToUTF16(JoinString(usages, ',')));
  }

  SECItem key_usage;
  key_usage.data = NULL;
  if (CERT_FindKeyUsageExtension(cert, &key_usage) == SECSuccess) {
    std::string key_usage_str = psm::ProcessKeyUsageBitString(&key_usage, ',');
    PORT_Free(key_usage.data);
    if (!key_usage_str.empty()) {
      rv += "\n  ";
      rv += l10n_util::GetStringFUTF8(IDS_CERT_X509_KEY_USAGE_FORMAT,
                                      UTF8ToUTF16(key_usage_str));
    }
  }

  std::vector<std::string> email_addresses;
  for (const char* addr = CERT_GetFirstEmailAddress(cert);
       addr; addr = CERT_GetNextEmailAddress(cert, addr)) {
    // The first email addr (from Subject) may be duplicated in Subject
    // Alternative Name, so check subsequent addresses are not equal to the
    // first one before adding to the list.
    if (!email_addresses.size() || email_addresses[0] != addr)
      email_addresses.push_back(addr);
  }
  if (email_addresses.size()) {
    rv += "\n  ";
    rv += l10n_util::GetStringFUTF8(
        IDS_CERT_EMAIL_ADDRESSES_FORMAT,
        UTF8ToUTF16(JoinString(email_addresses, ',')));
  }

  rv += '\n';
  rv += l10n_util::GetStringFUTF8(IDS_CERT_ISSUERNAME_FORMAT,
                                  UTF8ToUTF16(cert->issuerName));

  string16 token(UTF8ToUTF16(psm::GetCertTokenName(cert)));
  if (!token.empty()) {
    rv += '\n';
    rv += l10n_util::GetStringFUTF8(IDS_CERT_TOKEN_FORMAT, token);
  }

  return rv;
}

// static
void SSLClientCertificateSelector::OnComboBoxChanged(
    GtkComboBox* combo_box, SSLClientCertificateSelector* cert_selector) {
  int selected = gtk_combo_box_get_active(
      GTK_COMBO_BOX(cert_selector->cert_combo_box_));
  if (selected < 0)
    return;
  gtk_text_buffer_set_text(cert_selector->cert_details_buffer_,
                           cert_selector->details_strings_[selected].c_str(),
                           cert_selector->details_strings_[selected].size());
}

// static
void SSLClientCertificateSelector::OnResponse(
    GtkDialog* dialog, gint response_id,
    SSLClientCertificateSelector* cert_selector) {
  net::X509Certificate* cert = NULL;
  if (response_id == GTK_RESPONSE_OK ||
      response_id == RESPONSE_SHOW_CERT_INFO) {
    int selected = gtk_combo_box_get_active(
        GTK_COMBO_BOX(cert_selector->cert_combo_box_));
    if (selected >= 0 &&
        selected < static_cast<int>(
            cert_selector->cert_request_info_->client_certs.size()))
      cert = cert_selector->cert_request_info_->client_certs[selected];
  }
  if (response_id == RESPONSE_SHOW_CERT_INFO) {
    if (cert)
      ShowCertificateViewer(GTK_WINDOW(cert_selector->dialog_), cert);
    return;
  }
  cert_selector->delegate_->CertificateSelected(cert);
  gtk_widget_destroy(GTK_WIDGET(dialog));
}

// static
void SSLClientCertificateSelector::OnDestroy(
    GtkDialog* dialog,
    SSLClientCertificateSelector* cert_selector) {
  delete cert_selector;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// SSLClientAuthHandler platform specific implementation:

namespace browser {

void ShowSSLClientCertificateSelector(
    gfx::NativeWindow parent,
    net::SSLCertRequestInfo* cert_request_info,
    SSLClientAuthHandler* delegate) {
  (new SSLClientCertificateSelector(parent,
                                    cert_request_info,
                                    delegate))->Show();
}

}  // namespace browser
