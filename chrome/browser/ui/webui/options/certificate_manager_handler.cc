// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/certificate_manager_handler.h"

#include <algorithm>
#include <map>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"  // for FileAccessProvider
#include "base/id_map.h"
#include "base/memory/scoped_vector.h"
#include "base/safe_strerror_posix.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/ui/certificate_dialogs.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "net/base/crypto_module.h"
#include "net/base/net_errors.h"
#include "net/base/x509_certificate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#endif

using content::BrowserThread;

namespace {

static const char kKeyId[] = "id";
static const char kSubNodesId[] = "subnodes";
static const char kNameId[] = "name";
static const char kReadOnlyId[] = "readonly";
static const char kUntrustedId[] = "untrusted";
static const char kExtractableId[] = "extractable";
static const char kSecurityDeviceId[] = "device";
static const char kErrorId[] = "error";

// Enumeration of different callers of SelectFile.  (Start counting at 1 so
// if SelectFile is accidentally called with params=NULL it won't match any.)
enum {
  EXPORT_PERSONAL_FILE_SELECTED = 1,
  IMPORT_PERSONAL_FILE_SELECTED,
  IMPORT_SERVER_FILE_SELECTED,
  IMPORT_CA_FILE_SELECTED,
};

std::string OrgNameToId(const std::string& org) {
  return "org-" + org;
}

bool CallbackArgsToBool(const ListValue* args, int index, bool* result) {
  std::string string_value;
  if (!args->GetString(index, &string_value))
    return false;

  *result = string_value[0] == 't';
  return true;
}

struct DictionaryIdComparator {
  explicit DictionaryIdComparator(icu::Collator* collator)
      : collator_(collator) {
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

std::string NetErrorToString(int net_error) {
  switch (net_error) {
    // TODO(mattm): handle more cases.
    case net::ERR_IMPORT_CA_CERT_NOT_CA:
      return l10n_util::GetStringUTF8(IDS_CERT_MANAGER_ERROR_NOT_CA);
    case net::ERR_IMPORT_CERT_ALREADY_EXISTS:
      return l10n_util::GetStringUTF8(
          IDS_CERT_MANAGER_ERROR_CERT_ALREADY_EXISTS);
    default:
      return l10n_util::GetStringUTF8(IDS_CERT_MANAGER_UNKNOWN_ERROR);
  }
}

}  // namespace

namespace options {

///////////////////////////////////////////////////////////////////////////////
//  CertIdMap

class CertIdMap {
 public:
  CertIdMap() {}
  ~CertIdMap() {}

  std::string CertToId(net::X509Certificate* cert);
  net::X509Certificate* IdToCert(const std::string& id);
  net::X509Certificate* CallbackArgsToCert(const base::ListValue* args);

 private:
  typedef std::map<net::X509Certificate*, int32> CertMap;

  // Creates an ID for cert and looks up the cert for an ID.
  IDMap<net::X509Certificate>id_map_;

  // Finds the ID for a cert.
  CertMap cert_map_;

