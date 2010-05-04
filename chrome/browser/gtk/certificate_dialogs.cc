// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/certificate_dialogs.h"

#include <cms.h>
#include <gtk/gtk.h>

#include <vector>

#include "app/l10n_util.h"
#include "base/base64.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/shell_dialogs.h"
#include "chrome/third_party/mozilla_security_manager/nsNSSCertHelper.h"
#include "chrome/third_party/mozilla_security_manager/nsNSSCertificate.h"
#include "grit/generated_resources.h"

// PSM = Mozilla's Personal Security Manager.
namespace psm = mozilla_security_manager;

namespace {

////////////////////////////////////////////////////////////////////////////////
// General utility functions.

class Writer : public Task {
 public:
  Writer(const FilePath& path, const std::string& data)
      : path_(path), data_(data) {
  }

  virtual void Run() {
    int bytes_written = file_util::WriteFile(path_, data_.data(), data_.size());
    if (bytes_written != static_cast<ssize_t>(data_.size())) {
      LOG(ERROR) << "Writing " << path_.value() << " ("
                 << data_.size() << "B) returned " << bytes_written;
    }
  }
 private:
  FilePath path_;
  std::string data_;
};

void WriteFileOnFileThread(const FilePath& path, const std::string& data) {
  ChromeThread::PostTask(
      ChromeThread::FILE, FROM_HERE, new Writer(path, data));
}

////////////////////////////////////////////////////////////////////////////////
// NSS certificate export functions.

class FreeNSSCMSMessage {
 public:
  inline void operator()(NSSCMSMessage* x) const {
    NSS_CMSMessage_Destroy(x);
  }
};
typedef scoped_ptr_malloc<NSSCMSMessage, FreeNSSCMSMessage>
    ScopedNSSCMSMessage;

class FreeNSSCMSSignedData {
 public:
  inline void operator()(NSSCMSSignedData* x) const {
    NSS_CMSSignedData_Destroy(x);
  }
};
typedef scoped_ptr_malloc<NSSCMSSignedData, FreeNSSCMSSignedData>
    ScopedNSSCMSSignedData;

std::string GetDerString(CERTCertificate* cert) {
  return std::string(reinterpret_cast<const char*>(cert->derCert.data),
                     cert->derCert.len);
}

std::string WrapAt64(const std::string &str) {
  std::string result;
  for (size_t i = 0; i < str.size(); i += 64) {
    result.append(str, i, 64);  // Append clamps the len arg internally.
    result.append("\r\n");
  }
  return result;
}

std::string GetBase64String(CERTCertificate* cert) {
  std::string base64;
  if (!base::Base64Encode(GetDerString(cert), &base64)) {
    LOG(ERROR) << "base64 encoding error";
    return "";
  }
  return "-----BEGIN CERTIFICATE-----\r\n" +
      WrapAt64(base64) +
      "-----END CERTIFICATE-----\r\n";
}

std::string GetCMSString(std::vector<CERTCertificate*> cert_chain, size_t start,
                         size_t end) {
  ScopedPRArenaPool arena(PORT_NewArena(1024));
  CHECK(arena.get());

  ScopedNSSCMSMessage message(NSS_CMSMessage_Create(arena.get()));
  CHECK(message.get());

  // First, create SignedData with the certificate only (no chain).
  ScopedNSSCMSSignedData signed_data(
      NSS_CMSSignedData_CreateCertsOnly(message.get(), cert_chain[start],
                                        PR_FALSE));
  if (!signed_data.get()) {
    LOG(ERROR) << "NSS_CMSSignedData_Create failed";
    return "";
  }
  // Add the rest of the chain (if any).
  for (size_t i = start + 1; i < end; ++i) {
    if (NSS_CMSSignedData_AddCertificate(signed_data.get(), cert_chain[i]) !=
        SECSuccess) {
      LOG(ERROR) << "NSS_CMSSignedData_AddCertificate failed on " << i;
      return "";
    }
  }

  NSSCMSContentInfo *cinfo = NSS_CMSMessage_GetContentInfo(message.get());
  if (NSS_CMSContentInfo_SetContent_SignedData(
      message.get(), cinfo, signed_data.get()) == SECSuccess) {
    ignore_result(signed_data.release());
  } else {
    LOG(ERROR) << "NSS_CMSMessage_GetContentInfo failed";
    return "";
  }

  SECItem cert_p7 = { siBuffer, NULL, 0 };
  NSSCMSEncoderContext *ecx = NSS_CMSEncoder_Start(message.get(), NULL, NULL,
                                                   &cert_p7, arena.get(), NULL,
                                                   NULL, NULL, NULL, NULL,
                                                   NULL);
  if (!ecx) {
    LOG(ERROR) << "NSS_CMSEncoder_Start failed";
    return "";
  }

  if (NSS_CMSEncoder_Finish(ecx) != SECSuccess) {
    LOG(ERROR) << "NSS_CMSEncoder_Finish failed";
    return "";
  }

  return std::string(reinterpret_cast<const char*>(cert_p7.data), cert_p7.len);
}

////////////////////////////////////////////////////////////////////////////////
// General utility functions.

class Exporter : public SelectFileDialog::Listener {
 public:
  Exporter(gfx::NativeWindow parent, CERTCertificate* cert);
  ~Exporter();

