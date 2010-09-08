// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/authenticator.h"

#include "chrome/browser/sync/engine/net/server_connection_manager.h"
#include "chrome/browser/sync/engine/syncproto.h"
#include "chrome/browser/sync/protocol/sync.pb.h"
#include "chrome/browser/sync/util/user_settings.h"
#include "chrome/common/deprecated/event_sys-inl.h"
#include "chrome/common/net/gaia/gaia_authenticator.h"

namespace browser_sync {

using std::string;

Authenticator::Authenticator(ServerConnectionManager* manager,
                             UserSettings* settings)
    : server_connection_manager_(manager), settings_(settings) {
}

Authenticator::Authenticator(ServerConnectionManager* manager)
    : server_connection_manager_(manager), settings_(NULL) {
}

Authenticator::AuthenticationResult Authenticator::Authenticate() {
  // TODO(sync): Pull and work with saved credentials.
  return NO_SAVED_CREDENTIALS;
}

Authenticator::AuthenticationResult Authenticator::Authenticate(
    string username, string password) {
  // TODO(sync): need to figure out if this routine is used anywhere other
  // than the test code.
  gaia::GaiaAuthenticator auth_service("ChromiumBrowser", "chromiumsync",
      "https://www.google.com:443/accounts/ClientLogin");
  auth_service.set_message_loop(MessageLoop::current());
  if (!auth_service.Authenticate(username, password)) {
    return UNSPECIFIC_ERROR_RETURN;
  }
  CHECK(!auth_service.auth_token().empty());
  return AuthenticateToken(auth_service.auth_token());
}

COMPILE_ASSERT(sync_pb::ClientToServerResponse::ErrorType_MAX == 7,
               client_to_server_response_errors_changed);

Authenticator::AuthenticationResult Authenticator::HandleSuccessfulTokenRequest(
    const sync_pb::UserIdentification* user) {
  display_email_ = user->has_email() ? user->email() : "";
  display_name_ = user->has_display_name() ? user->display_name() : "";
  obfuscated_id_ = user->has_obfuscated_id() ? user->obfuscated_id() : "";
  return SUCCESS;
}

Authenticator::AuthenticationResult Authenticator::AuthenticateToken(
    string auth_token) {
  ClientToServerMessage client_to_server_message;
  // Used to be required for all requests.
  client_to_server_message.set_share("");
  client_to_server_message.set_message_contents(
      ClientToServerMessage::AUTHENTICATE);

  string tx, rx;
  client_to_server_message.SerializeToString(&tx);
  HttpResponse http_response;

  ServerConnectionManager::PostBufferParams params =
    { tx, &rx, &http_response };
  ScopedServerStatusWatcher watch(server_connection_manager_, &http_response);
  if (!server_connection_manager_->PostBufferWithAuth(&params, auth_token,
                                                      &watch)) {
    LOG(WARNING) << "Error posting from authenticator:" << http_response;
    return SERVICE_DOWN;
  }
  sync_pb::ClientToServerResponse response;
  if (!response.ParseFromString(rx))
    return CORRUPT_SERVER_RESPONSE;

  switch (response.error_code()) {
    case sync_pb::ClientToServerResponse::SUCCESS:
      if (response.has_authenticate() && response.authenticate().has_user())
        return HandleSuccessfulTokenRequest(&response.authenticate().user());
      // TODO:(sync) make this CORRUPT_SERVER_RESPONSE when all servers are
      // returning user identification at login time.
      return SUCCESS;
    case sync_pb::ClientToServerResponse::USER_NOT_ACTIVATED:
      return USER_NOT_ACTIVATED;
    case sync_pb::ClientToServerResponse::AUTH_INVALID:
    case sync_pb::ClientToServerResponse::AUTH_EXPIRED:
      // TODO(tim): This is an egregious layering violation (bug 35060).
      http_response.server_status = HttpResponse::SYNC_AUTH_ERROR;
      return BAD_AUTH_TOKEN;
    // should never happen (no birthday in this request).
    case sync_pb::ClientToServerResponse::NOT_MY_BIRTHDAY:
    // should never happen (auth isn't throttled).
    case sync_pb::ClientToServerResponse::THROTTLED:
    // should never happen (only for stores).
    case sync_pb::ClientToServerResponse::ACCESS_DENIED:
    // should never happen (only sent on get updates / commit)
    case sync_pb::ClientToServerResponse::CLEAR_PENDING:
    default:
      LOG(ERROR) << "Corrupt Server packet received by auth, error code " <<
        response.error_code();
      return CORRUPT_SERVER_RESPONSE;
  }
}

}  // namespace browser_sync