  DISALLOW_COPY_AND_ASSIGN(CertIdMap);
};

std::string CertIdMap::CertToId(net::X509Certificate* cert) {
  CertMap::const_iterator iter = cert_map_.find(cert);
  if (iter != cert_map_.end())
    return base::IntToString(iter->second);

  int32 new_id = id_map_.Add(cert);
  cert_map_[cert] = new_id;
  return base::IntToString(new_id);
}

net::X509Certificate* CertIdMap::IdToCert(const std::string& id) {
  int32 cert_id = 0;
  if (!base::StringToInt(id, &cert_id))
    return NULL;

  return id_map_.Lookup(cert_id);
}

net::X509Certificate* CertIdMap::CallbackArgsToCert(
    const ListValue* args) {
  std::string node_id;
  if (!args->GetString(0, &node_id))
    return NULL;

  net::X509Certificate* cert = IdToCert(node_id);
  if (!cert) {
    NOTREACHED();
    return NULL;
  }

  return cert;
}

///////////////////////////////////////////////////////////////////////////////
//  FileAccessProvider

// TODO(mattm): Move to some shared location?
class FileAccessProvider
    : public base::RefCountedThreadSafe<FileAccessProvider> {
 public:
  // The first parameter is 0 on success or errno on failure. The second
  // parameter is read result.
  typedef base::Callback<void(const int*, const std::string*)> ReadCallback;

  // The first parameter is 0 on success or errno on failure. The second
  // parameter is the number of bytes written on success.
  typedef base::Callback<void(const int*, const int*)> WriteCallback;

  CancelableTaskTracker::TaskId StartRead(const base::FilePath& path,
                                          const ReadCallback& callback,
                                          CancelableTaskTracker* tracker);
  CancelableTaskTracker::TaskId StartWrite(const base::FilePath& path,
                                           const std::string& data,
                                           const WriteCallback& callback,
                                           CancelableTaskTracker* tracker);

 private:
  friend class base::RefCountedThreadSafe<FileAccessProvider>;
  virtual ~FileAccessProvider() {}

  // Reads file at |path|. |saved_errno| is 0 on success or errno on failure.
  // When success, |data| has file content.
  void DoRead(const base::FilePath& path,
              int* saved_errno,
              std::string* data);
  // Writes data to file at |path|. |saved_errno| is 0 on success or errno on
  // failure. When success, |bytes_written| has number of bytes written.
  void DoWrite(const base::FilePath& path,
               const std::string& data,
               int* saved_errno,
               int* bytes_written);
};

CancelableTaskTracker::TaskId FileAccessProvider::StartRead(
    const base::FilePath& path,
    const ReadCallback& callback,
    CancelableTaskTracker* tracker) {
  // Owned by reply callback posted below.
  int* saved_errno = new int(0);
  std::string* data = new std::string();

  // Post task to file thread to read file.
  return tracker->PostTaskAndReply(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      FROM_HERE,
      base::Bind(&FileAccessProvider::DoRead, this, path, saved_errno, data),
      base::Bind(callback, base::Owned(saved_errno), base::Owned(data)));
}

CancelableTaskTracker::TaskId FileAccessProvider::StartWrite(
    const base::FilePath& path,
    const std::string& data,
    const WriteCallback& callback,
    CancelableTaskTracker* tracker) {
  // Owned by reply callback posted below.
  int* saved_errno = new int(0);
  int* bytes_written = new int(0);

  // Post task to file thread to write file.
  return tracker->PostTaskAndReply(
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
      FROM_HERE,
      base::Bind(&FileAccessProvider::DoWrite, this,
                 path, data, saved_errno, bytes_written),
      base::Bind(callback,
                 base::Owned(saved_errno), base::Owned(bytes_written)));
}

void FileAccessProvider::DoRead(const base::FilePath& path,
                                int* saved_errno,
                                std::string* data) {
  bool success = file_util::ReadFileToString(path, data);
  *saved_errno = success ? 0 : errno;
}

void FileAccessProvider::DoWrite(const base::FilePath& path,
                                 const std::string& data,
                                 int* saved_errno,
                                 int* bytes_written) {
  *bytes_written = file_util::WriteFile(path, data.data(), data.size());
  *saved_errno = bytes_written >= 0 ? 0 : errno;
}

///////////////////////////////////////////////////////////////////////////////
//  CertificateManagerHandler

CertificateManagerHandler::CertificateManagerHandler()
    : use_hardware_backed_(false),
      file_access_provider_(new FileAccessProvider()),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      cert_id_map_(new CertIdMap) {
  certificate_manager_model_.reset(new CertificateManagerModel(this));
}

CertificateManagerHandler::~CertificateManagerHandler() {
}

void CertificateManagerHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "certificateManagerPage",
                IDS_CERTIFICATE_MANAGER_TITLE);

