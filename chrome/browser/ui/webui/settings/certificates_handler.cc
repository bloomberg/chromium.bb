// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/certificates_handler.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"  // for FileAccessProvider
#include "base/i18n/string_compare.h"
#include "base/id_map.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/posix/safe_strerror.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/certificate_dialogs.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/crypto_module_password_dialog_nss.h"
#include "chrome/browser/ui/webui/certificate_viewer_webui.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/der/input.h"
#include "net/der/parser.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/policy/user_network_configuration_updater.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater_factory.h"
#endif

using base::UTF8ToUTF16;
using content::BrowserThread;

namespace {

// Field names for communicating certificate info to JS.
static const char kEmailField[] = "email";
static const char kExtractableField[] = "extractable";
static const char kKeyField[] = "id";
static const char kNameField[] = "name";
static const char kObjSignField[] = "objSign";
static const char kPolicyField[] = "policy";
static const char kReadonlyField[] = "readonly";
static const char kSslField[] = "ssl";
static const char kSubnodesField[] = "subnodes";
static const char kUntrustedField[] = "untrusted";

// Field names for communicating erros to JS.
static const char kCertificateErrors[] = "certificateErrors";
static const char kErrorDescription[] = "description";
static const char kErrorField[] = "error";
static const char kErrorTitle[] = "title";

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

struct DictionaryIdComparator {
  explicit DictionaryIdComparator(icu::Collator* collator)
      : collator_(collator) {}

  bool operator()(const base::Value& a, const base::Value& b) const {
    DCHECK(a.GetType() == base::Value::Type::DICTIONARY);
    DCHECK(b.GetType() == base::Value::Type::DICTIONARY);
    const base::DictionaryValue* a_dict;
    bool a_is_dictionary = a.GetAsDictionary(&a_dict);
    DCHECK(a_is_dictionary);
    const base::DictionaryValue* b_dict;
    bool b_is_dictionary = b.GetAsDictionary(&b_dict);
    DCHECK(b_is_dictionary);
    base::string16 a_str;
    base::string16 b_str;
    a_dict->GetString(kNameField, &a_str);
    b_dict->GetString(kNameField, &b_str);
    if (collator_ == NULL)
      return a_str < b_str;
    return base::i18n::CompareString16WithCollator(*collator_, a_str, b_str) ==
           UCOL_LESS;
  }

  icu::Collator* collator_;
};

std::string NetErrorToString(int net_error) {
  switch (net_error) {
    // TODO(mattm): handle more cases.
    case net::ERR_IMPORT_CA_CERT_NOT_CA:
      return l10n_util::GetStringUTF8(
          IDS_SETTINGS_CERTIFICATE_MANAGER_ERROR_NOT_CA);
    case net::ERR_IMPORT_CERT_ALREADY_EXISTS:
      return l10n_util::GetStringUTF8(
          IDS_SETTINGS_CERTIFICATE_MANAGER_ERROR_CERT_ALREADY_EXISTS);
    default:
      return l10n_util::GetStringUTF8(
          IDS_SETTINGS_CERTIFICATE_MANAGER_UNKNOWN_ERROR);
  }
}

// Struct to bind the Equals member function to an object for use in find_if.
struct CertEquals {
  explicit CertEquals(const net::X509Certificate* cert) : cert_(cert) {}
  bool operator()(const scoped_refptr<net::X509Certificate> cert) const {
    return cert_->Equals(cert.get());
  }
  const net::X509Certificate* cert_;
};

// Determine whether a certificate was stored with web trust by a policy.
bool IsPolicyInstalledWithWebTrust(const net::CertificateList& web_trust_certs,
                                   net::X509Certificate* cert) {
  return std::find_if(web_trust_certs.begin(), web_trust_certs.end(),
                      CertEquals(cert)) != web_trust_certs.end();
}

#if defined(OS_CHROMEOS)
void ShowCertificateViewerModalDialog(content::WebContents* web_contents,
                                      gfx::NativeWindow parent,
                                      net::X509Certificate* cert) {
  CertificateViewerModalDialog* dialog = new CertificateViewerModalDialog(cert);
  dialog->Show(web_contents, parent);
}
#endif

// Determine if |data| could be a PFX Protocol Data Unit.
// This only does the minimum parsing necessary to distinguish a PFX file from a
// DER encoded Certificate.
//
// From RFC 7292 section 4:
//   PFX ::= SEQUENCE {
//       version     INTEGER {v3(3)}(v3,...),
//       authSafe    ContentInfo,
//       macData     MacData OPTIONAL
//   }
// From RFC 5280 section 4.1:
//   Certificate  ::=  SEQUENCE  {
//       tbsCertificate       TBSCertificate,
//       signatureAlgorithm   AlgorithmIdentifier,
//       signatureValue       BIT STRING  }
//
//  Certificate must be DER encoded, while PFX may be BER encoded.
//  Therefore PFX can be distingushed by checking if the file starts with an
//  indefinite SEQUENCE, or a definite SEQUENCE { INTEGER,  ... }.
bool CouldBePFX(const std::string& data) {
  if (data.size() < 4)
    return false;

  // Indefinite length SEQUENCE.
  if (data[0] == 0x30 && static_cast<uint8_t>(data[1]) == 0x80)
    return true;

  // If the SEQUENCE is definite length, it can be parsed through the version
  // tag using DER parser, since INTEGER must be definite length, even in BER.
  net::der::Parser parser((net::der::Input(&data)));
  net::der::Parser sequence_parser;
  if (!parser.ReadSequence(&sequence_parser))
    return false;
  if (!sequence_parser.SkipTag(net::der::kInteger))
    return false;
  return true;
}

}  // namespace

