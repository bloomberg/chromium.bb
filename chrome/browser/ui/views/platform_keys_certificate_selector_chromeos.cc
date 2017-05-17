// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/platform_keys_certificate_selector_chromeos.h"

#include <stddef.h>
#include <utility>

#include "base/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/styled_label.h"

namespace chromeos {

PlatformKeysCertificateSelector::PlatformKeysCertificateSelector(
    const net::CertificateList& certificates,
    const std::string& extension_name,
    const CertificateSelectedCallback& callback,
    content::WebContents* web_contents)
    : CertificateSelector(certificates, web_contents),
      extension_name_(extension_name),
      callback_(callback) {
  DCHECK(!callback_.is_null());
  chrome::RecordDialogCreation(
      chrome::DialogIdentifier::PLATFORM_KEYS_CERTIFICATE_SELECTOR);
}

PlatformKeysCertificateSelector::~PlatformKeysCertificateSelector() {
  // Ensure to call back even if the dialog was closed because of the views
  // hierarchy being destroyed.
  if (!callback_.is_null())
    base::ResetAndReturn(&callback_).Run(nullptr);
}

void PlatformKeysCertificateSelector::Init() {
  const base::string16 name = base::ASCIIToUTF16(extension_name_);

  size_t offset;
  const base::string16 text = l10n_util::GetStringFUTF16(
      IDS_PLATFORM_KEYS_SELECT_CERT_DIALOG_TEXT, name, &offset);

  std::unique_ptr<views::StyledLabel> label(
      new views::StyledLabel(text, nullptr /* no listener */));

  views::StyledLabel::RangeStyleInfo bold_style;
  bold_style.weight = gfx::Font::Weight::BOLD;
  label->AddStyleRange(gfx::Range(offset, offset + name.size()), bold_style);
  CertificateSelector::InitWithText(std::move(label));
}

bool PlatformKeysCertificateSelector::Cancel() {
  DCHECK(!callback_.is_null());
  base::ResetAndReturn(&callback_).Run(nullptr);
  return true;
}

bool PlatformKeysCertificateSelector::Accept() {
  DCHECK(!callback_.is_null());
  scoped_refptr<net::X509Certificate> cert = GetSelectedCert();
  if (!cert)
    return false;
  base::ResetAndReturn(&callback_).Run(cert);
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
