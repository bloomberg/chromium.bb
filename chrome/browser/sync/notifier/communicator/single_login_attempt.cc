// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/sync/notifier/communicator/single_login_attempt.h"

#include "base/logging.h"
#include "chrome/browser/sync/notifier/communicator/connection_options.h"
#include "chrome/browser/sync/notifier/communicator/connection_settings.h"
#include "chrome/browser/sync/notifier/communicator/const_communicator.h"
#include "chrome/browser/sync/notifier/communicator/login_failure.h"
#include "chrome/browser/sync/notifier/communicator/login_settings.h"
#include "chrome/browser/sync/notifier/communicator/product_info.h"
#include "chrome/browser/sync/notifier/communicator/xmpp_connection_generator.h"
#include "chrome/browser/sync/notifier/communicator/xmpp_socket_adapter.h"
#include "chrome/browser/sync/notifier/gaia_auth/gaiaauth.h"
#include "talk/base/asynchttprequest.h"
#include "talk/base/firewallsocketserver.h"
#include "talk/base/signalthread.h"
#include "talk/base/taskrunner.h"
#include "talk/base/winsock_initializer.h"
#include "talk/xmllite/xmlelement.h"
#include "talk/xmpp/prexmppauth.h"
#include "talk/xmpp/xmppclient.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/xmppconstants.h"