namespace settings {

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
  typedef std::map<net::X509Certificate*, int32_t> CertMap;

  // Creates an ID for cert and looks up the cert for an ID.
  IDMap<net::X509Certificate*> id_map_;

  // Finds the ID for a cert.
  CertMap cert_map_;

  DISALLOW_COPY_AND_ASSIGN(CertIdMap);
};

std::string CertIdMap::CertToId(net::X509Certificate* cert) {
  CertMap::const_iterator iter = cert_map_.find(cert);
  if (iter != cert_map_.end())
    return base::IntToString(iter->second);

  int32_t new_id = id_map_.Add(cert);
  cert_map_[cert] = new_id;
  return base::IntToString(new_id);
}

net::X509Certificate* CertIdMap::IdToCert(const std::string& id) {
  int32_t cert_id = 0;
  if (!base::StringToInt(id, &cert_id))
    return NULL;

  return id_map_.Lookup(cert_id);
}

net::X509Certificate* CertIdMap::CallbackArgsToCert(
    const base::ListValue* args) {
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

  base::CancelableTaskTracker::TaskId StartRead(
      const base::FilePath& path,
      const ReadCallback& callback,
      base::CancelableTaskTracker* tracker);
  base::CancelableTaskTracker::TaskId StartWrite(
      const base::FilePath& path,
      const std::string& data,
      const WriteCallback& callback,
      base::CancelableTaskTracker* tracker);

 private:
  friend class base::RefCountedThreadSafe<FileAccessProvider>;
  virtual ~FileAccessProvider() {}

  // Reads file at |path|. |saved_errno| is 0 on success or errno on failure.
  // When success, |data| has file content.
  void DoRead(const base::FilePath& path, int* saved_errno, std::string* data);
  // Writes data to file at |path|. |saved_errno| is 0 on success or errno on
  // failure. When success, |bytes_written| has number of bytes written.
  void DoWrite(const base::FilePath& path,
               const std::string& data,
               int* saved_errno,
               int* bytes_written);
};

base::CancelableTaskTracker::TaskId FileAccessProvider::StartRead(
    const base::FilePath& path,
    const ReadCallback& callback,
    base::CancelableTaskTracker* tracker) {
  // Owned by reply callback posted below.
  int* saved_errno = new int(0);
  std::string* data = new std::string();

  // Post task to file thread to read file.
  return tracker->PostTaskAndReply(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE).get(),
      FROM_HERE,
      base::BindOnce(&FileAccessProvider::DoRead, this, path, saved_errno,
                     data),
      base::BindOnce(callback, base::Owned(saved_errno), base::Owned(data)));
}

base::CancelableTaskTracker::TaskId FileAccessProvider::StartWrite(
    const base::FilePath& path,
    const std::string& data,
    const WriteCallback& callback,
    base::CancelableTaskTracker* tracker) {
  // Owned by reply callback posted below.
  int* saved_errno = new int(0);
  int* bytes_written = new int(0);

  // Post task to file thread to write file.
  return tracker->PostTaskAndReply(
      BrowserThread::GetTaskRunnerForThread(BrowserThread::FILE).get(),
      FROM_HERE,
      base::BindOnce(&FileAccessProvider::DoWrite, this, path, data,
                     saved_errno, bytes_written),
      base::BindOnce(callback, base::Owned(saved_errno),
                     base::Owned(bytes_written)));
}

void FileAccessProvider::DoRead(const base::FilePath& path,
                                int* saved_errno,
                                std::string* data) {
  bool success = base::ReadFileToString(path, data);
  *saved_errno = success ? 0 : errno;
}