  // Tabs.
  localized_strings->SetString("personalCertsTabTitle",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_PERSONAL_CERTS_TAB_LABEL));
  localized_strings->SetString("serverCertsTabTitle",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_SERVER_CERTS_TAB_LABEL));
  localized_strings->SetString("caCertsTabTitle",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_CERT_AUTHORITIES_TAB_LABEL));
  localized_strings->SetString("unknownCertsTabTitle",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_UNKNOWN_TAB_LABEL));

  // Tab descriptions.
  localized_strings->SetString("personalCertsTabDescription",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_USER_TREE_DESCRIPTION));
  localized_strings->SetString("serverCertsTabDescription",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_SERVER_TREE_DESCRIPTION));
  localized_strings->SetString("caCertsTabDescription",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_AUTHORITIES_TREE_DESCRIPTION));
  localized_strings->SetString("unknownCertsTabDescription",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_UNKNOWN_TREE_DESCRIPTION));

  // Buttons.
  localized_strings->SetString("view_certificate",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_VIEW_CERT_BUTTON));
  localized_strings->SetString("import_certificate",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_IMPORT_BUTTON));
  localized_strings->SetString("export_certificate",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EXPORT_BUTTON));
  localized_strings->SetString("edit_certificate",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EDIT_BUTTON));
  localized_strings->SetString("delete_certificate",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_BUTTON));

  // Certificate Delete overlay strings.
  localized_strings->SetString("personalCertsTabDeleteConfirm",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_USER_FORMAT));
  localized_strings->SetString("personalCertsTabDeleteImpact",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_USER_DESCRIPTION));
  localized_strings->SetString("serverCertsTabDeleteConfirm",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_SERVER_FORMAT));
  localized_strings->SetString("serverCertsTabDeleteImpact",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_SERVER_DESCRIPTION));
  localized_strings->SetString("caCertsTabDeleteConfirm",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_CA_FORMAT));
  localized_strings->SetString("caCertsTabDeleteImpact",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_CA_DESCRIPTION));
  localized_strings->SetString("unknownCertsTabDeleteConfirm",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_DELETE_UNKNOWN_FORMAT));
  localized_strings->SetString("unknownCertsTabDeleteImpact", "");

  // Certificate Restore overlay strings.
  localized_strings->SetString("certificateRestorePasswordDescription",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_RESTORE_PASSWORD_DESC));
  localized_strings->SetString("certificatePasswordLabel",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_PASSWORD_LABEL));

  // Personal Certificate Export overlay strings.
  localized_strings->SetString("certificateExportPasswordDescription",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EXPORT_PASSWORD_DESC));
  localized_strings->SetString("certificateExportPasswordHelp",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EXPORT_PASSWORD_HELP));
  localized_strings->SetString("certificateConfirmPasswordLabel",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_CONFIRM_PASSWORD_LABEL));

  // Edit CA Trust & Import CA overlay strings.
  localized_strings->SetString("certificateEditCaTitle",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EDIT_CA_TITLE));
  localized_strings->SetString("certificateEditTrustLabel",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EDIT_TRUST_LABEL));
  localized_strings->SetString("certificateEditCaTrustDescriptionFormat",
      l10n_util::GetStringUTF16(
          IDS_CERT_MANAGER_EDIT_CA_TRUST_DESCRIPTION_FORMAT));
  localized_strings->SetString("certificateImportCaDescriptionFormat",
      l10n_util::GetStringUTF16(
          IDS_CERT_MANAGER_IMPORT_CA_DESCRIPTION_FORMAT));
  localized_strings->SetString("certificateCaTrustSSLLabel",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EDIT_CA_TRUST_SSL_LABEL));
  localized_strings->SetString("certificateCaTrustEmailLabel",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EDIT_CA_TRUST_EMAIL_LABEL));
  localized_strings->SetString("certificateCaTrustObjSignLabel",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_EDIT_CA_TRUST_OBJSIGN_LABEL));
  localized_strings->SetString("certificateImportErrorFormat",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_IMPORT_ERROR_FORMAT));

  // Badges next to certificates
  localized_strings->SetString("badgeCertUntrusted",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_UNTRUSTED));

#if defined(OS_CHROMEOS)
  localized_strings->SetString("importAndBindCertificate",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_IMPORT_AND_BIND_BUTTON));
  localized_strings->SetString("hardwareBackedKeyFormat",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_HARDWARE_BACKED_KEY_FORMAT));
  localized_strings->SetString("chromeOSDeviceName",
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_HARDWARE_BACKED));
#endif  // defined(OS_CHROMEOS)
}

void CertificateManagerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "viewCertificate",
      base::Bind(&CertificateManagerHandler::View, base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getCaCertificateTrust",
      base::Bind(&CertificateManagerHandler::GetCATrust,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "editCaCertificateTrust",
      base::Bind(&CertificateManagerHandler::EditCATrust,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "editServerCertificate",
      base::Bind(&CertificateManagerHandler::EditServer,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "cancelImportExportCertificate",
      base::Bind(&CertificateManagerHandler::CancelImportExportProcess,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "exportPersonalCertificate",
      base::Bind(&CertificateManagerHandler::ExportPersonal,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "exportAllPersonalCertificates",
      base::Bind(&CertificateManagerHandler::ExportAllPersonal,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "exportPersonalCertificatePasswordSelected",
      base::Bind(&CertificateManagerHandler::ExportPersonalPasswordSelected,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "importPersonalCertificate",
      base::Bind(&CertificateManagerHandler::StartImportPersonal,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "importPersonalCertificatePasswordSelected",
      base::Bind(&CertificateManagerHandler::ImportPersonalPasswordSelected,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "importCaCertificate",
      base::Bind(&CertificateManagerHandler::ImportCA,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "importCaCertificateTrustSelected",
      base::Bind(&CertificateManagerHandler::ImportCATrustSelected,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "importServerCertificate",
      base::Bind(&CertificateManagerHandler::ImportServer,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "exportCertificate",
      base::Bind(&CertificateManagerHandler::Export,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "deleteCertificate",
      base::Bind(&CertificateManagerHandler::Delete,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "populateCertificateManager",
      base::Bind(&CertificateManagerHandler::Populate,
                 base::Unretained(this)));

#if defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "checkTpmTokenReady",
      base::Bind(&CertificateManagerHandler::CheckTpmTokenReady,
                 base::Unretained(this)));
#endif
}

void CertificateManagerHandler::CertificatesRefreshed() {
  PopulateTree("personalCertsTab", net::USER_CERT);
  PopulateTree("serverCertsTab", net::SERVER_CERT);
  PopulateTree("caCertsTab", net::CA_CERT);
  PopulateTree("otherCertsTab", net::UNKNOWN_CERT);
  VLOG(1) << "populating finished";
}

void CertificateManagerHandler::FileSelected(const base::FilePath& path,
                                             int index,
                                             void* params) {
  switch (reinterpret_cast<intptr_t>(params)) {
    case EXPORT_PERSONAL_FILE_SELECTED:
      ExportPersonalFileSelected(path);
      break;
    case IMPORT_PERSONAL_FILE_SELECTED:
      ImportPersonalFileSelected(path);
      break;
    case IMPORT_SERVER_FILE_SELECTED:
      ImportServerFileSelected(path);
      break;
    case IMPORT_CA_FILE_SELECTED:
      ImportCAFileSelected(path);
      break;
    default:
      NOTREACHED();
  }
}

void CertificateManagerHandler::FileSelectionCanceled(void* params) {
  switch (reinterpret_cast<intptr_t>(params)) {
    case EXPORT_PERSONAL_FILE_SELECTED:
    case IMPORT_PERSONAL_FILE_SELECTED:
    case IMPORT_SERVER_FILE_SELECTED:
    case IMPORT_CA_FILE_SELECTED:
      ImportExportCleanup();
      break;
    default:
      NOTREACHED();
  }
}

void CertificateManagerHandler::View(const ListValue* args) {
  net::X509Certificate* cert = cert_id_map_->CallbackArgsToCert(args);
  if (!cert)
    return;
  ShowCertificateViewer(web_ui()->GetWebContents(), GetParentWindow(), cert);
}

void CertificateManagerHandler::GetCATrust(const ListValue* args) {
  net::X509Certificate* cert = cert_id_map_->CallbackArgsToCert(args);
  if (!cert) {
    web_ui()->CallJavascriptFunction("CertificateEditCaTrustOverlay.dismiss");
    return;
  }

  net::NSSCertDatabase::TrustBits trust_bits =
      certificate_manager_model_->cert_db()->GetCertTrust(cert, net::CA_CERT);
  base::FundamentalValue ssl_value(
      static_cast<bool>(trust_bits & net::NSSCertDatabase::TRUSTED_SSL));
  base::FundamentalValue email_value(
      static_cast<bool>(trust_bits & net::NSSCertDatabase::TRUSTED_EMAIL));
  base::FundamentalValue obj_sign_value(
      static_cast<bool>(trust_bits & net::NSSCertDatabase::TRUSTED_OBJ_SIGN));
  web_ui()->CallJavascriptFunction(
      "CertificateEditCaTrustOverlay.populateTrust",
      ssl_value, email_value, obj_sign_value);
}

void CertificateManagerHandler::EditCATrust(const ListValue* args) {
  net::X509Certificate* cert = cert_id_map_->CallbackArgsToCert(args);
  bool fail = !cert;
  bool trust_ssl = false;
  bool trust_email = false;
  bool trust_obj_sign = false;
  fail |= !CallbackArgsToBool(args, 1, &trust_ssl);
  fail |= !CallbackArgsToBool(args, 2, &trust_email);
  fail |= !CallbackArgsToBool(args, 3, &trust_obj_sign);
  if (fail) {
    LOG(ERROR) << "EditCATrust args fail";
    web_ui()->CallJavascriptFunction("CertificateEditCaTrustOverlay.dismiss");
    return;
  }

  bool result = certificate_manager_model_->SetCertTrust(
      cert,
      net::CA_CERT,
      trust_ssl * net::NSSCertDatabase::TRUSTED_SSL +
          trust_email * net::NSSCertDatabase::TRUSTED_EMAIL +
          trust_obj_sign * net::NSSCertDatabase::TRUSTED_OBJ_SIGN);
  web_ui()->CallJavascriptFunction("CertificateEditCaTrustOverlay.dismiss");
  if (!result) {
    // TODO(mattm): better error messages?
    ShowError(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_SET_TRUST_ERROR_TITLE),
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_UNKNOWN_ERROR));
  }
}

void CertificateManagerHandler::EditServer(const ListValue* args) {
  NOTIMPLEMENTED();
}

void CertificateManagerHandler::ExportPersonal(const ListValue* args) {
  net::X509Certificate* cert = cert_id_map_->CallbackArgsToCert(args);
  if (!cert)
    return;

  selected_cert_list_.push_back(cert);

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("p12"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_PKCS12_FILES));
  file_type_info.include_all_files = true;
  // TODO(kinaba): http://crbug.com/140425. Turn file_type_info.support_drive
  // on once Google Drive client on ChromeOS fully supports file writing.
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, string16(),
      base::FilePath(), &file_type_info, 1, FILE_PATH_LITERAL("p12"),
      GetParentWindow(),
      reinterpret_cast<void*>(EXPORT_PERSONAL_FILE_SELECTED));
}

void CertificateManagerHandler::ExportAllPersonal(const ListValue* args) {
  NOTIMPLEMENTED();
}

void CertificateManagerHandler::ExportPersonalFileSelected(
    const base::FilePath& path) {
  file_path_ = path;
  web_ui()->CallJavascriptFunction(
      "CertificateManager.exportPersonalAskPassword");
}

void CertificateManagerHandler::ExportPersonalPasswordSelected(
    const ListValue* args) {
  if (!args->GetString(0, &password_)) {
    web_ui()->CallJavascriptFunction("CertificateRestoreOverlay.dismiss");
    ImportExportCleanup();
    return;
  }

  // Currently, we don't support exporting more than one at a time.  If we do,
  // this would need to either change this to use UnlockSlotsIfNecessary or
  // change UnlockCertSlotIfNecessary to take a CertificateList.
  DCHECK_EQ(selected_cert_list_.size(), 1U);

  // TODO(mattm): do something smarter about non-extractable keys
  chrome::UnlockCertSlotIfNecessary(
      selected_cert_list_[0].get(),
      chrome::kCryptoModulePasswordCertExport,
      "",  // unused.
      base::Bind(&CertificateManagerHandler::ExportPersonalSlotsUnlocked,
                 base::Unretained(this)));
}

void CertificateManagerHandler::ExportPersonalSlotsUnlocked() {
  std::string output;
  int num_exported = certificate_manager_model_->cert_db()->ExportToPKCS12(
      selected_cert_list_,
      password_,
      &output);
  if (!num_exported) {
    web_ui()->CallJavascriptFunction("CertificateRestoreOverlay.dismiss");
    ShowError(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_PKCS12_EXPORT_ERROR_TITLE),
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_UNKNOWN_ERROR));
    ImportExportCleanup();
    return;
  }
  file_access_provider_->StartWrite(
      file_path_,
      output,
      base::Bind(&CertificateManagerHandler::ExportPersonalFileWritten,
                 base::Unretained(this)),
      &tracker_);
}

void CertificateManagerHandler::ExportPersonalFileWritten(
    const int* write_errno, const int* bytes_written) {
  web_ui()->CallJavascriptFunction("CertificateRestoreOverlay.dismiss");
  ImportExportCleanup();
  if (*write_errno) {
    ShowError(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_PKCS12_EXPORT_ERROR_TITLE),
        l10n_util::GetStringFUTF8(IDS_CERT_MANAGER_WRITE_ERROR_FORMAT,
                                  UTF8ToUTF16(safe_strerror(*write_errno))));
  }
}

void CertificateManagerHandler::StartImportPersonal(const ListValue* args) {
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  if (!args->GetBoolean(0, &use_hardware_backed_)) {
    // Unable to retrieve the hardware backed attribute from the args,
    // so bail.
    web_ui()->CallJavascriptFunction("CertificateRestoreOverlay.dismiss");
    ImportExportCleanup();
    return;
  }
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("p12"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_CERT_MANAGER_PKCS12_FILES));
  file_type_info.include_all_files = true;
  file_type_info.support_drive = true;
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, string16(),
      base::FilePath(), &file_type_info, 1, FILE_PATH_LITERAL("p12"),
      GetParentWindow(),
      reinterpret_cast<void*>(IMPORT_PERSONAL_FILE_SELECTED));
}

void CertificateManagerHandler::ImportPersonalFileSelected(
    const base::FilePath& path) {
  file_path_ = path;
  web_ui()->CallJavascriptFunction(
      "CertificateManager.importPersonalAskPassword");
}

void CertificateManagerHandler::ImportPersonalPasswordSelected(
    const ListValue* args) {
  if (!args->GetString(0, &password_)) {
    web_ui()->CallJavascriptFunction("CertificateRestoreOverlay.dismiss");
    ImportExportCleanup();
    return;
  }
  file_access_provider_->StartRead(
      file_path_,
      base::Bind(&CertificateManagerHandler::ImportPersonalFileRead,
                 base::Unretained(this)),
      &tracker_);
}

void CertificateManagerHandler::ImportPersonalFileRead(
    const int* read_errno, const std::string* data) {
  if (*read_errno) {
    ImportExportCleanup();
    web_ui()->CallJavascriptFunction("CertificateRestoreOverlay.dismiss");
    ShowError(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_PKCS12_IMPORT_ERROR_TITLE),
        l10n_util::GetStringFUTF8(IDS_CERT_MANAGER_READ_ERROR_FORMAT,
                                  UTF8ToUTF16(safe_strerror(*read_errno))));
    return;
  }

  file_data_ = *data;

  if (use_hardware_backed_) {
    module_ = certificate_manager_model_->cert_db()->GetPrivateModule();
  } else {
    module_ = certificate_manager_model_->cert_db()->GetPublicModule();
  }

  net::CryptoModuleList modules;
  modules.push_back(module_);
  chrome::UnlockSlotsIfNecessary(
      modules,
      chrome::kCryptoModulePasswordCertImport,
      "",  // unused.
      base::Bind(&CertificateManagerHandler::ImportPersonalSlotUnlocked,
                 base::Unretained(this)));
}

void CertificateManagerHandler::ImportPersonalSlotUnlocked() {
  // Determine if the private key should be unextractable after the import.
  // We do this by checking the value of |use_hardware_backed_| which is set
  // to true if importing into a hardware module. Currently, this only happens
  // for Chrome OS when the "Import and Bind" option is chosen.
  bool is_extractable = !use_hardware_backed_;
  int result = certificate_manager_model_->ImportFromPKCS12(
      module_, file_data_, password_, is_extractable);
  ImportExportCleanup();
  web_ui()->CallJavascriptFunction("CertificateRestoreOverlay.dismiss");
  int string_id;
  switch (result) {
    case net::OK:
      return;
    case net::ERR_PKCS12_IMPORT_BAD_PASSWORD:
      // TODO(mattm): if the error was a bad password, we should reshow the
      // password dialog after the user dismisses the error dialog.
      string_id = IDS_CERT_MANAGER_BAD_PASSWORD;
      break;
    case net::ERR_PKCS12_IMPORT_INVALID_MAC:
      string_id = IDS_CERT_MANAGER_PKCS12_IMPORT_INVALID_MAC;
      break;
    case net::ERR_PKCS12_IMPORT_INVALID_FILE:
      string_id = IDS_CERT_MANAGER_PKCS12_IMPORT_INVALID_FILE;
      break;
    case net::ERR_PKCS12_IMPORT_UNSUPPORTED:
      string_id = IDS_CERT_MANAGER_PKCS12_IMPORT_UNSUPPORTED;
      break;
    default:
      string_id = IDS_CERT_MANAGER_UNKNOWN_ERROR;
      break;
  }
  ShowError(
      l10n_util::GetStringUTF8(IDS_CERT_MANAGER_PKCS12_IMPORT_ERROR_TITLE),
      l10n_util::GetStringUTF8(string_id));
}

void CertificateManagerHandler::CancelImportExportProcess(
    const ListValue* args) {
  ImportExportCleanup();
}

void CertificateManagerHandler::ImportExportCleanup() {
  file_path_.clear();
  password_.clear();
  file_data_.clear();
  use_hardware_backed_ = false;
  selected_cert_list_.clear();
  module_ = NULL;

  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
  select_file_dialog_ = NULL;
}

void CertificateManagerHandler::ImportServer(const ListValue* args) {
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  ShowCertSelectFileDialog(
      select_file_dialog_.get(),
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      base::FilePath(),
      GetParentWindow(),
      reinterpret_cast<void*>(IMPORT_SERVER_FILE_SELECTED));
}

void CertificateManagerHandler::ImportServerFileSelected(
    const base::FilePath& path) {
  file_path_ = path;
  file_access_provider_->StartRead(
      file_path_,
      base::Bind(&CertificateManagerHandler::ImportServerFileRead,
                 base::Unretained(this)),
      &tracker_);
}

void CertificateManagerHandler::ImportServerFileRead(const int* read_errno,
                                                     const std::string* data) {
  if (*read_errno) {
    ImportExportCleanup();
    ShowError(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_SERVER_IMPORT_ERROR_TITLE),
        l10n_util::GetStringFUTF8(IDS_CERT_MANAGER_READ_ERROR_FORMAT,
                                  UTF8ToUTF16(safe_strerror(*read_errno))));
    return;
  }

  selected_cert_list_ = net::X509Certificate::CreateCertificateListFromBytes(
          data->data(), data->size(), net::X509Certificate::FORMAT_AUTO);
  if (selected_cert_list_.empty()) {
    ImportExportCleanup();
    ShowError(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_SERVER_IMPORT_ERROR_TITLE),
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_CERT_PARSE_ERROR));
    return;
  }

  net::NSSCertDatabase::ImportCertFailureList not_imported;
  // TODO(mattm): Add UI for trust. http://crbug.com/76274
  bool result = certificate_manager_model_->ImportServerCert(
      selected_cert_list_,
      net::NSSCertDatabase::TRUST_DEFAULT,
      &not_imported);
  if (!result) {
    ShowError(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_SERVER_IMPORT_ERROR_TITLE),
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_UNKNOWN_ERROR));
  } else if (!not_imported.empty()) {
    ShowImportErrors(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_SERVER_IMPORT_ERROR_TITLE),
        not_imported);
  }
  ImportExportCleanup();
}

