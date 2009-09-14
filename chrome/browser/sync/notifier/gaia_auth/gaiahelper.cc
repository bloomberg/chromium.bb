// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/gaia_auth/gaiahelper.h"
#include "talk/base/common.h"
#include "talk/base/cryptstring.h"
#include "talk/base/httpclient.h"
#include "talk/base/httpcommon-inl.h"
#include "talk/base/stringutils.h"
#include "talk/base/urlencode.h"
#include "talk/xmpp/constants.h"
#include "talk/xmpp/jid.h"

///////////////////////////////////////////////////////////////////////////////

namespace {

std::string GetValueForKey(const std::string& key, const std::string& nvp) {
  size_t start_of_line = 0;
  size_t end_of_line = 0;
  for (;;) {  // For each line.
    start_of_line = nvp.find_first_not_of("\r\n", end_of_line);
    if (start_of_line == std::string::npos)
      break;
    end_of_line = nvp.find_first_of("\r\n", start_of_line);
    if (end_of_line == std::string::npos) {
      end_of_line = nvp.length();
    }
    size_t equals = nvp.find('=', start_of_line);
    if (equals >= end_of_line ||
        equals == std::string::npos ||
        equals - start_of_line != key.length()) {
      continue;
    }

    if (nvp.find(key, start_of_line) == start_of_line) {
      return std::string(nvp, equals + 1, end_of_line - equals - 1);
    }
  }
  return "";
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////

namespace buzz {

GaiaServer::GaiaServer()
  : hostname_("www.google.com"),
    port_(443),
    use_ssl_(true) {
}

bool GaiaServer::SetServer(const char* url) {
  talk_base::Url<char> parsed(url);
  hostname_ = parsed.server();
  port_ = parsed.port();
  use_ssl_ = parsed.secure();
  return true;
}

bool GaiaServer::SetDebugServer(const char* server) {
  const char* colon = strchr(server, ':');
  if (colon) {
    hostname_ = std::string(server, colon - server);
    port_ = atoi(colon + 1);
    use_ssl_ = false;
    return true;
  }
  return false;
}

std::string GaiaServer::http_prefix() const {
  talk_base::Url<char> parsed("", hostname_, port_);
  parsed.set_secure(use_ssl_);
  return parsed.url();
}

///////////////////////////////////////////////////////////////////////////////

bool GaiaRequestSid(talk_base::HttpClient* client,
                    const std::string& username,
                    const talk_base::CryptString& password,
                    const std::string& signature,
                    const std::string& service,
                    const CaptchaAnswer& captcha_answer,
                    const GaiaServer& gaia_server) {
  buzz::Jid jid(username);
  std::string usable_name = username;
  if (jid.domain() == buzz::STR_DEFAULT_DOMAIN) {
    // The default domain (default.talk.google.com) is not usable for Gaia
    // auth. But both gmail.com and googlemain.com will work, because the gaia
    // server doesn't check to make sure the appropriate one is being used. So
    // we just slam on gmail.com
    usable_name = jid.node() + "@" + buzz::STR_GMAIL_COM;
  }

  std::string post_data;
  post_data += "Email=" + UrlEncodeString(usable_name);
  post_data += "&Passwd=" + password.UrlEncode();
  post_data += "&PersistentCookie=false";
  post_data += "&source=" + signature;
  // TODO(chron): This behavior is not the same as in the other gaia auth
  // loader. We should make it the same. Probably GOOGLE is enough, we don't
  // want to auth against hosted accounts.
  post_data += "&accountType=HOSTED_OR_GOOGLE";
  post_data += "&skipvpage=true";
  if (!service.empty()) {
    post_data += "&service=" + service;
  }

  if (!captcha_answer.captcha_token().empty()) {
    post_data += "&logintoken=" + captcha_answer.captcha_token();
    post_data += "&logincaptcha="
                 + UrlEncodeString(captcha_answer.captcha_answer());
  }

  client->reset();
  client->set_server(talk_base::SocketAddress(gaia_server.hostname(),
                                              gaia_server.port(), false));
  client->request().verb = talk_base::HV_POST;
  client->request().path = "/accounts/ClientAuth";
  client->request().setContent("application/x-www-form-urlencoded",
    new talk_base::MemoryStream(post_data.data(), post_data.size()));
  client->response().document.reset(new talk_base::MemoryStream);
  client->start();
  return true;
}

GaiaResponse GaiaParseSidResponse(const talk_base::HttpClient& client,
                                  const GaiaServer& gaia_server,
                                  std::string* captcha_token,
                                  std::string* captcha_url,
                                  std::string* sid,
                                  std::string* lsid,
                                  std::string* auth) {
  uint32 status_code = client.response().scode;
  const talk_base::MemoryStream* stream =
      static_cast<const talk_base::MemoryStream*>(
          client.response().document.get());
  size_t length;
  stream->GetPosition(&length);
  std::string response;
  if (length > 0) {
    response.assign(stream->GetBuffer(), length);
  }

  LOG(LS_INFO) << "GaiaAuth request to " << client.request().path;
  LOG(LS_INFO) << "GaiaAuth Status Code: " << status_code;
  LOG(LS_INFO) << response;

  if (status_code == talk_base::HC_FORBIDDEN) {
    // The error URL may be the relative path to the captcha jpg.
    std::string image_url = GetValueForKey("CaptchaUrl", response);
    if (!image_url.empty()) {
      // We should activate this "full url code" once we have a better ways
      // to crack the URL for later download.  Right now we are too dependent
      // on what Gaia returns.
#if 0
      if (image_url.find("http://") != 0 &&
          image_url.find("https://") != 0) {
        if (image_url.find("/") == 0) {
          *captcha_url = gaia_server.http_prefix() + image_url;
        } else {
          *captcha_url = Utf8(gaia_server.http_prefix()).AsString()
                         + "/accounts/" + image_url;
        }
      }
#else
      *captcha_url = "/accounts/" + image_url;
#endif

      *captcha_token = GetValueForKey("CaptchaToken", response);
    }
    return GR_UNAUTHORIZED;
  }

  if (status_code != talk_base::HC_OK) {
    return GR_ERROR;
  }

  *sid = GetValueForKey("SID", response);
  *lsid = GetValueForKey("LSID", response);
  if (auth) {
    *auth = GetValueForKey("Auth", response);
  }
  if (sid->empty() || lsid->empty()) {
    return GR_ERROR;
  }

  return GR_SUCCESS;
}

bool GaiaRequestAuthToken(talk_base::HttpClient* client,
                          const std::string& sid,
                          const std::string& lsid,
                          const std::string& service,
                          const GaiaServer& gaia_server) {
  std::string post_data;
  post_data += "SID=" + UrlEncodeString(sid);
  post_data += "&LSID=" + UrlEncodeString(lsid);
  post_data += "&service=" + service;
  post_data += "&Session=true";  // Creates two week cookie.

  client->reset();
  client->set_server(talk_base::SocketAddress(gaia_server.hostname(),
                                              gaia_server.port(), false));
  client->request().verb = talk_base::HV_POST;
  client->request().path = "/accounts/IssueAuthToken";
  client->request().setContent("application/x-www-form-urlencoded",
      new talk_base::MemoryStream(post_data.data(), post_data.size()));
  client->response().document.reset(new talk_base::MemoryStream);
  client->start();
  return true;
}

GaiaResponse GaiaParseAuthTokenResponse(const talk_base::HttpClient& client,
                                        std::string* auth_token) {
  if (client.response().scode != talk_base::HC_OK) {
    return GR_ERROR;
  }

  const talk_base::MemoryStream* stream =
      static_cast<const talk_base::MemoryStream*>(
          client.response().document.get());
  size_t length;
  stream->GetPosition(&length);
  while ((length > 0) && isspace(stream->GetBuffer()[length-1]))
    --length;
  auth_token->assign(stream->GetBuffer(), length);
  return auth_token->empty() ? GR_ERROR : GR_SUCCESS;
}

}  // namespace buzz