void FileAccessProvider::DoWrite(const base::FilePath& path,
                                 const std::string& data,
                                 int* saved_errno,
                                 int* bytes_written) {
  *bytes_written = base::WriteFile(path, data.data(), data.size());
  *saved_errno = *bytes_written >= 0 ? 0 : errno;
}

///////////////////////////////////////////////////////////////////////////////
//  CertificatesHandler

CertificatesHandler::CertificatesHandler(bool show_certs_in_modal_dialog)
    : show_certs_in_modal_dialog_(show_certs_in_modal_dialog),
      requested_certificate_manager_model_(false),
      use_hardware_backed_(false),
      file_access_provider_(new FileAccessProvider()),
      cert_id_map_(new CertIdMap),
      weak_ptr_factory_(this) {}

CertificatesHandler::~CertificatesHandler() {}

void CertificatesHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "viewCertificate", base::Bind(&CertificatesHandler::HandleViewCertificate,
                                    base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "getCaCertificateTrust",
      base::Bind(&CertificatesHandler::HandleGetCATrust,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "editCaCertificateTrust",
      base::Bind(&CertificatesHandler::HandleEditCATrust,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "cancelImportExportCertificate",
      base::Bind(&CertificatesHandler::HandleCancelImportExportProcess,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "exportPersonalCertificate",
      base::Bind(&CertificatesHandler::HandleExportPersonal,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "exportPersonalCertificatePasswordSelected",
      base::Bind(&CertificatesHandler::HandleExportPersonalPasswordSelected,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "importPersonalCertificate",
      base::Bind(&CertificatesHandler::HandleImportPersonal,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "importPersonalCertificatePasswordSelected",
      base::Bind(&CertificatesHandler::HandleImportPersonalPasswordSelected,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "importCaCertificate",
      base::Bind(&CertificatesHandler::HandleImportCA, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "importCaCertificateTrustSelected",
      base::Bind(&CertificatesHandler::HandleImportCATrustSelected,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "importServerCertificate",
      base::Bind(&CertificatesHandler::HandleImportServer,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "exportCertificate",
      base::Bind(&CertificatesHandler::HandleExportCertificate,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "deleteCertificate",
      base::Bind(&CertificatesHandler::HandleDeleteCertificate,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "refreshCertificates",
      base::Bind(&CertificatesHandler::HandleRefreshCertificates,
                 base::Unretained(this)));
}

void CertificatesHandler::CertificatesRefreshed() {
  net::CertificateList web_trusted_certs;
#if defined(OS_CHROMEOS)
  policy::UserNetworkConfigurationUpdater* service =
      policy::UserNetworkConfigurationUpdaterFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));
  if (service)
    service->GetWebTrustedCertificates(&web_trusted_certs);
#endif
  PopulateTree("personalCerts", net::USER_CERT, web_trusted_certs);
  PopulateTree("serverCerts", net::SERVER_CERT, web_trusted_certs);
  PopulateTree("caCerts", net::CA_CERT, web_trusted_certs);
  PopulateTree("otherCerts", net::OTHER_CERT, web_trusted_certs);
}

void CertificatesHandler::FileSelected(const base::FilePath& path,
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

void CertificatesHandler::FileSelectionCanceled(void* params) {
  switch (reinterpret_cast<intptr_t>(params)) {
    case EXPORT_PERSONAL_FILE_SELECTED:
    case IMPORT_PERSONAL_FILE_SELECTED:
    case IMPORT_SERVER_FILE_SELECTED:
    case IMPORT_CA_FILE_SELECTED:
      ImportExportCleanup();
      RejectCallback(base::Value());
      break;
    default:
      NOTREACHED();
  }
}

void CertificatesHandler::HandleViewCertificate(const base::ListValue* args) {
  net::X509Certificate* cert = cert_id_map_->CallbackArgsToCert(args);
  if (!cert)
    return;
#if defined(OS_CHROMEOS)
  if (show_certs_in_modal_dialog_) {
    ShowCertificateViewerModalDialog(web_ui()->GetWebContents(),
                                     GetParentWindow(), cert);
    return;
  }
#endif
  ShowCertificateViewer(web_ui()->GetWebContents(), GetParentWindow(), cert);
}

void CertificatesHandler::AssignWebUICallbackId(const base::ListValue* args) {
  CHECK_LE(1U, args->GetSize());
  CHECK(webui_callback_id_.empty());
  CHECK(args->GetString(0, &webui_callback_id_));
}

void CertificatesHandler::HandleGetCATrust(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(2U, args->GetSize());
  AssignWebUICallbackId(args);
  std::string node_id;
  CHECK(args->GetString(1, &node_id));

  net::X509Certificate* cert = cert_id_map_->IdToCert(node_id);
  CHECK(cert);

  net::NSSCertDatabase::TrustBits trust_bits =
      certificate_manager_model_->cert_db()->GetCertTrust(cert, net::CA_CERT);
  std::unique_ptr<base::DictionaryValue> ca_trust_info(
      new base::DictionaryValue);
  ca_trust_info->SetBoolean(
      kSslField,
      static_cast<bool>(trust_bits & net::NSSCertDatabase::TRUSTED_SSL));
  ca_trust_info->SetBoolean(
      kEmailField,
      static_cast<bool>(trust_bits & net::NSSCertDatabase::TRUSTED_EMAIL));
  ca_trust_info->SetBoolean(
      kObjSignField,
      static_cast<bool>(trust_bits & net::NSSCertDatabase::TRUSTED_OBJ_SIGN));
  ResolveCallback(*ca_trust_info);
}

void CertificatesHandler::HandleEditCATrust(const base::ListValue* args) {
  CHECK_EQ(5U, args->GetSize());
  AssignWebUICallbackId(args);
  std::string node_id;
  CHECK(args->GetString(1, &node_id));

  net::X509Certificate* cert = cert_id_map_->IdToCert(node_id);
  CHECK(cert);

  bool trust_ssl = false;
  bool trust_email = false;
  bool trust_obj_sign = false;
  CHECK(args->GetBoolean(2, &trust_ssl));
  CHECK(args->GetBoolean(3, &trust_email));
  CHECK(args->GetBoolean(4, &trust_obj_sign));

  bool result = certificate_manager_model_->SetCertTrust(
      cert, net::CA_CERT,
      trust_ssl * net::NSSCertDatabase::TRUSTED_SSL +
          trust_email * net::NSSCertDatabase::TRUSTED_EMAIL +
          trust_obj_sign * net::NSSCertDatabase::TRUSTED_OBJ_SIGN);
  if (!result) {
    // TODO(mattm): better error messages?
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_SET_TRUST_ERROR_TITLE),
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_UNKNOWN_ERROR));
  } else {
    ResolveCallback(base::Value());
  }
}

void CertificatesHandler::HandleExportPersonal(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  AssignWebUICallbackId(args);
  std::string node_id;
  CHECK(args->GetString(1, &node_id));

  net::X509Certificate* cert = cert_id_map_->IdToCert(node_id);
  CHECK(cert);
  selected_cert_list_.push_back(cert);

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("p12"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_SETTINGS_CERTIFICATE_MANAGER_PKCS12_FILES));
  file_type_info.include_all_files = true;
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, base::string16(),
      base::FilePath(), &file_type_info, 1, FILE_PATH_LITERAL("p12"),
      GetParentWindow(),
      reinterpret_cast<void*>(EXPORT_PERSONAL_FILE_SELECTED));
}

void CertificatesHandler::ExportPersonalFileSelected(
    const base::FilePath& path) {
  file_path_ = path;
  ResolveCallback(base::Value());
}

void CertificatesHandler::HandleExportPersonalPasswordSelected(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  AssignWebUICallbackId(args);
  CHECK(args->GetString(1, &password_));

  // Currently, we don't support exporting more than one at a time.  If we do,
  // this would need to either change this to use UnlockSlotsIfNecessary or
  // change UnlockCertSlotIfNecessary to take a CertificateList.
  DCHECK_EQ(selected_cert_list_.size(), 1U);

  // TODO(mattm): do something smarter about non-extractable keys
  chrome::UnlockCertSlotIfNecessary(
      selected_cert_list_[0].get(), chrome::kCryptoModulePasswordCertExport,
      net::HostPortPair(),  // unused.
      GetParentWindow(),
      base::Bind(&CertificatesHandler::ExportPersonalSlotsUnlocked,
                 base::Unretained(this)));
}

void CertificatesHandler::ExportPersonalSlotsUnlocked() {
  std::string output;
  int num_exported = certificate_manager_model_->cert_db()->ExportToPKCS12(
      selected_cert_list_, password_, &output);
  if (!num_exported) {
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_PKCS12_EXPORT_ERROR_TITLE),
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_UNKNOWN_ERROR));
    ImportExportCleanup();
    return;
  }
  file_access_provider_->StartWrite(
      file_path_, output,
      base::Bind(&CertificatesHandler::ExportPersonalFileWritten,
                 base::Unretained(this)),
      &tracker_);
}

void CertificatesHandler::ExportPersonalFileWritten(const int* write_errno,
                                                    const int* bytes_written) {
  ImportExportCleanup();
  if (*write_errno) {
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_PKCS12_EXPORT_ERROR_TITLE),
        l10n_util::GetStringFUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_WRITE_ERROR_FORMAT,
            UTF8ToUTF16(base::safe_strerror(*write_errno))));
  } else {
    ResolveCallback(base::Value());
  }
}

