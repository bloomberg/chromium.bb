// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_DBUS_AUTHPOLICY_DBUS_CONSTANTS_H_
#define SYSTEM_API_DBUS_AUTHPOLICY_DBUS_CONSTANTS_H_

namespace authpolicy {

const char kAuthPolicyInterface[] = "org.chromium.AuthPolicy";
const char kAuthPolicyServicePath[] = "/org/chromium/AuthPolicy";
const char kAuthPolicyServiceName[] = "org.chromium.AuthPolicy";
// Methods
const char kAuthPolicyAuthenticateUser[] = "AuthenticateUser";
const char kAuthPolicyJoinADDomain[] = "JoinADDomain";
const char kAuthPolicyRefreshUserPolicy[] = "RefreshUserPolicy";
const char kAuthPolicyRefreshDevicePolicy[] = "RefreshDevicePolicy";

// D-Bus call error codes. These values are written to logs. New enum values can
// be added, but existing enums must never be renumbered or deleted and reused.
enum ErrorType {
  // Everything is A-OK!
  ERROR_NONE = 0,
  // Unspecified error.
  ERROR_UNKNOWN = 1,
  // Unspecified D-Bus error.
  ERROR_DBUS_FAILURE = 2,
  // Badly formatted user principal name.
  ERROR_PARSE_UPN_FAILED = 3,
  // Auth failed because of bad user name.
  ERROR_BAD_USER_NAME = 4,
  // Auth failed because of bad password.
  ERROR_BAD_PASSWORD = 5,
  // Auth failed because of expired password.
  ERROR_PASSWORD_EXPIRED = 6,
  // Auth failed because of bad realm or network.
  ERROR_CANNOT_RESOLVE_KDC = 7,
  // kinit exited with unspecified error.
  ERROR_KINIT_FAILED = 8,
  // net exited with unspecified error.
  ERROR_NET_FAILED = 9,
  // smdclient exited with unspecified error.
  ERROR_SMBCLIENT_FAILED = 10,
  // authpolicy_parser exited with unknown error.
  ERROR_PARSE_FAILED = 11,
  // Parsing GPOs failed.
  ERROR_PARSE_PREG_FAILED = 12,
  // GPO data is bad.
  ERROR_BAD_GPOS = 13,
  // Some local IO operation failed.
  ERROR_LOCAL_IO = 14,
  // Machine is not joined to AD domain yet.
  ERROR_NOT_JOINED = 15,
  // User is not logged in yet.
  ERROR_NOT_LOGGED_IN = 16,
  // Failed to send policy to Session Manager.
  ERROR_STORE_POLICY_FAILED = 17,
  // User doesn't have the right to join machines to the domain.
  ERROR_JOIN_ACCESS_DENIED = 18,
  // General network problem.
  ERROR_NETWORK_PROBLEM = 19,
  // Machine name contains restricted characters.
  ERROR_BAD_MACHINE_NAME = 20,
  // Machine name too long.
  ERROR_MACHINE_NAME_TOO_LONG = 21,
  // User joined maximum number of machines to the domain.
  ERROR_USER_HIT_JOIN_QUOTA = 22,
  // Kinit or smbclient failed to contact Key Distribution Center.
  ERROR_CONTACTING_KDC_FAILED = 23,
  // Kerberos credentials cache not found.
  ERROR_NO_CREDENTIALS_CACHE_FOUND = 24,
  // Kerberos ticket expired while renewing credentials.
  ERROR_KERBEROS_TICKET_EXPIRED = 25,
  // klist exited with unspecified error.
  ERROR_KLIST_FAILED = 26,
  // Should be the last.
  ERROR_COUNT,
};

}  // namespace authpolicy

#endif  // SYSTEM_API_DBUS_AUTHPOLICY_DBUS_CONSTANTS_H_
