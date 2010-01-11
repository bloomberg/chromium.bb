// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Gaia auth code for XMPP notifier support. This should be merged with the
// other gaia auth file when we have time.

#ifndef CHROME_BROWSER_SYNC_NOTIFIER_GAIA_AUTH_GAIAAUTH_H_
#define CHROME_BROWSER_SYNC_NOTIFIER_GAIA_AUTH_GAIAAUTH_H_

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/thread.h"
#include "chrome/browser/sync/notifier/gaia_auth/gaiahelper.h"
#include "talk/base/cryptstring.h"
#include "talk/base/messagequeue.h"
#include "talk/base/proxyinfo.h"
#include "talk/xmpp/prexmppauth.h"

namespace talk_base {
class FirewallManager;
}

namespace buzz {

///////////////////////////////////////////////////////////////////////////////
// GaiaAuth
///////////////////////////////////////////////////////////////////////////////

class GaiaAuth : public PreXmppAuth {
 public:
  GaiaAuth(const std::string& user_agent, const std::string& signature);
  virtual ~GaiaAuth();

  void set_proxy(const talk_base::ProxyInfo& proxy) {
    proxy_ = proxy;
  }
  void set_firewall(talk_base::FirewallManager* firewall) {
    firewall_ = firewall;
  }
  void set_captcha_answer(const CaptchaAnswer& captcha_answer) {
    captcha_answer_ = captcha_answer;
  }

  // From inside XMPP login, this is called.
  virtual void StartPreXmppAuth(const buzz::Jid& jid,
                                const talk_base::SocketAddress& server,
                                const talk_base::CryptString& pass,
                                const std::string& auth_cookie);

  void StartTokenAuth(const buzz::Jid& jid,
                      const talk_base::CryptString& pass,
                      const std::string& service);

  // This is used when calling GetAuth().
  void StartAuth(const buzz::Jid& jid,
                 const talk_base::CryptString& pass,
                 const std::string& service);

  // This is used when bootstrapping from a download page.
  void StartAuthFromSid(const buzz::Jid& jid,
                        const std::string& sid,
                        const std::string& service);

  virtual bool IsAuthDone();
  virtual bool IsAuthorized();
  virtual bool HadError();
  virtual int GetError();
  virtual buzz::CaptchaChallenge GetCaptchaChallenge();
  // Returns the auth token that can be sent in an url param to gaia in order
  // to generate an auth cookie.
  virtual std::string GetAuthCookie();

  // Returns the auth cookie for gaia.
  std::string GetAuth();
  std::string GetSID();

  // Sets / gets the token service to use.
  std::string token_service() const { return token_service_; }
  void set_token_service(const std::string& token_service) {
    token_service_ = token_service;
  }

  virtual std::string ChooseBestSaslMechanism(
      const std::vector<std::string>& mechanisms, bool encrypted);
  virtual buzz::SaslMechanism* CreateSaslMechanism(
      const std::string& mechanism);

  std::string CreateAuthenticatedUrl(const std::string& continue_url,
      const std::string& service);

  sigslot::signal0<> SignalAuthenticationError;
  sigslot::signal0<> SignalCertificateExpired;
  sigslot::signal1<const std::string&> SignalFreshAuthCookie;

  // Needs to be public for the RunnableMethodTraits specialization in
  // gaiaauth.cc.
  class WorkerTask;

 private:
  void OnAuthDone();

  void InternalStartGaiaAuth(const buzz::Jid& jid,
                             const talk_base::SocketAddress& server,
                             const talk_base::CryptString& pass,
                             const std::string& sid,
                             const std::string& service,
                             bool obtain_auth);

  std::string agent_;
  std::string signature_;
  talk_base::ProxyInfo proxy_;
  talk_base::FirewallManager* firewall_;
  base::Thread worker_thread_;
  scoped_ptr<WorkerTask> worker_task_;
  bool done_;

  CaptchaAnswer captcha_answer_;
  std::string token_service_;
};

///////////////////////////////////////////////////////////////////////////////
// Globals
///////////////////////////////////////////////////////////////////////////////

extern GaiaServer g_gaia_server;

///////////////////////////////////////////////////////////////////////////////

}  // namespace buzz

#endif  // CHROME_BROWSER_SYNC_NOTIFIER_GAIA_AUTH_GAIAAUTH_H_
