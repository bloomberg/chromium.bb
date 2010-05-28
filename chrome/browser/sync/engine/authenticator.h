// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The authenticator is a cross-platform class that handles authentication for
// the sync client.
//
// Current State:
// The authenticator is currently only used to authenticate tokens using the
// newer protocol buffer request.

#ifndef CHROME_BROWSER_SYNC_ENGINE_AUTHENTICATOR_H_
#define CHROME_BROWSER_SYNC_ENGINE_AUTHENTICATOR_H_

#include <string>

#include "base/basictypes.h"
#include "base/port.h"

namespace sync_pb {
class UserIdentification;
}

namespace browser_sync {

class ServerConnectionManager;
class UserSettings;

class Authenticator {
 public:
  // Single return enum.
  enum AuthenticationResult {
    SUCCESS = 0,
    // We couldn't log on because we don't have saved credentials.
    NO_SAVED_CREDENTIALS,
    // We can't reach auth server (i.e. we're offline or server's down).
    NOT_CONNECTED,
    // Server's up, but we're down.
    SERVICE_DOWN,
    // We contacted the server, but the response didn't make sense.
    CORRUPT_SERVER_RESPONSE,
    // Bad username/password.
    BAD_CREDENTIALS,
    // Credentials are fine, but the user hasn't signed up.
    USER_NOT_ACTIVATED,

    // Return values for internal use.

    // We will never return this to the user unless they call AuthenticateToken
    // directly. Other auth functions retry and then return
    // CORRUPT_SERVER_RESPONSE.
    // TODO(sync): Implement retries.
    BAD_AUTH_TOKEN,
    // We should never return this, it's a placeholder during development.
    // TODO(sync): Remove this
    UNSPECIFIC_ERROR_RETURN,
  };

  // Constructor. This class will keep the connection authenticated.
  // TODO(sync): Make it work as described.
  // TODO(sync): Require a UI callback mechanism.
  Authenticator(ServerConnectionManager* manager, UserSettings* settings);

  // Constructor for a simple authenticator used for programmatic login from
  // test programs.
  explicit Authenticator(ServerConnectionManager* manager);

  // This version of Authenticate tries to use saved credentials, if we have
  // any.
  AuthenticationResult Authenticate();

  // We save the username and password in memory (if given) so we
  // can refresh the long-lived auth token if it expires.
  // Also we save a 10-bit hash of the password to allow offline login.
  AuthenticationResult Authenticate(std::string username, std::string password);

  // A version of the auth token to authenticate cookie portion of
  // authentication. It uses the new proto buffer based call instead of the HTTP
  // GET based one we currently use.
  // Can return one of SUCCESS, SERVICE_DOWN, CORRUPT_SERVER_RESPONSE,
  // USER_NOT_ACTIVATED or BAD_AUTH_TOKEN. See above for the meaning of these
  // values.
  // TODO(sync): Make this function private when we're done.
  AuthenticationResult AuthenticateToken(std::string auth_token);

  const char* display_email() const { return display_email_.c_str(); }
  const char* display_name() const { return display_name_.c_str(); }
 private:
  // Stores the information in the UserIdentification returned from the server.
  AuthenticationResult HandleSuccessfulTokenRequest(
      const sync_pb::UserIdentification* user);
  // The server connection manager that we're looking after.
  ServerConnectionManager* server_connection_manager_;
  // Returns SUCCESS or the value that should be returned to the user.
  std::string display_email_;
  std::string display_name_;
  std::string obfuscated_id_;
  UserSettings* const settings_;
  DISALLOW_COPY_AND_ASSIGN(Authenticator);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_ENGINE_AUTHENTICATOR_H_