namespace notifier {

static void FillProxyInfo(const buzz::XmppClientSettings& xcs,
                          talk_base::ProxyInfo* proxy) {
  ASSERT(proxy != NULL);
  proxy->type = xcs.proxy();
  proxy->address.SetIP(xcs.proxy_host());
  proxy->address.SetPort(xcs.proxy_port());
  if (xcs.use_proxy_auth()) {
    proxy->username = xcs.proxy_user();
    proxy->password = xcs.proxy_pass();
  }
}

static void GetClientErrorInformation(
    buzz::XmppClient* client,
    buzz::XmppEngine::Error* error,
    int* subcode,
    buzz::XmlElement** stream_error,
    buzz::CaptchaChallenge* captcha_challenge) {
  ASSERT(client != NULL);
  ASSERT(error && subcode && stream_error && captcha_challenge);

  *error = client->GetError(subcode);
  *captcha_challenge = client->GetCaptchaChallenge();

  *stream_error = NULL;
  if (*error == buzz::XmppEngine::ERROR_STREAM) {
    const buzz::XmlElement* error_element = client->GetStreamError();
    if (error_element) {
      *stream_error = new buzz::XmlElement(*error_element);
    }
  }
}

SingleLoginAttempt::SingleLoginAttempt(talk_base::Task* parent,
                                       LoginSettings* login_settings,
                                       bool successful_connection)
    : talk_base::Task(parent),
      state_(buzz::XmppEngine::STATE_NONE),
      code_(buzz::XmppEngine::ERROR_NONE),
      subcode_(0),
      need_authentication_(false),
      certificate_expired_(false),
      cookie_refreshed_(false),
      successful_connection_(successful_connection),
      login_settings_(login_settings),
      client_(NULL) {
#if defined(OS_WIN)
  talk_base::EnsureWinsockInit();
#endif
  connection_generator_.reset(new XmppConnectionGenerator(
                                  this,
                                  &login_settings_->connection_options(),
                                  login_settings_->proxy_only(),
                                  login_settings_->server_list(),
                                  login_settings_->server_count()));

  connection_generator_->SignalExhaustedSettings.connect(
      this,
      &SingleLoginAttempt::OnAttemptedAllConnections);
  connection_generator_->SignalNewSettings.connect(
      this,
      &SingleLoginAttempt::DoLogin);
}

SingleLoginAttempt::~SingleLoginAttempt() {
  // If this assertion goes off, it means that "Stop()" didn't get called like
  // it should have been.
  ASSERT(client_ == NULL);
}

bool SingleLoginAttempt::auto_reconnect() const {
  return login_settings_->connection_options().auto_reconnect();
}

const talk_base::ProxyInfo& SingleLoginAttempt::proxy() const {
  ASSERT(connection_generator_.get());
  return connection_generator_->proxy();
}

int SingleLoginAttempt::ProcessStart() {
  ASSERT(GetState() == talk_base::Task::STATE_START);
  connection_generator_->StartGenerating();

  // After being started, this class is callback driven and does signaling from
  // those callbacks (with checks to see if it is done if it may be called back
  // from something that isn't a child task).
  return talk_base::Task::STATE_BLOCKED;
}

void SingleLoginAttempt::Stop() {
  ClearClient();
  talk_base::Task::Stop();

  // No more signals should happen after being stopped. This is needed because
  // some of these signals happen due to other components doing signaling which
  // may continue running even though this task is stopped.
  SignalUnexpectedDisconnect.disconnect_all();
  SignalRedirect.disconnect_all();
  SignalLoginFailure.disconnect_all();
  SignalNeedAutoReconnect.disconnect_all();
  SignalClientStateChange.disconnect_all();
}

void SingleLoginAttempt::OnAttemptedAllConnections(
    bool successfully_resolved_dns,
    int first_dns_error) {

  // Maybe we needed proxy authentication?
  if (need_authentication_) {
    LoginFailure failure(LoginFailure::PROXY_AUTHENTICATION_ERROR);
    SignalLoginFailure(failure);
    return;
  }

  if (certificate_expired_) {
    LoginFailure failure(LoginFailure::CERTIFICATE_EXPIRED_ERROR);
    SignalLoginFailure(failure);
    return;
  }

  if (!successfully_resolved_dns) {
    code_ = buzz::XmppEngine::ERROR_SOCKET;
    subcode_ = first_dns_error;
  }

  LOG(INFO) << "Connection failed with error " << code_;

  // We were connected and we had a problem.
  if (successful_connection_ && auto_reconnect()) {
    SignalNeedAutoReconnect();
    // Expect to be deleted at this point.
    return;
  }

  DiagnoseConnectionError();
}

void SingleLoginAttempt::UseNextConnection() {
  ASSERT(connection_generator_.get() != NULL);
  ClearClient();
  connection_generator_->UseNextConnection();
}

void SingleLoginAttempt::UseCurrentConnection() {
  ASSERT(connection_generator_.get() != NULL);
  ClearClient();
  connection_generator_->UseCurrentConnection();
}

void SingleLoginAttempt::DoLogin(
    const ConnectionSettings& connection_settings) {
  if (client_) {
    return;
  }

  buzz::XmppClientSettings client_settings;
  // Set the user settings portion.
  *static_cast<buzz::XmppClientSettings*>(&client_settings) =
      login_settings_->user_settings();
  // Fill in the rest of the client settings.
  connection_settings.FillXmppClientSettings(&client_settings);

  client_ = new buzz::XmppClient(this);
  SignalLogInput.repeat(client_->SignalLogInput);
  SignalLogOutput.repeat(client_->SignalLogOutput);

  // Listen for connection progress.
  client_->SignalStateChange.connect(this,
                                     &SingleLoginAttempt::OnClientStateChange);

  // Transition to "start".
  OnClientStateChange(buzz::XmppEngine::STATE_START);
  // Start connecting.
  client_->Connect(client_settings, login_settings_->lang(),
                   CreateSocket(client_settings),
                   CreatePreXmppAuth(client_settings));
  client_->Start();
}

void SingleLoginAttempt::OnAuthenticationError() {
  // We can check this flag later if all connection options fail.
  need_authentication_ = true;
}

void SingleLoginAttempt::OnCertificateExpired() {
  // We can check this flag later if all connection options fail.
  certificate_expired_ = true;
}

buzz::AsyncSocket* SingleLoginAttempt::CreateSocket(
    const buzz::XmppClientSettings& xcs) {
  bool allow_unverified_certs =
      login_settings_->connection_options().allow_unverified_certs();
  XmppSocketAdapter* adapter = new XmppSocketAdapter(xcs,
                                                     allow_unverified_certs);
  adapter->SignalAuthenticationError.connect(
      this,
      &SingleLoginAttempt::OnAuthenticationError);
  if (login_settings_->firewall()) {
    adapter->set_firewall(true);
  }
  return adapter;
}

buzz::PreXmppAuth* SingleLoginAttempt::CreatePreXmppAuth(
    const buzz::XmppClientSettings& xcs) {
  if (login_settings_->no_gaia_auth())
    return NULL;

  // For GMail, use Gaia preauthentication over HTTP.
  buzz::GaiaAuth* auth = new buzz::GaiaAuth(GetUserAgentString(),
                                            GetProductSignature());
  auth->SignalAuthenticationError.connect(
      this,
      &SingleLoginAttempt::OnAuthenticationError);
  auth->SignalCertificateExpired.connect(
      this,
      &SingleLoginAttempt::OnCertificateExpired);
  auth->SignalFreshAuthCookie.connect(
      this,
      &SingleLoginAttempt::OnFreshAuthCookie);
  auth->set_token_service(xcs.token_service());

  talk_base::ProxyInfo proxy;
  FillProxyInfo(xcs, &proxy);
  auth->set_proxy(proxy);
  auth->set_firewall(login_settings_->firewall());
  return auth;
}

void SingleLoginAttempt::OnFreshAuthCookie(const std::string& auth_cookie) {
  // Remember this is a fresh cookie.
  cookie_refreshed_ = true;

  // TODO(sync): do the cookie logic (part of which is in the #if 0 below).

  // The following code is what PhoneWindow does for the equivalent method.
#if 0
  // Save cookie
  AccountInfo current(account_history_.current());
  current.set_auth_cookie(auth_cookie);
  account_history_.set_current(current);

  // Calc next time to refresh cookie, between 5 and 10 days. The cookie has
  // 14 days of life; this gives at least 4 days of retries before the current
  // cookie expires, maximizing the chance of having a valid cookie next time
  // the connection servers go down.
  FTULL now;

  // NOTE: The following line is win32.  Address this when implementing this
  // code (doing "the cookie logic").
  GetSystemTimeAsFileTime(&(now.ft));
  ULONGLONG five_days = (ULONGLONG)10000 * 1000 * 60 * 60 * 24 * 5;  // 5 days
  ULONGLONG random = (ULONGLONG)10000 *          // get to 100 ns units
      ((rand() % (5 * 24 * 60)) * (60 * 1000) +  // random min. in 5 day period
      (rand() % 1000) * 60);                     // random 1/1000th of a minute
  next_cookie_refresh_ = now.ull + five_days + random;  // 5-10 days
#endif
}

void SingleLoginAttempt::DiagnoseConnectionError() {
  switch (code_) {
    case buzz::XmppEngine::ERROR_MISSING_USERNAME:
    case buzz::XmppEngine::ERROR_NETWORK_TIMEOUT:
    case buzz::XmppEngine::ERROR_DOCUMENT_CLOSED:
    case buzz::XmppEngine::ERROR_BIND:
    case buzz::XmppEngine::ERROR_AUTH:
    case buzz::XmppEngine::ERROR_TLS:
    case buzz::XmppEngine::ERROR_UNAUTHORIZED:
    case buzz::XmppEngine::ERROR_VERSION:
    case buzz::XmppEngine::ERROR_STREAM:
    case buzz::XmppEngine::ERROR_XML:
    case buzz::XmppEngine::ERROR_NONE:
    default: {
      LoginFailure failure(LoginFailure::XMPP_ERROR, code_, subcode_);
      SignalLoginFailure(failure);
      return;
    }

      // The following errors require diagnosistics:
      // * spurious close of connection
      // * socket errors after auth
    case buzz::XmppEngine::ERROR_CONNECTION_CLOSED:
    case buzz::XmppEngine::ERROR_SOCKET:
      break;
  }

  talk_base::AsyncHttpRequest *http_request =
    new talk_base::AsyncHttpRequest(GetUserAgentString());
  http_request->set_host("www.google.com");
  http_request->set_port(80);
  http_request->set_secure(false);
  http_request->request().path = "/";
  http_request->request().verb = talk_base::HV_GET;

  talk_base::ProxyInfo proxy;
  ASSERT(connection_generator_.get() != NULL);
  if (connection_generator_.get()) {
    proxy = connection_generator_->proxy();
  }
  http_request->set_proxy(proxy);
  http_request->set_firewall(login_settings_->firewall());

  http_request->SignalWorkDone.connect(this,
                                       &SingleLoginAttempt::OnHttpTestDone);
  http_request->Start();
  http_request->Release();
}

void SingleLoginAttempt::OnHttpTestDone(talk_base::SignalThread* thread) {
  ASSERT(thread != NULL);

  talk_base::AsyncHttpRequest* request =
    static_cast<talk_base::AsyncHttpRequest*>(thread);

  if (request->response().scode == 200) {
    // We were able to do an HTTP GET of www.google.com:80

    //
    // The original error should be reported
    //
    LoginFailure failure(LoginFailure::XMPP_ERROR, code_, subcode_);
    SignalLoginFailure(failure);
    return;
  }

  // Otherwise lets transmute the error into ERROR_SOCKET, and put the subcode
  // as an indicator of what we think the problem might be.

#if 0
  // TODO(sync): determine if notifier has an analogous situation.

  //
  // We weren't able to do an HTTP GET of www.google.com:80
  //
  GAutoupdater::Version version_logged_in(g_options.version_logged_in());
  GAutoupdater::Version version_installed(GetProductVersion().c_str());
  if (version_logged_in < version_installed) {
    //
    // Google Talk has been updated and can no longer connect to the Google
    // Talk Service. Your firewall is probably not allowing the new version of
    // Google Talk to connect to the internet. Please adjust your firewall
    // settings to allow the new version of Google Talk to connect to the
    // internet.
    //
    // We'll use the "error=1" to help figure this out for now.
    //
    LoginFailure failure(LoginFailure::XMPP_ERROR,
                         buzz::XmppEngine::ERROR_SOCKET,
                         1);
    SignalLoginFailure(failure);
    return;
  }
#endif

  //
  // Any other checking we can add here?
  //

  //
  // Google Talk is unable to use your internet connection. Either your network
  // isn't configured or Google Talk is being blocked by a local firewall.
  //
  // We'll use the "error=0" to help figure this out for now
  //
  LoginFailure failure(LoginFailure::XMPP_ERROR,
                       buzz::XmppEngine::ERROR_SOCKET,
                       0);
  SignalLoginFailure(failure);
}

void SingleLoginAttempt::OnClientStateChange(buzz::XmppEngine::State state) {
  if (state_ == state)
    return;

  buzz::XmppEngine::State previous_state = state_;
  state_ = state;

  switch (state) {
    case buzz::XmppEngine::STATE_NONE:
    case buzz::XmppEngine::STATE_START:
    case buzz::XmppEngine::STATE_OPENING:
      // Do nothing.
      break;
    case buzz::XmppEngine::STATE_OPEN:
      successful_connection_ = true;
      break;
    case buzz::XmppEngine::STATE_CLOSED:
      OnClientStateChangeClosed(previous_state);
      break;
  }
  SignalClientStateChange(state);
  if (state_ == buzz::XmppEngine::STATE_CLOSED) {
    OnClientStateChange(buzz::XmppEngine::STATE_NONE);
  }
}

void SingleLoginAttempt::ClearClient() {
  if (client_ != NULL) {
    client_->Disconnect();

    // If this assertion goes off, it means that the disconnect didn't occur
    // properly.  See SingleLoginAttempt::OnClientStateChange,
    // case XmppEngine::STATE_CLOSED
    ASSERT(client_ == NULL);
  }
}

void SingleLoginAttempt::OnClientStateChangeClosed(
    buzz::XmppEngine::State previous_state) {
  buzz::XmppEngine::Error error = buzz::XmppEngine::ERROR_NONE;
  int error_subcode = 0;
  buzz::CaptchaChallenge captcha_challenge;
  buzz::XmlElement* stream_error_ptr;
  GetClientErrorInformation(client_,
                            &error,
                            &error_subcode,
                            &stream_error_ptr,
                            &captcha_challenge);
  scoped_ptr<buzz::XmlElement> stream_error(stream_error_ptr);

  client_->SignalStateChange.disconnect(this);
  client_ = NULL;

  if (error == buzz::XmppEngine::ERROR_NONE) {
    SignalLogoff();
    return;
  } else if (previous_state == buzz::XmppEngine::STATE_OPEN) {
    // Handler should attempt reconnect.
    SignalUnexpectedDisconnect();
    return;
  } else {
    HandleConnectionError(error, error_subcode, stream_error.get(),
                          captcha_challenge);
  }
}

void SingleLoginAttempt::HandleConnectionPasswordError(
    const buzz::CaptchaChallenge& captcha_challenge) {
  LOG(INFO) << "SingleLoginAttempt::HandleConnectionPasswordError";

  // Clear the auth cookie.
  std::string current_auth_cookie =
      login_settings_->user_settings().auth_cookie();
  login_settings_->modifiable_user_settings()->set_auth_cookie("");
  // If there was an auth cookie and it was the same as the last auth cookie,
  // then it is a stale cookie. Retry login.
  if (!current_auth_cookie.empty() && !cookie_refreshed_) {
    UseCurrentConnection();
    return;
  }

  LoginFailure failure(LoginFailure::XMPP_ERROR, code_, subcode_,
                       captcha_challenge);
  SignalLoginFailure(failure);
}

void SingleLoginAttempt::HandleConnectionError(
    buzz::XmppEngine::Error code,
    int subcode,
    const buzz::XmlElement* stream_error,
    const buzz::CaptchaChallenge& captcha_challenge) {
  LOG(INFO) << "(" << code << ", " << subcode << ")";

  // Save off the error code information, so we can use it to tell the user
  // what went wrong if all else fails.
  code_ = code;
  subcode_ = subcode;
  if ((code_ == buzz::XmppEngine::ERROR_UNAUTHORIZED) ||
      (code_ == buzz::XmppEngine::ERROR_MISSING_USERNAME)) {
    // There was a problem with credentials (username/password).
    HandleConnectionPasswordError(captcha_challenge);
    return;
  }

  // Unexpected disconnect,
  // Unreachable host,
  // Or internal server binding error -
  // All these are temporary problems, so continue reconnecting.

  // GaiaAuth signals this directly via SignalCertificateExpired, but
  // SChannelAdapter propagates the error through SocketWindow as a socket
  // error.
  if (code_ == buzz::XmppEngine::ERROR_SOCKET &&
      subcode_ == SEC_E_CERT_EXPIRED) {
    certificate_expired_ = true;
  }

  login_settings_->modifiable_user_settings()->set_resource("");

  // Look for stream::error server redirection stanza "see-other-host".
  if (stream_error) {
    const buzz::XmlElement* other =
        stream_error->FirstNamed(buzz::QN_XSTREAM_SEE_OTHER_HOST);
    if (other) {
      const buzz::XmlElement* text =
          stream_error->FirstNamed(buzz::QN_XSTREAM_TEXT);
      if (text) {
        // Yep, its a "stream:error" with "see-other-host" text, let's parse
        // out the server:port, and then reconnect with that.
        const std::string& redirect = text->BodyText();
        size_t colon = redirect.find(":");
        int redirect_port = kDefaultXmppPort;
        std::string redirect_server;
        if (colon == std::string::npos) {
          redirect_server = redirect;
        } else {
          redirect_server = redirect.substr(0, colon);
          const std::string& port_text = redirect.substr(colon + 1);
          std::istringstream ist(port_text);
          ist >> redirect_port;
        }
        // We never allow a redirect to port 0.
        if (redirect_port == 0) {
          redirect_port = kDefaultXmppPort;
        }
        SignalRedirect(redirect_server, redirect_port);
        // May be deleted at this point.
        return;
      }
    }
  }

  ASSERT(connection_generator_.get() != NULL);
  if (!connection_generator_.get()) {
    return;
  }

  // Iterate to the next possible connection (still trying to connect).
  UseNextConnection();
}

}  // namespace notifier