void CertificateManagerHandler::ImportCA(const ListValue* args) {
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  ShowCertSelectFileDialog(select_file_dialog_.get(),
                           ui::SelectFileDialog::SELECT_OPEN_FILE,
                           base::FilePath(),
                           GetParentWindow(),
                           reinterpret_cast<void*>(IMPORT_CA_FILE_SELECTED));
}

void CertificateManagerHandler::ImportCAFileSelected(
    const base::FilePath& path) {
  file_path_ = path;
  file_access_provider_->StartRead(
      file_path_,
      base::Bind(&CertificateManagerHandler::ImportCAFileRead,
                 base::Unretained(this)),
      &tracker_);
}

void CertificateManagerHandler::ImportCAFileRead(const int* read_errno,
                                                const std::string* data) {
  if (*read_errno) {
    ImportExportCleanup();
    ShowError(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_CA_IMPORT_ERROR_TITLE),
        l10n_util::GetStringFUTF8(IDS_CERT_MANAGER_READ_ERROR_FORMAT,
                                  UTF8ToUTF16(safe_strerror(*read_errno))));
    return;
  }

  selected_cert_list_ = net::X509Certificate::CreateCertificateListFromBytes(
          data->data(), data->size(), net::X509Certificate::FORMAT_AUTO);
  if (selected_cert_list_.empty()) {
    ImportExportCleanup();
    ShowError(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_CA_IMPORT_ERROR_TITLE),
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_CERT_PARSE_ERROR));
    return;
  }

  scoped_refptr<net::X509Certificate> root_cert =
      certificate_manager_model_->cert_db()->FindRootInList(
          selected_cert_list_);

  // TODO(mattm): check here if root_cert is not a CA cert and show error.

  StringValue cert_name(root_cert->subject().GetDisplayName());
  web_ui()->CallJavascriptFunction("CertificateEditCaTrustOverlay.showImport",
                                   cert_name);
}

