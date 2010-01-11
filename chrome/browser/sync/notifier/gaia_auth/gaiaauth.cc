// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/thread.h"
#include "chrome/browser/sync/notifier/base/ssl_adapter.h"
#include "chrome/browser/sync/notifier/gaia_auth/gaiaauth.h"
#include "talk/base/asynchttprequest.h"
#include "talk/base/firewallsocketserver.h"
#include "talk/base/httpclient.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/socketadapters.h"
#include "talk/base/socketpool.h"
#include "talk/base/stringutils.h"
#include "talk/base/urlencode.h"
#include "talk/xmpp/saslcookiemechanism.h"
#include "talk/xmpp/saslplainmechanism.h"

namespace buzz {

static const int kGaiaAuthTimeoutMs = 30 * 1000;  // 30 sec

// Warning, this is externed.
GaiaServer g_gaia_server;

///////////////////////////////////////////////////////////////////////////////
// GaiaAuth::WorkerTask
///////////////////////////////////////////////////////////////////////////////

// GaiaAuth is NOT invoked during SASL authentication, but it is invoked even
// before XMPP login begins.  As a PreXmppAuth object, it is driven by
// XmppClient before the XMPP socket is opened.  The job of GaiaAuth is to goes
// out using HTTPS POST to grab cookies from GAIA.
//
// It is used by XmppClient. It grabs a SaslAuthenticator which knows how to
// play the cookie login.

class GaiaAuth::WorkerTask {
 public:
  WorkerTask(const std::string& username,
             const talk_base::CryptString& pass,
             const std::string& service,
             const talk_base::ProxyInfo& proxy,
             talk_base::FirewallManager* firewall,
             const std::string& token,
             const CaptchaAnswer& captcha_answer,
             bool obtain_auth,
             const std::string& user_agent,
             const std::string& signature,
             const std::string& token_service) :
    username_(username),
    pass_(pass),
    service_(service),
    proxy_(proxy),
    firewall_(firewall),
    success_(false),
    error_(true),
    error_code_(0),
    proxy_auth_required_(false),
    certificate_expired_(false),
    auth_token_(token),
    captcha_answer_(captcha_answer),
    fresh_auth_token_(false),
    obtain_auth_(obtain_auth),
    agent_(user_agent),
    signature_(signature),
    token_service_(token_service) {}

  ~WorkerTask() {}

