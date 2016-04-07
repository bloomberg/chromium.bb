// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SSL_IOS_CHROME_SECURITY_STATE_MODEL_CLIENT_H_
#define IOS_CHROME_BROWSER_SSL_IOS_CHROME_SECURITY_STATE_MODEL_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "components/security_state/security_state_model.h"
#include "components/security_state/security_state_model_client.h"
#include "ios/web/public/web_state/web_state_user_data.h"

namespace web {
class WebState;
}  // namespace web

// Uses a WebState to provide a SecurityStateModel with the
// information that it needs to determine the page's security status.
class IOSChromeSecurityStateModelClient
    : public security_state::SecurityStateModelClient,
      public web::WebStateUserData<IOSChromeSecurityStateModelClient> {
 public:
  ~IOSChromeSecurityStateModelClient() override;

  const security_state::SecurityStateModel::SecurityInfo& GetSecurityInfo()
      const;

  // SecurityStateModelClient:
  void GetVisibleSecurityState(
      security_state::SecurityStateModel::VisibleSecurityState* state) override;
  bool RetrieveCert(scoped_refptr<net::X509Certificate>* cert) override;
  bool UsedPolicyInstalledCertificate() override;
  bool IsOriginSecure(const GURL& url) override;

 private:
  explicit IOSChromeSecurityStateModelClient(web::WebState* web_state);
  friend class web::WebStateUserData<IOSChromeSecurityStateModelClient>;

  web::WebState* web_state_;
  std::unique_ptr<security_state::SecurityStateModel> security_state_model_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromeSecurityStateModelClient);
};

#endif  // IOS_CHROME_BROWSER_SSL_IOS_CHROME_SECURITY_STATE_MODEL_CLIENT_H_
