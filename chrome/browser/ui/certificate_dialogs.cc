// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/certificate_dialogs.h"


#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"

using content::BrowserThread;
using content::WebContents;

namespace {

void WriterCallback(const base::FilePath& path, const std::string& data) {
  int bytes_written = file_util::WriteFile(path, data.data(), data.size());
  if (bytes_written != static_cast<ssize_t>(data.size())) {
    LOG(ERROR) << "Writing " << path.value() << " ("
               << data.size() << "B) returned " << bytes_written;
  }
}

void WriteFileOnFileThread(const base::FilePath& path,
                           const std::string& data) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE, base::Bind(&WriterCallback, path, data));
}

std::string WrapAt64(const std::string &str) {
  std::string result;
  for (size_t i = 0; i < str.size(); i += 64) {
    result.append(str, i, 64);  // Append clamps the len arg internally.
    result.append("\r\n");
  }
  return result;
}

std::string GetBase64String(net::X509Certificate::OSCertHandle cert) {
  std::string base64;
  if (!base::Base64Encode(
      x509_certificate_model::GetDerString(cert), &base64)) {
    LOG(ERROR) << "base64 encoding error";
    return "";
  }
  return "-----BEGIN CERTIFICATE-----\r\n" +
      WrapAt64(base64) +
      "-----END CERTIFICATE-----\r\n";
}

////////////////////////////////////////////////////////////////////////////////
// General utility functions.

class Exporter : public ui::SelectFileDialog::Listener {
 public:
  Exporter(WebContents* web_contents, gfx::NativeWindow parent,
           net::X509Certificate::OSCertHandle cert);
  virtual ~Exporter();

  // SelectFileDialog::Listener implemenation.
  virtual void FileSelected(const base::FilePath& path,
                            int index, void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;
 private:
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // The certificate hierarchy (leaf cert first).
  net::X509Certificate::OSCertHandles cert_chain_list_;
};

Exporter::Exporter(WebContents* web_contents,
                   gfx::NativeWindow parent,
                   net::X509Certificate::OSCertHandle cert)
    : select_file_dialog_(ui::SelectFileDialog::Create(
        this, new ChromeSelectFilePolicy(web_contents))) {
  x509_certificate_model::GetCertChainFromCert(cert, &cert_chain_list_);

  // TODO(mattm): should this default to some directory?
  // Maybe SavePackage::GetSaveDirPreference? (Except that it's private.)
  base::FilePath suggested_path("certificate");
  std::string cert_title = x509_certificate_model::GetTitle(cert);
  if (!cert_title.empty())
    suggested_path = base::FilePath(cert_title);

  ShowCertSelectFileDialog(select_file_dialog_.get(),
                           ui::SelectFileDialog::SELECT_SAVEAS_FILE,
                           suggested_path,
                           parent,
                           NULL);
}

Exporter::~Exporter() {
  // There may be pending file dialogs, we need to tell them that we've gone
  // away so they don't try and call back to us.
  if (select_file_dialog_.get())
    select_file_dialog_->ListenerDestroyed();

  x509_certificate_model::DestroyCertChain(&cert_chain_list_);
}

void Exporter::FileSelected(const base::FilePath& path, int index,
                            void* params) {
  std::string data;
  switch (index) {
    case 2:
      for (size_t i = 0; i < cert_chain_list_.size(); ++i)
        data += GetBase64String(cert_chain_list_[i]);
      break;
    case 3:
      data = x509_certificate_model::GetDerString(cert_chain_list_[0]);
      break;
    case 4:
      data = x509_certificate_model::GetCMSString(cert_chain_list_, 0, 1);
      break;
    case 5:
      data = x509_certificate_model::GetCMSString(
          cert_chain_list_, 0, cert_chain_list_.size());
      break;
    case 1:
    default:
      data = GetBase64String(cert_chain_list_[0]);
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

void ShowCertSelectFileDialog(ui::SelectFileDialog* select_file_dialog,
                              ui::SelectFileDialog::Type type,
                              const base::FilePath& suggested_path,
                              gfx::NativeWindow parent,
                              void* params) {
  ui::SelectFileDialog::FileTypeInfo file_type_info;
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
  // TODO(kinaba): http://crbug.com/140425. Turn file_type_info.support_drive
  // on for saving once Google Drive client on ChromeOS supports it.
  if (type == ui::SelectFileDialog::SELECT_OPEN_FILE)
    file_type_info.support_drive = true;
  select_file_dialog->SelectFile(
      type, string16(),
      suggested_path, &file_type_info, 1,
      FILE_PATH_LITERAL("crt"),
      parent, params);
}

void ShowCertExportDialog(WebContents* web_contents,
                          gfx::NativeWindow parent,
                          net::X509Certificate::OSCertHandle cert) {
  new Exporter(web_contents, parent, cert);
}