  void DoWork(MessageLoop* parent_message_loop, Task* on_work_done_task) {
    DCHECK(parent_message_loop);
    DCHECK(on_work_done_task);
    LOG(INFO) << "GaiaAuth Begin";
    // Maybe we already have an auth token, then there is nothing to do.
    if (!auth_token_.empty()) {
      LOG(INFO) << "Reusing auth token:" << auth_token_;
      success_ = true;
      error_ = false;
    } else {
      // SocketFactory::CreateSSLAdapter() is called on the following
      // object somewhere in the depths of libjingle so we wrap it
      // with SSLAdapterSocketFactory to make sure it returns the
      // right SSLAdapter (see http://crbug.com/30721 ).
      notifier::SSLAdapterSocketFactory<talk_base::PhysicalSocketServer>
          physical;
      talk_base::SocketServer* ss = &physical;
      if (firewall_) {
        ss = new talk_base::FirewallSocketServer(ss, firewall_);
      }

      talk_base::SslSocketFactory factory(ss, agent_);
      factory.SetProxy(proxy_);
      if (g_gaia_server.use_ssl()) {
        factory.SetIgnoreBadCert(true);
        factory.UseSSL(g_gaia_server.hostname().c_str());
      }
      factory.SetLogging(talk_base::LS_VERBOSE, "GaiaAuth");

#if defined(OS_WIN)
      talk_base::ReuseSocketPool pool(&factory);
#else
      // On non-Windows platforms our SSL socket wrapper
      // implementation does not support restartable SSL sockets, so
      // we turn it off and use a NewSocketPool instead.  This means
      // that we create separate connections for each HTTP request but
      // this is okay since we do only two (in GaiaRequestSid() and
      // GaiaRequestAuthToken()).
      factory.SetUseRestartableSSLSockets(false);
      talk_base::NewSocketPool pool(&factory);
#endif
      talk_base::HttpClient http(agent_, &pool);

      talk_base::HttpMonitor monitor(ss);
      monitor.Connect(&http);

      // If we do not already have a SID, let's get one using our password.
      if (sid_.empty() || (auth_.empty() && obtain_auth_)) {
        GaiaRequestSid(&http, username_, pass_, signature_,
                       obtain_auth_ ? service_ : "", captcha_answer_,
                       g_gaia_server);
        // TODO(akalin): handle timeouts better; this can cause jank,
        // e.g. when we're waiting on this and the user stops syncing,
        // which causes the GaiaAuth object to be destroyed, which
        // waits on this. (bug: http://crbug.com/31981 )
        ss->Wait(kGaiaAuthTimeoutMs, true);

        error_code_ = monitor.error();   // Save off the error code.

        if (!monitor.done()) {
          LOG(INFO) << "GaiaAuth request timed out";
          goto Cleanup;
        } else if (monitor.error()) {
          LOG(INFO) << "GaiaAuth request error: " << monitor.error();
          if (monitor.error() == talk_base::HE_AUTH) {
            success_ = false;
            proxy_auth_required_ = true;
          } else if (monitor.error() == talk_base::HE_CERTIFICATE_EXPIRED) {
            success_ = false;
            certificate_expired_ = true;
          }
          goto Cleanup;
        } else {
          std::string captcha_token, captcha_url;
          switch (GaiaParseSidResponse(http, g_gaia_server,
                                       &captcha_token, &captcha_url,
                                       &sid_, &lsid_, &auth_)) {
          case GR_ERROR:
            goto Cleanup;

          case GR_UNAUTHORIZED:
            if (!captcha_url.empty()) {
              captcha_challenge_ = buzz::CaptchaChallenge(captcha_token,
                                                          captcha_url);
            }
            // We had no "error" - we were just unauthorized.
            error_ = false;
            error_code_ = 0;
            goto Cleanup;

          case GR_SUCCESS:
            break;
          }
        }
      }

      // If all we need is a SID, then we are done now.
      if (service_.empty() || obtain_auth_) {
        success_ = true;
        error_ = false;
        error_code_ = 0;
        goto Cleanup;
      }

      monitor.reset();
      GaiaRequestAuthToken(&http, sid_, lsid_, service_, g_gaia_server);
      ss->Wait(kGaiaAuthTimeoutMs, true);

      error_code_ = monitor.error();   // Save off the error code.

      if (!monitor.done()) {
        LOG(INFO) << "GaiaAuth request timed out";
      } else if (monitor.error()) {
        LOG(INFO) << "GaiaAuth request error: " << monitor.error();
        if (monitor.error() == talk_base::HE_AUTH) {
          success_ = false;
          proxy_auth_required_ = true;
        } else if (monitor.error() == talk_base::HE_CERTIFICATE_EXPIRED) {
          success_ = false;
          certificate_expired_ = true;
        }
      } else {
        if (GR_SUCCESS == GaiaParseAuthTokenResponse(http, &auth_token_)) {
          fresh_auth_token_ = true;
          success_ = true;
          error_ = false;
          error_code_ = 0;
        }
      }
    }

    // Done authenticating.

  Cleanup:
    LOG(INFO) << "GaiaAuth done";
    parent_message_loop->PostTask(FROM_HERE, on_work_done_task);
  }

  bool Succeeded() const { return success_; }
  bool HadError() const { return error_; }
  int GetError() const { return error_code_; }
  bool ProxyAuthRequired() const { return proxy_auth_required_; }
  bool CertificateExpired() const { return certificate_expired_; }
  const buzz::CaptchaChallenge& GetCaptchaChallenge() {
    return captcha_challenge_;
  }
  bool fresh_auth_token() const { return fresh_auth_token_; }

  talk_base::CryptString GetPassword() const { return pass_; }
  std::string GetSID() const { return sid_; }
  std::string GetAuth() const { return auth_; }
  std::string GetToken() const { return auth_token_; }
  std::string GetUsername() const { return username_; }
  std::string GetTokenService() const { return token_service_; }