void CertificateManagerHandler::ImportCATrustSelected(const ListValue* args) {
  bool fail = false;
  bool trust_ssl = false;
  bool trust_email = false;
  bool trust_obj_sign = false;
  fail |= !CallbackArgsToBool(args, 0, &trust_ssl);
  fail |= !CallbackArgsToBool(args, 1, &trust_email);
  fail |= !CallbackArgsToBool(args, 2, &trust_obj_sign);
  if (fail) {
    LOG(ERROR) << "ImportCATrustSelected args fail";
    ImportExportCleanup();
    web_ui()->CallJavascriptFunction("CertificateEditCaTrustOverlay.dismiss");
    return;
  }

  // TODO(mattm): add UI for setting explicit distrust, too.
  // http://crbug.com/128411
  net::NSSCertDatabase::ImportCertFailureList not_imported;
  bool result = certificate_manager_model_->ImportCACerts(
      selected_cert_list_,
      trust_ssl * net::NSSCertDatabase::TRUSTED_SSL +
          trust_email * net::NSSCertDatabase::TRUSTED_EMAIL +
          trust_obj_sign * net::NSSCertDatabase::TRUSTED_OBJ_SIGN,
      &not_imported);
  web_ui()->CallJavascriptFunction("CertificateEditCaTrustOverlay.dismiss");
  if (!result) {
    ShowError(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_CA_IMPORT_ERROR_TITLE),
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_UNKNOWN_ERROR));
  } else if (!not_imported.empty()) {
    ShowImportErrors(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_CA_IMPORT_ERROR_TITLE),
        not_imported);
  }
  ImportExportCleanup();
}