void CertificatesHandler::HandleImportPersonal(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  AssignWebUICallbackId(args);
  CHECK(args->GetBoolean(1, &use_hardware_backed_));

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(1);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("p12"));
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("pfx"));
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("crt"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(
          IDS_SETTINGS_CERTIFICATE_MANAGER_USAGE_SSL_CLIENT));
  file_type_info.include_all_files = true;
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_OPEN_FILE, base::string16(),
      base::FilePath(), &file_type_info, 1, FILE_PATH_LITERAL("p12"),
      GetParentWindow(),
      reinterpret_cast<void*>(IMPORT_PERSONAL_FILE_SELECTED));
}

void CertificatesHandler::ImportPersonalFileSelected(
    const base::FilePath& path) {
  file_access_provider_->StartRead(
      path, base::Bind(&CertificatesHandler::ImportPersonalFileRead,
                       base::Unretained(this)),
      &tracker_);
}

void CertificatesHandler::ImportPersonalFileRead(const int* read_errno,
                                                 const std::string* data) {
  if (*read_errno) {
    ImportExportCleanup();
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_ERROR_TITLE),
        l10n_util::GetStringFUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_READ_ERROR_FORMAT,
            UTF8ToUTF16(base::safe_strerror(*read_errno))));
    return;
  }

  file_data_ = *data;

  if (CouldBePFX(file_data_)) {
    ResolveCallback(base::Value(true));
    return;
  }

  // Non .p12/.pfx files are assumed to be single/chain certificates without
  // private key data. The default extension according to spec is '.crt',
  // however other extensions are also used in some places to represent these
  // certificates.
  int result = certificate_manager_model_->ImportUserCert(file_data_);
  ImportExportCleanup();
  int string_id;
  switch (result) {
    case net::OK:
      ResolveCallback(base::Value(false));
      return;
    case net::ERR_NO_PRIVATE_KEY_FOR_CERT:
      string_id = IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_MISSING_KEY;
      break;
    case net::ERR_CERT_INVALID:
      string_id = IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_INVALID_FILE;
      break;
    default:
      string_id = IDS_SETTINGS_CERTIFICATE_MANAGER_UNKNOWN_ERROR;
      break;
  }
  RejectCallbackWithError(
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_ERROR_TITLE),
      l10n_util::GetStringUTF8(string_id));
}

