// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ssl_client_certificate_selector_webui.h"

#include <vector>
#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl_client_certificate_selector.h"
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/browser/ui/webui/constrained_html_ui.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "chrome/common/url_constants.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/size.h"

namespace {

const int kDialogWidth = 460;
const int kDialogHeight = 382;

// Formats a certificate into details text.
std::string FormatDetailsText(
    net::X509Certificate::OSCertHandle cert) {
  std::string rv;

  rv += l10n_util::GetStringFUTF8(
      IDS_CERT_SUBJECTNAME_FORMAT,
      UTF8ToUTF16(x509_certificate_model::GetSubjectName(cert)));

  rv += "\n  ";
  rv += l10n_util::GetStringFUTF8(
      IDS_CERT_SERIAL_NUMBER_FORMAT,
      UTF8ToUTF16(
          x509_certificate_model::GetSerialNumberHexified(cert, "")));

  base::Time issued, expires;
  if (x509_certificate_model::GetTimes(cert, &issued, &expires)) {
    string16 issued_str = base::TimeFormatShortDateAndTime(issued);
    string16 expires_str = base::TimeFormatShortDateAndTime(expires);
    rv += "\n  ";
    rv += l10n_util::GetStringFUTF8(IDS_CERT_VALIDITY_RANGE_FORMAT,
                                    issued_str, expires_str);
  }

  std::vector<std::string> usages;
  x509_certificate_model::GetUsageStrings(cert, &usages);
  if (usages.size()) {
    rv += "\n  ";
    rv += l10n_util::GetStringFUTF8(IDS_CERT_X509_EXTENDED_KEY_USAGE_FORMAT,
                                    UTF8ToUTF16(JoinString(usages, ',')));
  }

  std::string key_usage_str = x509_certificate_model::GetKeyUsageString(cert);
  if (!key_usage_str.empty()) {
    rv += "\n  ";
    rv += l10n_util::GetStringFUTF8(IDS_CERT_X509_KEY_USAGE_FORMAT,
                                    UTF8ToUTF16(key_usage_str));
  }

  std::vector<std::string> email_addresses;
  x509_certificate_model::GetEmailAddresses(cert, &email_addresses);
  if (email_addresses.size()) {
    rv += "\n  ";
    rv += l10n_util::GetStringFUTF8(
        IDS_CERT_EMAIL_ADDRESSES_FORMAT,
        UTF8ToUTF16(JoinString(email_addresses, ',')));
  }

  rv += '\n';
  rv += l10n_util::GetStringFUTF8(
      IDS_CERT_ISSUERNAME_FORMAT,
      UTF8ToUTF16(x509_certificate_model::GetIssuerName(cert)));

  string16 token(UTF8ToUTF16(x509_certificate_model::GetTokenName(cert)));
  if (!token.empty()) {
    rv += '\n';
    rv += l10n_util::GetStringFUTF8(IDS_CERT_TOKEN_FORMAT, token);
  }

  return rv;
}

std::string FormatComboBoxText(
    net::X509Certificate::OSCertHandle cert, const std::string& nickname) {
  std::string rv(nickname);
  rv += " [";
  rv += x509_certificate_model::GetSerialNumberHexified(cert, "");
  rv += ']';
  return rv;
}
}

////////////////////////////////////////////////////////////////////////////////
// SSLClientCertificateSelectorWebUI

void SSLClientCertificateSelectorWebUI::ShowDialog(
    TabContentsWrapper* wrapper,
    net::SSLCertRequestInfo* cert_request_info,
    SSLClientAuthHandler* delegate) {
  SSLClientCertificateSelectorWebUI* ui =
      new SSLClientCertificateSelectorWebUI(wrapper,
                                            cert_request_info,
                                            delegate);
  Profile* profile = wrapper->profile();
  ConstrainedHtmlUI::CreateConstrainedHtmlDialog(profile, ui, wrapper);
}

SSLClientCertificateSelectorWebUI::SSLClientCertificateSelectorWebUI(
    TabContentsWrapper* wrapper,
    net::SSLCertRequestInfo* cert_request_info,
    SSLClientAuthHandler* delegate)
    : wrapper_(wrapper),
      cert_request_info_(cert_request_info),
      delegate_(delegate) {

  ChromeWebUIDataSource* source = new ChromeWebUIDataSource(
      chrome::kChromeUISSLClientCertificateSelectorHost);

  source->AddLocalizedString("siteLabel",
                             IDS_CERT_SELECTOR_SITE_DESCRIPTION_LABEL);
  source->AddLocalizedString("chooseLabel",
                             IDS_CERT_SELECTOR_CHOOSE_DESCRIPTION_LABEL);
  source->AddLocalizedString("detailsLabel",
                             IDS_CERT_SELECTOR_DETAILS_DESCRIPTION_LABEL);
  source->AddLocalizedString("info", IDS_PAGEINFO_CERT_INFO_BUTTON);
  source->AddLocalizedString("cancel", IDS_CANCEL);
  source->AddLocalizedString("ok", IDS_OK);

  // Set the json path.
  source->set_json_path("strings.js");

  // Add required resources.
  source->add_resource_path("ssl_client_certificate_selector.js",
                            IDR_SSL_CLIENT_CERTIFICATE_SELECTOR_JS);
  source->add_resource_path("ssl_client_certificate_selector.css",
                            IDR_SSL_CLIENT_CERTIFICATE_SELECTOR_CSS);

  // Set default resource.
  source->set_default_resource(IDR_SSL_CLIENT_CERTIFICATE_SELECTOR_HTML);

  wrapper->profile()->GetChromeURLDataManager()->AddDataSource(source);
}