 private:
  std::string username_;
  talk_base::CryptString pass_;
  std::string service_;
  talk_base::ProxyInfo proxy_;
  talk_base::FirewallManager * firewall_;
  bool done_;
  bool success_;
  bool error_;
  int error_code_;
  bool proxy_auth_required_;
  bool certificate_expired_;
  std::string sid_;
  std::string lsid_;
  std::string auth_;
  std::string auth_token_;
  buzz::CaptchaChallenge captcha_challenge_;
  CaptchaAnswer captcha_answer_;
  bool fresh_auth_token_;
  bool obtain_auth_;
  std::string agent_;
  std::string signature_;
  std::string token_service_;
};

}  // namespace buzz

// We outlive any runnable methods we create, so stub out
// RunnableMethodTraits for GaiaAuth and WorkerTask.

template <>
struct RunnableMethodTraits<buzz::GaiaAuth> {
  void RetainCallee(buzz::GaiaAuth* gaia_auth) {}
  void ReleaseCallee(buzz::GaiaAuth* gaia_auth) {}
};

template <>
struct RunnableMethodTraits<buzz::GaiaAuth::WorkerTask> {
  void RetainCallee(buzz::GaiaAuth::WorkerTask* gaia_auth) {}
  void ReleaseCallee(buzz::GaiaAuth::WorkerTask* gaia_auth) {}
};