void CertificatesHandler::HandleImportPersonalPasswordSelected(
    const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  AssignWebUICallbackId(args);
  CHECK(args->GetString(1, &password_));

  if (use_hardware_backed_) {
    slot_ = certificate_manager_model_->cert_db()->GetPrivateSlot();
  } else {
    slot_ = certificate_manager_model_->cert_db()->GetPublicSlot();
  }

  std::vector<crypto::ScopedPK11Slot> modules;
  modules.push_back(crypto::ScopedPK11Slot(PK11_ReferenceSlot(slot_.get())));
  chrome::UnlockSlotsIfNecessary(
      std::move(modules), chrome::kCryptoModulePasswordCertImport,
      net::HostPortPair(),  // unused.
      GetParentWindow(),
      base::Bind(&CertificatesHandler::ImportPersonalSlotUnlocked,
                 base::Unretained(this)));
}

void CertificatesHandler::ImportPersonalSlotUnlocked() {
  // Determine if the private key should be unextractable after the import.
  // We do this by checking the value of |use_hardware_backed_| which is set
  // to true if importing into a hardware module. Currently, this only happens
  // for Chrome OS when the "Import and Bind" option is chosen.
  bool is_extractable = !use_hardware_backed_;
  int result = certificate_manager_model_->ImportFromPKCS12(
      slot_.get(), file_data_, password_, is_extractable);
  ImportExportCleanup();
  int string_id;
  switch (result) {
    case net::OK:
      ResolveCallback(base::Value());
      return;
    case net::ERR_PKCS12_IMPORT_BAD_PASSWORD:
      // TODO(mattm): if the error was a bad password, we should reshow the
      // password dialog after the user dismisses the error dialog.
      string_id = IDS_SETTINGS_CERTIFICATE_MANAGER_BAD_PASSWORD;
      break;
    case net::ERR_PKCS12_IMPORT_INVALID_MAC:
      string_id = IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_INVALID_MAC;
      break;
    case net::ERR_PKCS12_IMPORT_INVALID_FILE:
      string_id = IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_INVALID_FILE;
      break;
    case net::ERR_PKCS12_IMPORT_UNSUPPORTED:
      string_id = IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_UNSUPPORTED;
      break;
    default:
      string_id = IDS_SETTINGS_CERTIFICATE_MANAGER_UNKNOWN_ERROR;
      break;
  }
  RejectCallbackWithError(
      l10n_util::GetStringUTF8(
          IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_ERROR_TITLE),
      l10n_util::GetStringUTF8(string_id));
}

