// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/platform_keys_certificate_selector_chromeos.h"

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

PlatformKeysCertificateSelector::PlatformKeysCertificateSelector(
    const net::CertificateList& certificates,
    const std::string& extension_name,
    const CertificateSelectedCallback& callback,
    content::WebContents* web_contents)
    : CertificateSelector(certificates, web_contents),
      extension_name_(extension_name),
      callback_(callback) {
}

PlatformKeysCertificateSelector::~PlatformKeysCertificateSelector() {
}

void PlatformKeysCertificateSelector::Init() {
  const base::string16 text =
      l10n_util::GetStringFUTF16(IDS_PLATFORM_KEYS_SELECT_CERT_DIALOG_TEXT,
                                 base::ASCIIToUTF16(extension_name_));
  CertificateSelector::InitWithText(text);
}

bool PlatformKeysCertificateSelector::Cancel() {
  callback_.Run(nullptr);
  return true;
}

bool PlatformKeysCertificateSelector::Accept() {
  scoped_refptr<net::X509Certificate> cert = GetSelectedCert();
  if (!cert)
    return false;
  callback_.Run(cert);
  return true;
}

void ShowPlatformKeysCertificateSelector(
    content::WebContents* web_contents,
    const std::string& extension_name,
    const net::CertificateList& certificates,
    const base::Callback<void(const scoped_refptr<net::X509Certificate>&)>&
        callback) {
  PlatformKeysCertificateSelector* selector =
      new PlatformKeysCertificateSelector(certificates, extension_name,
                                          callback, web_contents);
  selector->Init();
  selector->Show();
}

}  // namespace chromeos
