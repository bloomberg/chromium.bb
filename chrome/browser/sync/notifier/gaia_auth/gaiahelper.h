// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_GAIA_AUTH_GAIAHELPER_H__
#define CHROME_BROWSER_SYNC_NOTIFIER_GAIA_AUTH_GAIAHELPER_H__

#include <string>

namespace talk_base {
class CryptString;
class HttpClient;
}

namespace buzz {

///////////////////////////////////////////////////////////////////////////////

class CaptchaAnswer {
 public:
  CaptchaAnswer() {}
  CaptchaAnswer(const std::string& token, const std::string& answer)
    : captcha_token_(token), captcha_answer_(answer) {
  }
  const std::string& captcha_token() const { return captcha_token_; }
  const std::string& captcha_answer() const { return captcha_answer_; }

 private:
  std::string captcha_token_;
  std::string captcha_answer_;
};

class GaiaServer {
 public:
  GaiaServer();

  bool SetServer(const char* url);  // protocol://server:port
  bool SetDebugServer(const char* server);  // server:port

  const std::string& hostname() const { return hostname_; }
  int port() const { return port_; }
  bool use_ssl() const { return use_ssl_; }

  std::string http_prefix() const;

 private:
  std::string hostname_;
  int port_;
  bool use_ssl_;
};

///////////////////////////////////////////////////////////////////////////////
// Gaia Authentication Helper Functions
///////////////////////////////////////////////////////////////////////////////

enum GaiaResponse { GR_ERROR, GR_UNAUTHORIZED, GR_SUCCESS };

bool GaiaRequestSid(talk_base::HttpClient* client,
                    const std::string& username,
                    const talk_base::CryptString& password,
                    const std::string& signature,
                    const std::string& service,
                    const CaptchaAnswer& captcha_answer,
                    const GaiaServer& gaia_server);

GaiaResponse GaiaParseSidResponse(const talk_base::HttpClient& client,
                                  const GaiaServer& gaia_server,
                                  std::string* captcha_token,
                                  std::string* captcha_url,
                                  std::string* sid,
                                  std::string* lsid,
                                  std::string* auth);

bool GaiaRequestAuthToken(talk_base::HttpClient* client,
                          const std::string& sid,
                          const std::string& lsid,
                          const std::string& service,
                          const GaiaServer& gaia_server);

GaiaResponse GaiaParseAuthTokenResponse(const talk_base::HttpClient& client,
                                        std::string* auth_token);

///////////////////////////////////////////////////////////////////////////////

}  // namespace buzz

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_GAIA_AUTH_GAIAHELPER_H__