void CertificatesHandler::HandleCancelImportExportProcess(
    const base::ListValue* args) {
  ImportExportCleanup();
}

void CertificatesHandler::ImportExportCleanup() {
  file_path_.clear();
  password_.clear();
  file_data_.clear();
  use_hardware_backed_ = false;
  selected_cert_list_.clear();
  slot_.reset();
  tracker_.TryCancelAll();

  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();
  select_file_dialog_ = NULL;
}

void CertificatesHandler::HandleImportServer(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  AssignWebUICallbackId(args);

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  ShowCertSelectFileDialog(
      select_file_dialog_.get(), ui::SelectFileDialog::SELECT_OPEN_FILE,
      base::FilePath(), GetParentWindow(),
      reinterpret_cast<void*>(IMPORT_SERVER_FILE_SELECTED));
}

void CertificatesHandler::ImportServerFileSelected(const base::FilePath& path) {
  file_access_provider_->StartRead(
      path, base::Bind(&CertificatesHandler::ImportServerFileRead,
                       base::Unretained(this)),
      &tracker_);
}

void CertificatesHandler::ImportServerFileRead(const int* read_errno,
                                               const std::string* data) {
  if (*read_errno) {
    ImportExportCleanup();
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_SERVER_IMPORT_ERROR_TITLE),
        l10n_util::GetStringFUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_READ_ERROR_FORMAT,
            UTF8ToUTF16(base::safe_strerror(*read_errno))));
    return;
  }

  selected_cert_list_ = net::X509Certificate::CreateCertificateListFromBytes(
      data->data(), data->size(), net::X509Certificate::FORMAT_AUTO);
  if (selected_cert_list_.empty()) {
    ImportExportCleanup();
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_SERVER_IMPORT_ERROR_TITLE),
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_CERT_PARSE_ERROR));
    return;
  }

  net::NSSCertDatabase::ImportCertFailureList not_imported;
  // TODO(mattm): Add UI for trust. http://crbug.com/76274
  bool result = certificate_manager_model_->ImportServerCert(
      selected_cert_list_, net::NSSCertDatabase::TRUST_DEFAULT, &not_imported);
  if (!result) {
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_SERVER_IMPORT_ERROR_TITLE),
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_UNKNOWN_ERROR));
  } else if (!not_imported.empty()) {
    RejectCallbackWithImportError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_SERVER_IMPORT_ERROR_TITLE),
        not_imported);
  } else {
    ResolveCallback(base::Value());
  }
  ImportExportCleanup();
}

void CertificatesHandler::HandleImportCA(const base::ListValue* args) {
  CHECK_EQ(1U, args->GetSize());
  AssignWebUICallbackId(args);

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, new ChromeSelectFilePolicy(web_ui()->GetWebContents()));
  ShowCertSelectFileDialog(select_file_dialog_.get(),
                           ui::SelectFileDialog::SELECT_OPEN_FILE,
                           base::FilePath(), GetParentWindow(),
                           reinterpret_cast<void*>(IMPORT_CA_FILE_SELECTED));
}

void CertificatesHandler::ImportCAFileSelected(const base::FilePath& path) {
  file_access_provider_->StartRead(
      path, base::Bind(&CertificatesHandler::ImportCAFileRead,
                       base::Unretained(this)),
      &tracker_);
}

void CertificatesHandler::ImportCAFileRead(const int* read_errno,
                                           const std::string* data) {
  if (*read_errno) {
    ImportExportCleanup();
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_CA_IMPORT_ERROR_TITLE),
        l10n_util::GetStringFUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_READ_ERROR_FORMAT,
            UTF8ToUTF16(base::safe_strerror(*read_errno))));
    return;
  }

  selected_cert_list_ = net::X509Certificate::CreateCertificateListFromBytes(
      data->data(), data->size(), net::X509Certificate::FORMAT_AUTO);
  if (selected_cert_list_.empty()) {
    ImportExportCleanup();
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_CA_IMPORT_ERROR_TITLE),
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_CERT_PARSE_ERROR));
    return;
  }

  scoped_refptr<net::X509Certificate> root_cert =
      certificate_manager_model_->cert_db()->FindRootInList(
          selected_cert_list_);

  // TODO(mattm): check here if root_cert is not a CA cert and show error.

  base::Value cert_name(root_cert->subject().GetDisplayName());
  ResolveCallback(cert_name);
}