namespace buzz {

///////////////////////////////////////////////////////////////////////////////
// GaiaAuth
///////////////////////////////////////////////////////////////////////////////

GaiaAuth::GaiaAuth(const std::string &user_agent, const std::string &sig)
    : agent_(user_agent), signature_(sig), firewall_(0),
      worker_thread_("GaiaAuth worker thread"), worker_task_(NULL),
      done_(false) {
}

GaiaAuth::~GaiaAuth() {
}

void GaiaAuth::StartPreXmppAuth(const buzz::Jid& jid,
                                const talk_base::SocketAddress& server,
                                const talk_base::CryptString& pass,
                                const std::string & auth_cookie) {
  InternalStartGaiaAuth(jid, server, pass, auth_cookie, "mail", false);
}

void GaiaAuth::StartTokenAuth(const buzz::Jid& jid,
                              const talk_base::CryptString& pass,
                              const std::string& service) {
  InternalStartGaiaAuth(jid, talk_base::SocketAddress(), pass, "", service,
                        false);
}

void GaiaAuth::StartAuth(const buzz::Jid& jid,
                         const talk_base::CryptString& pass,
                         const std::string & service) {
  InternalStartGaiaAuth(jid, talk_base::SocketAddress(), pass, "", service,
                        true);
}

void GaiaAuth::StartAuthFromSid(const buzz::Jid& jid,
                                const std::string& sid,
                                const std::string& service) {
  InternalStartGaiaAuth(jid, talk_base::SocketAddress(),
                        talk_base::CryptString(), sid, service, false);
}

void GaiaAuth::InternalStartGaiaAuth(const buzz::Jid& jid,
                                     const talk_base::SocketAddress& server,
                                     const talk_base::CryptString& pass,
                                     const std::string& token,
                                     const std::string& service,
                                     bool obtain_auth) {
  worker_task_.reset(
      new WorkerTask(jid.Str(), pass, service, proxy_, firewall_,
                     token, captcha_answer_, obtain_auth, agent_,
                     signature_, token_service_));
  MessageLoop* current_message_loop = MessageLoop::current();
  DCHECK(current_message_loop);
  Task* on_work_done_task = NewRunnableMethod(this, &GaiaAuth::OnAuthDone);
  LOG(INFO) << "GaiaAuth begin (async)";
  worker_thread_.Start();
  worker_thread_.message_loop()->PostTask(
      FROM_HERE, NewRunnableMethod(worker_task_.get(),
                                   &WorkerTask::DoWork,
                                   current_message_loop,
                                   on_work_done_task));
}

void GaiaAuth::OnAuthDone() {
  LOG(INFO) << "GaiaAuth done (async)";
  worker_thread_.Stop();
  done_ = true;

  if (worker_task_->fresh_auth_token()) {
    SignalFreshAuthCookie(worker_task_->GetToken());
  }
  if (worker_task_->ProxyAuthRequired()) {
    SignalAuthenticationError();
  }
  if (worker_task_->CertificateExpired()) {
    SignalCertificateExpired();
  }
  SignalAuthDone();
}

std::string GaiaAuth::ChooseBestSaslMechanism(
    const std::vector<std::string> & mechanisms, bool encrypted) {
  if (!done_)
    return "";

  std::vector<std::string>::const_iterator it;

  // A token is the weakest auth - 15s, service-limited, so prefer it.
  it = std::find(mechanisms.begin(), mechanisms.end(), "X-GOOGLE-TOKEN");
  if (it != mechanisms.end())
    return "X-GOOGLE-TOKEN";

  // A cookie is the next weakest - 14 days.
  it = std::find(mechanisms.begin(), mechanisms.end(), "X-GOOGLE-COOKIE");
  if (it != mechanisms.end())
    return "X-GOOGLE-COOKIE";

  // Never pass @google.com passwords without encryption!!
  if (!encrypted &&
      buzz::Jid(worker_task_->GetUsername()).domain() == "google.com") {
    return "";
  }

  // As a last resort, use plain authentication.
  if (buzz::Jid(worker_task_->GetUsername()).domain() != "google.com") {
    it = std::find(mechanisms.begin(), mechanisms.end(), "PLAIN");
    if (it != mechanisms.end())
      return "PLAIN";
  }

  // No good mechanism found.
  return "";
}

buzz::SaslMechanism* GaiaAuth::CreateSaslMechanism(
    const std::string& mechanism) {
  if (!done_) {
    return NULL;
  }

  if (mechanism == "X-GOOGLE-TOKEN") {
    return new buzz::SaslCookieMechanism(
        mechanism,
        worker_task_->GetUsername(),
        worker_task_->GetToken(),
        worker_task_->GetTokenService());
  }

  if (mechanism == "X-GOOGLE-COOKIE") {
    return new buzz::SaslCookieMechanism(
        "X-GOOGLE-COOKIE",
        worker_task_->GetUsername(),
        worker_task_->GetSID(),
        worker_task_->GetTokenService());
  }

  if (mechanism == "PLAIN") {
    return new buzz::SaslPlainMechanism(buzz::Jid(worker_task_->GetUsername()),
                                        worker_task_->GetPassword());
  }

  // Oh well - none of the above.
  return NULL;
}

std::string GaiaAuth::CreateAuthenticatedUrl(
    const std::string & continue_url, const std::string & service) {
  if (!done_ || worker_task_->GetToken().empty())
    return "";

  std::string url;
  // Note that http_prefix always ends with a "/".
  url += g_gaia_server.http_prefix()
         + "accounts/TokenAuth?auth="
         // Do not URL encode - GAIA doesn't like that.
         + worker_task_->GetToken();
  url += "&service=" + service;
  url += "&continue=" + UrlEncodeString(continue_url);
  url += "&source=" +  signature_;
  return url;
}

std::string GaiaAuth::GetAuthCookie() {
  assert(IsAuthDone() && IsAuthorized());
  if (!done_ || !worker_task_->Succeeded()) {
    return "";
  }
  return worker_task_->GetToken();
}

std::string GaiaAuth::GetAuth() {
  assert(IsAuthDone() && IsAuthorized());
  if (!done_ || !worker_task_->Succeeded()) {
    return "";
  }
  return worker_task_->GetAuth();
}

std::string GaiaAuth::GetSID() {
  assert(IsAuthDone() && IsAuthorized());
  if (!done_ || !worker_task_->Succeeded()) {
    return "";
  }
  return worker_task_->GetSID();
}

bool GaiaAuth::IsAuthDone() {
  return done_;
}

bool GaiaAuth::IsAuthorized() {
  return done_ && worker_task_ != NULL && worker_task_->Succeeded();
}

bool GaiaAuth::HadError() {
  return done_ && worker_task_ != NULL && worker_task_->HadError();
}

int GaiaAuth::GetError() {
  if (done_ && worker_task_ != NULL) {
    return worker_task_->GetError();
  }
  return 0;
}

buzz::CaptchaChallenge GaiaAuth::GetCaptchaChallenge() {
  if (!done_ || worker_task_->Succeeded()) {
    return buzz::CaptchaChallenge();
  }
  return worker_task_->GetCaptchaChallenge();
}

}  // namespace buzz