void CertificateManagerHandler::Export(const ListValue* args) {
  net::X509Certificate* cert = cert_id_map_->CallbackArgsToCert(args);
  if (!cert)
    return;
  ShowCertExportDialog(web_ui()->GetWebContents(), GetParentWindow(),
                       cert->os_cert_handle());
}

void CertificateManagerHandler::Delete(const ListValue* args) {
  net::X509Certificate* cert = cert_id_map_->CallbackArgsToCert(args);
  if (!cert)
    return;
  bool result = certificate_manager_model_->Delete(cert);
  if (!result) {
    // TODO(mattm): better error messages?
    ShowError(
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_DELETE_CERT_ERROR_TITLE),
        l10n_util::GetStringUTF8(IDS_CERT_MANAGER_UNKNOWN_ERROR));
  }
}

void CertificateManagerHandler::Populate(const ListValue* args) {
  certificate_manager_model_->Refresh();
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
        cert_dict->SetString(kKeyId, cert_id_map_->CertToId(cert));
        cert_dict->SetString(kNameId, certificate_manager_model_->GetColumnText(
            *cert, CertificateManagerModel::COL_SUBJECT_NAME));
        cert_dict->SetBoolean(
            kReadOnlyId,
            certificate_manager_model_->cert_db()->IsReadOnly(cert));
        cert_dict->SetBoolean(
            kUntrustedId,
            certificate_manager_model_->cert_db()->IsUntrusted(cert));
        // TODO(hshi): This should be determined by testing for PKCS #11
        // CKA_EXTRACTABLE attribute. We may need to use the NSS function
        // PK11_ReadRawAttribute to do that.
        cert_dict->SetBoolean(
            kExtractableId,
            !certificate_manager_model_->IsHardwareBacked(cert));
        // TODO(mattm): Other columns.
        subnodes->Append(cert_dict);
      }
      std::sort(subnodes->begin(), subnodes->end(), comparator);

      dict->Set(kSubNodesId, subnodes);
      nodes->Append(dict);
    }
    std::sort(nodes->begin(), nodes->end(), comparator);

    ListValue args;
    args.Append(new base::StringValue(tree_name));
    args.Append(nodes);
    web_ui()->CallJavascriptFunction("CertificateManager.onPopulateTree", args);
  }
}

