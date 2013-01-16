// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/certificate_viewer_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_url_data_manager.h"
#include "chrome/browser/ui/webui/chrome_web_ui_data_source.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

CertificateViewerUI::CertificateViewerUI(content::WebUI* web_ui)
    : ConstrainedWebDialogUI(web_ui) {
  // Set up the chrome://view-cert source.
  ChromeWebUIDataSource* html_source =
      new ChromeWebUIDataSource(chrome::kChromeUICertificateViewerHost);

  // Localized strings.
  html_source->set_use_json_js_format_v2();
  html_source->AddLocalizedString("general", IDS_CERT_INFO_GENERAL_TAB_LABEL);
  html_source->AddLocalizedString("details", IDS_CERT_INFO_DETAILS_TAB_LABEL);
  html_source->AddLocalizedString("close", IDS_CLOSE);
  html_source->AddLocalizedString("export",
      IDS_CERT_DETAILS_EXPORT_CERTIFICATE);
  html_source->AddLocalizedString("usages",
      IDS_CERT_INFO_VERIFIED_USAGES_GROUP);
  html_source->AddLocalizedString("issuedTo", IDS_CERT_INFO_SUBJECT_GROUP);
  html_source->AddLocalizedString("issuedBy", IDS_CERT_INFO_ISSUER_GROUP);
  html_source->AddLocalizedString("cn", IDS_CERT_INFO_COMMON_NAME_LABEL);
  html_source->AddLocalizedString("o", IDS_CERT_INFO_ORGANIZATION_LABEL);
  html_source->AddLocalizedString("ou",
      IDS_CERT_INFO_ORGANIZATIONAL_UNIT_LABEL);
  html_source->AddLocalizedString("sn", IDS_CERT_INFO_SERIAL_NUMBER_LABEL);
  html_source->AddLocalizedString("validity", IDS_CERT_INFO_VALIDITY_GROUP);
  html_source->AddLocalizedString("issuedOn", IDS_CERT_INFO_ISSUED_ON_LABEL);
  html_source->AddLocalizedString("expiresOn", IDS_CERT_INFO_EXPIRES_ON_LABEL);
  html_source->AddLocalizedString("fingerprints",
      IDS_CERT_INFO_FINGERPRINTS_GROUP);
  html_source->AddLocalizedString("sha256",
      IDS_CERT_INFO_SHA256_FINGERPRINT_LABEL);
  html_source->AddLocalizedString("sha1", IDS_CERT_INFO_SHA1_FINGERPRINT_LABEL);
  html_source->AddLocalizedString("hierarchy",
      IDS_CERT_DETAILS_CERTIFICATE_HIERARCHY_LABEL);
  html_source->AddLocalizedString("certFields",
      IDS_CERT_DETAILS_CERTIFICATE_FIELDS_LABEL);
  html_source->AddLocalizedString("certFieldVal",
      IDS_CERT_DETAILS_CERTIFICATE_FIELD_VALUE_LABEL);
  html_source->set_json_path("strings.js");

  // Add required resources.
  html_source->add_resource_path("certificate_viewer.js",
      IDR_CERTIFICATE_VIEWER_JS);
  html_source->add_resource_path("certificate_viewer.css",
      IDR_CERTIFICATE_VIEWER_CSS);
  html_source->set_default_resource(IDR_CERTIFICATE_VIEWER_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  ChromeURLDataManager::AddDataSourceImpl(profile, html_source);
}

CertificateViewerUI::~CertificateViewerUI() {
}