void CertificatesHandler::HandleImportCATrustSelected(
    const base::ListValue* args) {
  CHECK_EQ(4U, args->GetSize());
  AssignWebUICallbackId(args);

  bool trust_ssl = false;
  bool trust_email = false;
  bool trust_obj_sign = false;
  CHECK(args->GetBoolean(1, &trust_ssl));
  CHECK(args->GetBoolean(2, &trust_email));
  CHECK(args->GetBoolean(3, &trust_obj_sign));

  // TODO(mattm): add UI for setting explicit distrust, too.
  // http://crbug.com/128411
  net::NSSCertDatabase::ImportCertFailureList not_imported;
  bool result = certificate_manager_model_->ImportCACerts(
      selected_cert_list_,
      trust_ssl * net::NSSCertDatabase::TRUSTED_SSL +
          trust_email * net::NSSCertDatabase::TRUSTED_EMAIL +
          trust_obj_sign * net::NSSCertDatabase::TRUSTED_OBJ_SIGN,
      &not_imported);
  if (!result) {
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_CA_IMPORT_ERROR_TITLE),
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_UNKNOWN_ERROR));
  } else if (!not_imported.empty()) {
    RejectCallbackWithImportError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_CA_IMPORT_ERROR_TITLE),
        not_imported);
  } else {
    ResolveCallback(base::Value());
  }
  ImportExportCleanup();
}

void CertificatesHandler::HandleExportCertificate(const base::ListValue* args) {
  net::X509Certificate* cert = cert_id_map_->CallbackArgsToCert(args);
  if (!cert)
    return;
  ShowCertExportDialog(web_ui()->GetWebContents(), GetParentWindow(), cert);
}

void CertificatesHandler::HandleDeleteCertificate(const base::ListValue* args) {
  CHECK_EQ(2U, args->GetSize());
  AssignWebUICallbackId(args);
  std::string node_id;
  CHECK(args->GetString(1, &node_id));

  net::X509Certificate* cert = cert_id_map_->IdToCert(node_id);
  CHECK(cert);

  bool result = certificate_manager_model_->Delete(cert);
  if (!result) {
    // TODO(mattm): better error messages?
    RejectCallbackWithError(
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_DELETE_CERT_ERROR_TITLE),
        l10n_util::GetStringUTF8(
            IDS_SETTINGS_CERTIFICATE_MANAGER_UNKNOWN_ERROR));
  } else {
    ResolveCallback(base::Value());
  }
}

void CertificatesHandler::OnCertificateManagerModelCreated(
    std::unique_ptr<CertificateManagerModel> model) {
  certificate_manager_model_ = std::move(model);
  CertificateManagerModelReady();
}

void CertificatesHandler::CertificateManagerModelReady() {
  base::Value user_db_available_value(
      certificate_manager_model_->is_user_db_available());
  base::Value tpm_available_value(
      certificate_manager_model_->is_tpm_available());
  FireWebUIListener("certificates-model-ready", user_db_available_value,
                    tpm_available_value);
  certificate_manager_model_->Refresh();
}

void CertificatesHandler::HandleRefreshCertificates(
    const base::ListValue* args) {
  AllowJavascript();

  if (certificate_manager_model_) {
    // Already have a model, the webui must be re-loading.  Just re-run the
    // webui initialization.
    CertificateManagerModelReady();
    return;
  }

  if (!requested_certificate_manager_model_) {
    // Request that a model be created.
    CertificateManagerModel::Create(
        Profile::FromWebUI(web_ui()), this,
        base::Bind(&CertificatesHandler::OnCertificateManagerModelCreated,
                   weak_ptr_factory_.GetWeakPtr()));
    requested_certificate_manager_model_ = true;
    return;
  }

  // We are already waiting for a CertificateManagerModel to be created, no need
  // to do anything.
}