void CertificateManagerHandler::ShowError(const std::string& title,
                                          const std::string& error) const {
  ScopedVector<const Value> args;
  args.push_back(new base::StringValue(title));
  args.push_back(new base::StringValue(error));
  args.push_back(new base::StringValue(l10n_util::GetStringUTF8(IDS_OK)));
  args.push_back(Value::CreateNullValue());  // cancelTitle
  args.push_back(Value::CreateNullValue());  // okCallback
  args.push_back(Value::CreateNullValue());  // cancelCallback
  web_ui()->CallJavascriptFunction("AlertOverlay.show", args.get());
}

void CertificateManagerHandler::ShowImportErrors(
    const std::string& title,
    const net::NSSCertDatabase::ImportCertFailureList& not_imported) const {
  std::string error;
  if (selected_cert_list_.size() == 1)
    error = l10n_util::GetStringUTF8(
        IDS_CERT_MANAGER_IMPORT_SINGLE_NOT_IMPORTED);
  else if (not_imported.size() == selected_cert_list_.size())
    error = l10n_util::GetStringUTF8(IDS_CERT_MANAGER_IMPORT_ALL_NOT_IMPORTED);
  else
    error = l10n_util::GetStringUTF8(IDS_CERT_MANAGER_IMPORT_SOME_NOT_IMPORTED);

  ListValue cert_error_list;
  for (size_t i = 0; i < not_imported.size(); ++i) {
    const net::NSSCertDatabase::ImportCertFailure& failure = not_imported[i];
    DictionaryValue* dict = new DictionaryValue;
    dict->SetString(kNameId, failure.certificate->subject().GetDisplayName());
    dict->SetString(kErrorId, NetErrorToString(failure.net_error));
    cert_error_list.Append(dict);
  }

  StringValue title_value(title);
  StringValue error_value(error);
  web_ui()->CallJavascriptFunction("CertificateImportErrorOverlay.show",
                                   title_value,
                                   error_value,
                                   cert_error_list);
}

#if defined(OS_CHROMEOS)
void CertificateManagerHandler::CheckTpmTokenReady(const ListValue* args) {
  chromeos::CryptohomeClient* cryptohome_client =
      chromeos::DBusThreadManager::Get()->GetCryptohomeClient();
  cryptohome_client->Pkcs11IsTpmTokenReady(
      base::Bind(&CertificateManagerHandler::CheckTpmTokenReadyInternal,
                 weak_ptr_factory_.GetWeakPtr()));
}

void CertificateManagerHandler::CheckTpmTokenReadyInternal(
    chromeos::DBusMethodCallStatus call_status,
    bool is_tpm_token_ready) {
  base::FundamentalValue ready(
      call_status == chromeos::DBUS_METHOD_CALL_SUCCESS && is_tpm_token_ready);
  web_ui()->CallJavascriptFunction("CertificateManager.onCheckTpmTokenReady",
                                   ready);
}
#endif

gfx::NativeWindow CertificateManagerHandler::GetParentWindow() const {
  return web_ui()->GetWebContents()->GetView()->GetTopLevelNativeWindow();
}

}  // namespace options