  // SelectFileDialog::Listener implemenation.
  virtual void FileSelected(const FilePath& path,
                            int index, void* params);
  virtual void FileSelectionCanceled(void* params);
 private:
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // The certificate hierarchy (leaf cert first).
  CERTCertList* cert_chain_list_;
  // The same contents of cert_chain_list_ in a vector for easier access.
  std::vector<CERTCertificate*> cert_chain_;
};

Exporter::Exporter(gfx::NativeWindow parent, CERTCertificate* cert)
    : select_file_dialog_(SelectFileDialog::Create(this)) {
  cert_chain_list_ = CERT_GetCertChainFromCert(cert, PR_Now(),
                                               certUsageSSLServer);
  DCHECK(cert_chain_list_);
  CERTCertListNode* node;
  for (node = CERT_LIST_HEAD(cert_chain_list_);
       !CERT_LIST_END(node, cert_chain_list_);
       node = CERT_LIST_NEXT(node)) {
    cert_chain_.push_back(node->cert);
  }

  // TODO(mattm): should this default to some directory?
  // Maybe SavePackage::GetSaveDirPreference? (Except that it's private.)
  FilePath suggested_path("certificate");
  std::string cert_title = psm::GetCertTitle(cert);
  if (!cert_title.empty())
    suggested_path = FilePath(cert_title);

  SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.extensions.resize(5);
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("pem"));
  file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("crt"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_CERT_EXPORT_TYPE_BASE64));
  file_type_info.extensions[1].push_back(FILE_PATH_LITERAL("pem"));
  file_type_info.extensions[1].push_back(FILE_PATH_LITERAL("crt"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_CERT_EXPORT_TYPE_BASE64_CHAIN));
  file_type_info.extensions[2].push_back(FILE_PATH_LITERAL("der"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_CERT_EXPORT_TYPE_DER));
  file_type_info.extensions[3].push_back(FILE_PATH_LITERAL("p7c"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_CERT_EXPORT_TYPE_PKCS7));
  file_type_info.extensions[4].push_back(FILE_PATH_LITERAL("p7c"));
  file_type_info.extension_description_overrides.push_back(
      l10n_util::GetStringUTF16(IDS_CERT_EXPORT_TYPE_PKCS7_CHAIN));
  file_type_info.include_all_files = true;
  select_file_dialog_->SelectFile(
      SelectFileDialog::SELECT_SAVEAS_FILE, string16(),
      suggested_path, &file_type_info, 1,
      FILE_PATH_LITERAL("crt"), parent,
      NULL);
}

Exporter::~Exporter() {
  CERT_DestroyCertList(cert_chain_list_);
}

void Exporter::FileSelected(const FilePath& path, int index, void* params) {
  std::string data;
  switch (index) {
    case 2:
      for (size_t i = 0; i < cert_chain_.size(); ++i)
        data += GetBase64String(cert_chain_[i]);
      break;
    case 3:
      data = GetDerString(cert_chain_[0]);
      break;
    case 4:
      data = GetCMSString(cert_chain_, 0, 1);
      break;
    case 5:
      data = GetCMSString(cert_chain_, 0, cert_chain_.size());
      break;
    case 1:
    default:
      data = GetBase64String(cert_chain_[0]);
      break;
  }

  if (!data.empty())
    WriteFileOnFileThread(path, data);

  delete this;
}

void Exporter::FileSelectionCanceled(void* params) {
  delete this;
}

} // namespace

void ShowCertExportDialog(gfx::NativeWindow parent, CERTCertificate* cert) {
  new Exporter(parent, cert);
}