SSLClientCertificateSelectorWebUI::~SSLClientCertificateSelectorWebUI() {
}

// HtmlDialogUIDelegate methods
bool SSLClientCertificateSelectorWebUI::IsDialogModal() const {
  return true;
}

string16 SSLClientCertificateSelectorWebUI::GetDialogTitle() const {
  return string16();
}

GURL SSLClientCertificateSelectorWebUI::GetDialogContentURL() const {
  return GURL(chrome::kChromeUISSLClientCertificateSelectorURL);
}

void SSLClientCertificateSelectorWebUI::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  handlers->push_back(const_cast<SSLClientCertificateSelectorWebUI*>(this));
}

void SSLClientCertificateSelectorWebUI::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDialogWidth, kDialogHeight);
}

std::string SSLClientCertificateSelectorWebUI::GetDialogArgs() const {
  return std::string();
}

void SSLClientCertificateSelectorWebUI::OnDialogClosed(
    const std::string& json_retval) {
  scoped_ptr<Value> parsed_value(base::JSONReader::Read(json_retval, false));
  ListValue* list = NULL;
  double value = 0;
  if (parsed_value.get() && parsed_value->GetAsList(&list) &&
      list->GetDouble(0, &value)) {
    // User selected a certificate.
    size_t index = (size_t) value;
    if (index < cert_request_info_->client_certs.size()) {
      scoped_refptr<net::X509Certificate> selected_cert(
          cert_request_info_->client_certs[index]);
      browser::UnlockCertSlotIfNecessary(
          selected_cert,
          browser::kCryptoModulePasswordClientAuth,
          cert_request_info_->host_and_port,
          base::Bind(&SSLClientCertificateSelectorWebUI::Unlocked,
                     delegate_,
                     selected_cert));
      return;
    }
  }

  // Anything else constitutes a cancel.
  delegate_->CertificateSelected(NULL);
  delegate_ = NULL;
}

// static
void SSLClientCertificateSelectorWebUI::Unlocked(SSLClientAuthHandler* delegate,
    net::X509Certificate* selected_cert) {
  delegate->CertificateSelected(selected_cert);
}


void SSLClientCertificateSelectorWebUI::OnCloseContents(TabContents* source,
    bool* out_close_dialog) {
  NOTIMPLEMENTED();
}

bool SSLClientCertificateSelectorWebUI::ShouldShowDialogTitle() const {
  return true;
}

// WebUIMessageHandler methods

void SSLClientCertificateSelectorWebUI::RegisterMessages() {
  web_ui_->RegisterMessageCallback("requestDetails",
      base::Bind(&SSLClientCertificateSelectorWebUI::RequestDetails,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("viewCertificate",
      base::Bind(&SSLClientCertificateSelectorWebUI::ViewCertificate,
                 base::Unretained(this)));
}

void SSLClientCertificateSelectorWebUI::RequestDetails(
    const base::ListValue* args) {
  std::vector<std::string> nicknames;
  x509_certificate_model::GetNicknameStringsFromCertList(
      cert_request_info_->client_certs,
      l10n_util::GetStringUTF8(IDS_CERT_SELECTOR_CERT_EXPIRED),
      l10n_util::GetStringUTF8(IDS_CERT_SELECTOR_CERT_NOT_YET_VALID),
      &nicknames);

  DCHECK_EQ(nicknames.size(), cert_request_info_->client_certs.size());


  DictionaryValue dict;
  dict.SetString("site", cert_request_info_->host_and_port.c_str());
  ListValue* certificates = new ListValue();
  ListValue* details = new ListValue();
  for (size_t i = 0; i < cert_request_info_->client_certs.size(); ++i) {
    net::X509Certificate::OSCertHandle cert =
        cert_request_info_->client_certs[i]->os_cert_handle();

    certificates->Append(new StringValue(
        FormatComboBoxText(cert, nicknames[i])));
    details->Append(new StringValue(
        FormatDetailsText(cert)));
  }
  dict.Set("certificates", certificates);
  dict.Set("details", details);
  // Send list of tab contents details to javascript.
  web_ui_->CallJavascriptFunction(
      "sslClientCertificateSelector.setDetails",
      dict);
}

void SSLClientCertificateSelectorWebUI::ViewCertificate(
    const base::ListValue* args) {
  double value = 0;
  if (args && args->GetDouble(0, &value)) {
    size_t index = (size_t) value;
    if (index < cert_request_info_->client_certs.size())
      ShowCertificateViewer(NULL, cert_request_info_->client_certs[index]);
  }
}

////////////////////////////////////////////////////////////////////////////////
// Platform API

namespace browser {

void ShowSSLClientCertificateSelector(
    TabContentsWrapper* wrapper,
    net::SSLCertRequestInfo* cert_request_info,
    SSLClientAuthHandler* delegate) {
#if defined(USE_AURA) || defined(OS_CHROMEOS)
  SSLClientCertificateSelectorWebUI::ShowDialog(wrapper,
                                                cert_request_info,
                                                delegate);
#else
  // TODO(rbyers): Remove the IsMoreWebUI check and (ideally) all #ifdefs onnce
  // we can select exactly one version of this dialog to use for each platform
  // at build time.  http://crbug.com/102775
  if (ChromeWebUI::IsMoreWebUI())
    SSLClientCertificateSelectorWebUI::ShowDialog(wrapper,
                                                  cert_request_info,
                                                  delegate);
  else
    ShowNativeSSLClientCertificateSelector(wrapper,
                                           cert_request_info,
                                           delegate);
#endif
}

}  // namespace browser
