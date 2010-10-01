// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/certificate_manager_handler.h"

#include "app/l10n_util.h"
#include "app/l10n_util_collator.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/certificate_manager_model.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "grit/generated_resources.h"
#include "net/base/x509_certificate.h"

namespace {

static const char kKeyId[] = "id";
static const char kSubNodesId[] = "subnodes";
static const char kNameId[] = "name";
static const char kIconId[] = "icon";
static const char kSecurityDeviceId[] = "device";

// TODO(mattm): These are duplicated from cookies_view_handler.cc
// Encodes a pointer value into a hex string.
std::string PointerToHexString(const void* pointer) {
  return base::HexEncode(&pointer, sizeof(pointer));
}

// Decodes a pointer from a hex string.
void* HexStringToPointer(const std::string& str) {
  std::vector<uint8> buffer;
  if (!base::HexStringToBytes(str, &buffer) ||
      buffer.size() != sizeof(void*)) {
    return NULL;
  }

  return *reinterpret_cast<void**>(&buffer[0]);
}

std::string OrgNameToId(const std::string& org) {
  return "org-" + org;
}

std::string CertToId(const net::X509Certificate& cert) {
  return "cert-" + PointerToHexString(&cert);
}

net::X509Certificate* IdToCert(const std::string& id) {
  if (!StartsWithASCII(id, "cert-", true))
    return NULL;
  return reinterpret_cast<net::X509Certificate*>(HexStringToPointer(id.substr(5)));
}

struct DictionaryIdComparator {
  DictionaryIdComparator(icu::Collator* collator) : collator_(collator) {
  }

  bool operator()(const Value* a,
                  const Value* b) const {
    DCHECK(a->GetType() == Value::TYPE_DICTIONARY);
    DCHECK(b->GetType() == Value::TYPE_DICTIONARY);
    const DictionaryValue* a_dict = reinterpret_cast<const DictionaryValue*>(a);
    const DictionaryValue* b_dict = reinterpret_cast<const DictionaryValue*>(b);
    string16 a_str;
    string16 b_str;
    a_dict->GetString(kNameId, &a_str);
    b_dict->GetString(kNameId, &b_str);
    if (collator_ == NULL)
      return a_str < b_str;
    return l10n_util::CompareString16WithCollator(
        collator_, a_str, b_str) == UCOL_LESS;
  }

  icu::Collator* collator_;
};

}  // namespace

CertificateManagerHandler::CertificateManagerHandler() {
  certificate_manager_model_.reset(new CertificateManagerModel);
}

CertificateManagerHandler::~CertificateManagerHandler() {
}

void CertificateManagerHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("certificateManagerPage",
      l10n_util::GetStringUTF16(IDS_CERTIFICATE_MANAGER_TITLE));

  // Tabs.
  localized_strings->SetString("personalCertsTabTitle",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_PERSONAL_CERTS_TAB_LABEL));
  localized_strings->SetString("emailCertsTabTitle",
      l10n_util::GetStringUTF16(
          IDS_CERT_MANAGER_OTHER_PEOPLES_CERTS_TAB_LABEL));
  localized_strings->SetString("serverCertsTabTitle",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_SERVER_CERTS_TAB_LABEL));
  localized_strings->SetString("caCertsTabTitle",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_CERT_AUTHORITIES_TAB_LABEL));
  localized_strings->SetString("unknownCertsTabTitle",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_UNKNOWN_TAB_LABEL));

  // Tab descriptions.
  localized_strings->SetString("personalCertsTabDescription",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_USER_TREE_DESCRIPTION));
  localized_strings->SetString("emailCertsTabDescription",
      l10n_util::GetStringUTF16(
          IDS_CERT_MANAGER_OTHER_PEOPLE_TREE_DESCRIPTION));
  localized_strings->SetString("serverCertsTabDescription",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_SERVER_TREE_DESCRIPTION));
  localized_strings->SetString("caCertsTabDescription",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_AUTHORITIES_TREE_DESCRIPTION));
  localized_strings->SetString("unknownCertsTabDescription",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_UNKNOWN_TREE_DESCRIPTION));

  // Tree columns.
  localized_strings->SetString("certNameColumn",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_NAME_COLUMN_LABEL));
  localized_strings->SetString("certDeviceColumn",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DEVICE_COLUMN_LABEL));
  localized_strings->SetString("certSerialColumn",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_SERIAL_NUMBER_COLUMN_LABEL));
  localized_strings->SetString("certExpiresColumn",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EXPIRES_COLUMN_LABEL));
  localized_strings->SetString("certEmailColumn",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EMAIL_ADDRESS_COLUMN_LABEL));

  // Buttons.
  localized_strings->SetString("view_certificate",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_VIEW_CERT_BUTTON));
}

void CertificateManagerHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback("viewCertificate",
      NewCallback(this, &CertificateManagerHandler::View));

  dom_ui_->RegisterMessageCallback("populateCertificateManager",
      NewCallback(this, &CertificateManagerHandler::Populate));
}

void CertificateManagerHandler::View(const ListValue* args) {
  std::string node_id;
  if (!args->GetString(0, &node_id)){
    return;
  }
  net::X509Certificate* cert = IdToCert(node_id);
  if (!cert) {
    NOTREACHED();
    return;
  }
  ShowCertificateViewer(
      dom_ui_->tab_contents()->view()->GetTopLevelNativeWindow(), cert);
}

void CertificateManagerHandler::Populate(const ListValue* args) {
  certificate_manager_model_->Refresh();

  PopulateTree("personalCertsTab", net::USER_CERT);
  PopulateTree("emailCertsTab", net::EMAIL_CERT);
  PopulateTree("serverCertsTab", net::SERVER_CERT);
  PopulateTree("caCertsTab", net::CA_CERT);
  PopulateTree("otherCertsTab", net::UNKNOWN_CERT);
}

void CertificateManagerHandler::PopulateTree(const std::string& tab_name,
                                             net::CertType type) {
  const std::string tree_name = tab_name + "-tree";

  scoped_ptr<icu::Collator> collator;
  UErrorCode error = U_ZERO_ERROR;
  collator.reset(
      icu::Collator::createInstance(
          icu::Locale(g_browser_process->GetApplicationLocale().c_str()),
          error));
  if (U_FAILURE(error))
    collator.reset(NULL);
  DictionaryIdComparator comparator(collator.get());
  CertificateManagerModel::OrgGroupingMap map;

  certificate_manager_model_->FilterAndBuildOrgGroupingMap(type, &map);

  {
    ListValue* nodes = new ListValue;
    for (CertificateManagerModel::OrgGroupingMap::iterator i = map.begin();
         i != map.end(); ++i) {
      // Populate first level (org name).
      DictionaryValue* dict = new DictionaryValue;
      dict->SetString(kKeyId, OrgNameToId(i->first));
      dict->SetString(kNameId, i->first);

      // Populate second level (certs).
      ListValue* subnodes = new ListValue;
      for (net::CertificateList::const_iterator org_cert_it = i->second.begin();
           org_cert_it != i->second.end(); ++org_cert_it) {
        DictionaryValue* cert_dict = new DictionaryValue;
        net::X509Certificate* cert = org_cert_it->get();
        cert_dict->SetString(kKeyId, CertToId(*cert));
        cert_dict->SetString(kNameId, certificate_manager_model_->GetColumnText(
            *cert, CertificateManagerModel::COL_SUBJECT_NAME));
        // TODO(mattm): Other columns.
        // TODO(mattm): Get a real icon (or figure out how to make the domui
        // tree not use icons at all).
        cert_dict->SetString(kIconId, "chrome://theme/IDR_COOKIE_ICON");
        subnodes->Append(cert_dict);
      }
      std::sort(subnodes->begin(), subnodes->end(), comparator);

      dict->Set(kSubNodesId, subnodes);
      nodes->Append(dict);
    }
    std::sort(nodes->begin(), nodes->end(), comparator);

    ListValue args;
    args.Append(Value::CreateStringValue(tree_name));
    args.Append(nodes);
    dom_ui_->CallJavascriptFunction(L"CertificateManager.onPopulateTree", args);
  }
}
