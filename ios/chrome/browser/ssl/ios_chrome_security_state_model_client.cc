// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ssl/ios_chrome_security_state_model_client.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "ios/web/public/cert_store.h"
#include "ios/web/public/navigation_item.h"
#include "ios/web/public/navigation_manager.h"
#include "ios/web/public/origin_util.h"
#include "ios/web/public/security_style.h"
#include "ios/web/public/ssl_status.h"
#include "ios/web/public/web_state/web_state.h"
#include "net/cert/x509_certificate.h"

DEFINE_WEB_STATE_USER_DATA_KEY(IOSChromeSecurityStateModelClient);

namespace {

// Converts a web::SecurityStyle (an indicator of a request's
// overall security level computed by //ios/web) into a
// SecurityStateModel::SecurityLevel (a finer-grained SecurityStateModel
// concept that can express all of SecurityStateModel's policies that
// //ios/web doesn't necessarily know about).
security_state::SecurityStateModel::SecurityLevel
GetSecurityLevelForSecurityStyle(web::SecurityStyle style) {
  switch (style) {
    case web::SECURITY_STYLE_UNKNOWN:
      NOTREACHED();
      return security_state::SecurityStateModel::NONE;
    case web::SECURITY_STYLE_UNAUTHENTICATED:
      return security_state::SecurityStateModel::NONE;
    case web::SECURITY_STYLE_AUTHENTICATION_BROKEN:
      return security_state::SecurityStateModel::SECURITY_ERROR;
    case web::SECURITY_STYLE_WARNING:
      // //ios/web currently doesn't use this style.
      NOTREACHED();
      return security_state::SecurityStateModel::SECURITY_WARNING;
    case web::SECURITY_STYLE_AUTHENTICATED:
      return security_state::SecurityStateModel::SECURE;
  }
  return security_state::SecurityStateModel::NONE;
}

}  // namespace

IOSChromeSecurityStateModelClient::IOSChromeSecurityStateModelClient(
    web::WebState* web_state)
    : web_state_(web_state),
      security_state_model_(new security_state::SecurityStateModel()) {
  security_state_model_->SetClient(this);
}

IOSChromeSecurityStateModelClient::~IOSChromeSecurityStateModelClient() {}

const security_state::SecurityStateModel::SecurityInfo&
IOSChromeSecurityStateModelClient::GetSecurityInfo() const {
  return security_state_model_->GetSecurityInfo();
}

bool IOSChromeSecurityStateModelClient::RetrieveCert(
    scoped_refptr<net::X509Certificate>* cert) {
  web::NavigationItem* item =
      web_state_->GetNavigationManager()->GetVisibleItem();
  if (!item)
    return false;

  int cert_id = item->GetSSL().cert_id;
  return web::CertStore::GetInstance()->RetrieveCert(cert_id, cert);
}

bool IOSChromeSecurityStateModelClient::UsedPolicyInstalledCertificate() {
  return false;
}

bool IOSChromeSecurityStateModelClient::IsOriginSecure(const GURL& url) {
  return web::IsOriginSecure(url);
}

void IOSChromeSecurityStateModelClient::GetVisibleSecurityState(
    security_state::SecurityStateModel::VisibleSecurityState* state) {
  web::NavigationItem* item =
      web_state_->GetNavigationManager()->GetVisibleItem();
  if (!item || item->GetSSL().security_style == web::SECURITY_STYLE_UNKNOWN) {
    *state = security_state::SecurityStateModel::VisibleSecurityState();
    return;
  }

  state->initialized = true;
  state->url = item->GetURL();
  const web::SSLStatus& ssl = item->GetSSL();
  state->initial_security_level =
      GetSecurityLevelForSecurityStyle(ssl.security_style);
  state->cert_id = ssl.cert_id;
  state->cert_status = ssl.cert_status;
  state->connection_status = ssl.connection_status;
  state->security_bits = ssl.security_bits;
  state->displayed_mixed_content =
      (ssl.content_status & web::SSLStatus::DISPLAYED_INSECURE_CONTENT)
          ? true
          : false;
}