void CertificatesHandler::PopulateTree(
    const std::string& tab_name,
    net::CertType type,
    const net::CertificateList& web_trust_certs) {
  std::unique_ptr<icu::Collator> collator;
  UErrorCode error = U_ZERO_ERROR;
  collator.reset(icu::Collator::createInstance(
      icu::Locale(g_browser_process->GetApplicationLocale().c_str()), error));
  if (U_FAILURE(error))
    collator.reset(NULL);
  DictionaryIdComparator comparator(collator.get());
  CertificateManagerModel::OrgGroupingMap map;

  certificate_manager_model_->FilterAndBuildOrgGroupingMap(type, &map);

  {
    std::unique_ptr<base::ListValue> nodes =
        base::MakeUnique<base::ListValue>();
    for (CertificateManagerModel::OrgGroupingMap::iterator i = map.begin();
         i != map.end(); ++i) {
      // Populate first level (org name).
      std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
      dict->SetString(kKeyField, OrgNameToId(i->first));
      dict->SetString(kNameField, i->first);

      // Populate second level (certs).
      auto subnodes = base::MakeUnique<base::ListValue>();
      for (net::CertificateList::const_iterator org_cert_it = i->second.begin();
           org_cert_it != i->second.end(); ++org_cert_it) {
        std::unique_ptr<base::DictionaryValue> cert_dict(
            new base::DictionaryValue);
        net::X509Certificate* cert = org_cert_it->get();
        cert_dict->SetString(kKeyField, cert_id_map_->CertToId(cert));
        cert_dict->SetString(
            kNameField, certificate_manager_model_->GetColumnText(
                            *cert, CertificateManagerModel::COL_SUBJECT_NAME));
        cert_dict->SetBoolean(
            kReadonlyField,
            certificate_manager_model_->cert_db()->IsReadOnly(cert));
        // Policy-installed certificates with web trust are trusted.
        bool policy_trusted =
            IsPolicyInstalledWithWebTrust(web_trust_certs, cert);
        cert_dict->SetBoolean(
            kUntrustedField,
            !policy_trusted &&
                certificate_manager_model_->cert_db()->IsUntrusted(cert));
        cert_dict->SetBoolean(kPolicyField, policy_trusted);
        // TODO(hshi): This should be determined by testing for PKCS #11
        // CKA_EXTRACTABLE attribute. We may need to use the NSS function
        // PK11_ReadRawAttribute to do that.
        cert_dict->SetBoolean(
            kExtractableField,
            !certificate_manager_model_->IsHardwareBacked(cert));
        // TODO(mattm): Other columns.
        subnodes->Append(std::move(cert_dict));
      }
      std::sort(subnodes->begin(), subnodes->end(), comparator);

      dict->Set(kSubnodesField, std::move(subnodes));
      nodes->Append(std::move(dict));
    }
    std::sort(nodes->begin(), nodes->end(), comparator);

    FireWebUIListener("certificates-changed", base::Value(tab_name), *nodes);
  }
}

void CertificatesHandler::ResolveCallback(const base::Value& response) {
  DCHECK(!webui_callback_id_.empty());
  ResolveJavascriptCallback(base::Value(webui_callback_id_), response);
  webui_callback_id_.clear();
}

void CertificatesHandler::RejectCallback(const base::Value& response) {
  DCHECK(!webui_callback_id_.empty());
  RejectJavascriptCallback(base::Value(webui_callback_id_), response);
  webui_callback_id_.clear();
}

void CertificatesHandler::RejectCallbackWithError(const std::string& title,
                                                  const std::string& error) {
  std::unique_ptr<base::DictionaryValue> error_info(new base::DictionaryValue);
  error_info->SetString(kErrorTitle, title);
  error_info->SetString(kErrorDescription, error);
  RejectCallback(*error_info);
}

void CertificatesHandler::RejectCallbackWithImportError(
    const std::string& title,
    const net::NSSCertDatabase::ImportCertFailureList& not_imported) {
  std::string error;
  if (selected_cert_list_.size() == 1)
    error = l10n_util::GetStringUTF8(
        IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_SINGLE_NOT_IMPORTED);
  else if (not_imported.size() == selected_cert_list_.size())
    error = l10n_util::GetStringUTF8(
        IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_ALL_NOT_IMPORTED);
  else
    error = l10n_util::GetStringUTF8(
        IDS_SETTINGS_CERTIFICATE_MANAGER_IMPORT_SOME_NOT_IMPORTED);

  std::unique_ptr<base::ListValue> cert_error_list =
      base::MakeUnique<base::ListValue>();
  for (size_t i = 0; i < not_imported.size(); ++i) {
    const net::NSSCertDatabase::ImportCertFailure& failure = not_imported[i];
    std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
    dict->SetString(kNameField,
                    failure.certificate->subject().GetDisplayName());
    dict->SetString(kErrorField, NetErrorToString(failure.net_error));
    cert_error_list->Append(std::move(dict));
  }

  std::unique_ptr<base::DictionaryValue> error_info(new base::DictionaryValue);
  error_info->SetString(kErrorTitle, title);
  error_info->SetString(kErrorDescription, error);
  error_info->Set(kCertificateErrors, std::move(cert_error_list));
  RejectCallback(*error_info);
}

gfx::NativeWindow CertificatesHandler::GetParentWindow() const {
  return web_ui()->GetWebContents()->GetTopLevelNativeWindow();
}

}  // namespace settings
