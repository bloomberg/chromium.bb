// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/certificate_dialogs.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "chrome/grit/generated_resources.h"
#include "net/base/filename_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "url/gurl.h"

namespace {

void WriterCallback(const base::FilePath& path, const std::string& data) {
  int bytes_written = base::WriteFile(path, data.data(), data.size());
  if (bytes_written != static_cast<ssize_t>(data.size())) {
    LOG(ERROR) << "Writing " << path.value() << " ("
               << data.size() << "B) returned " << bytes_written;
  }
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
  std::string der_cert;
  if (!net::X509Certificate::GetDEREncoded(cert, &der_cert))
    return std::string();
  std::string base64;
  base::Base64Encode(der_cert, &base64);
  return "-----BEGIN CERTIFICATE-----\r\n" +
      WrapAt64(base64) +
      "-----END CERTIFICATE-----\r\n";
}

////////////////////////////////////////////////////////////////////////////////
// General utility functions.

class Exporter : public ui::SelectFileDialog::Listener {
 public:
  Exporter(content::WebContents* web_contents,
           gfx::NativeWindow parent,
           net::X509Certificate::OSCertHandles::iterator certs_begin,
           net::X509Certificate::OSCertHandles::iterator certs_end);
  ~Exporter() override;

  // SelectFileDialog::Listener implemenation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

 private:
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // The certificate hierarchy (leaf cert first).
  net::X509Certificate::OSCertHandles cert_chain_list_;
};

Exporter::Exporter(content::WebContents* web_contents,
                   gfx::NativeWindow parent,
                   net::X509Certificate::OSCertHandles::iterator certs_begin,
                   net::X509Certificate::OSCertHandles::iterator certs_end)
    : select_file_dialog_(ui::SelectFileDialog::Create(
          this,
          std::make_unique<ChromeSelectFilePolicy>(web_contents))) {
  DCHECK(certs_begin != certs_end);
  for (net::X509Certificate::OSCertHandles::iterator i = certs_begin;
       i != certs_end;
       ++i) {
    cert_chain_list_.push_back(net::X509Certificate::DupOSCertHandle(*i));
  }

  // TODO(mattm): should this default to some directory?
  // Maybe SavePackage::GetSaveDirPreference? (Except that it's private.)
  std::string cert_title = x509_certificate_model::GetTitle(*certs_begin);
  base::FilePath suggested_path =
      net::GenerateFileName(GURL::EmptyGURL(),  // url
                            std::string(),      // content_disposition
                            std::string(),      // referrer_charset
                            cert_title,         // suggested_name
                            std::string(),      // mime_type
                            "certificate");     // default_name

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

  std::for_each(cert_chain_list_.begin(),
                cert_chain_list_.end(),
                &net::X509Certificate::FreeOSCertHandle);
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
      net::X509Certificate::GetDEREncoded(cert_chain_list_[0], &data);
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

  if (!data.empty()) {
    base::PostTaskWithTraits(FROM_HERE, {base::MayBlock()},
                             base::BindOnce(&WriterCallback, path, data));
  }

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
  select_file_dialog->SelectFile(
      type, base::string16(),
      suggested_path, &file_type_info,
      1,  // 1-based index for |file_type_info.extensions| to specify default.
      FILE_PATH_LITERAL("crt"),
      parent, params);
}

void ShowCertExportDialog(content::WebContents* web_contents,
                          gfx::NativeWindow parent,
                          const scoped_refptr<net::X509Certificate>& cert) {
  net::X509Certificate::OSCertHandles cert_chain;
  cert_chain.push_back(cert->os_cert_handle());
  const net::X509Certificate::OSCertHandles& certs =
      cert->GetIntermediateCertificates();
  cert_chain.insert(cert_chain.end(), certs.begin(), certs.end());
  new Exporter(web_contents, parent, cert_chain.begin(), cert_chain.end());
}

void ShowCertExportDialog(
    content::WebContents* web_contents,
    gfx::NativeWindow parent,
    net::X509Certificate::OSCertHandles::iterator certs_begin,
    net::X509Certificate::OSCertHandles::iterator certs_end) {
  new Exporter(web_contents, parent, certs_begin, certs_end);
}
