// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/invalidation_switches.h"

namespace invalidation {
namespace switches {

// Overrides the default host:port used for notifications.
const char kSyncNotificationHostPort[]      = "sync-notification-host-port";

// Tries to connect to XMPP using SSLTCP first (for testing).
const char kSyncTrySsltcpFirstForXmpp[]     = "sync-try-ssltcp-first-for-xmpp";

// Invalidates any login info passed into sync's XMPP connection.
const char kSyncInvalidateXmppLogin[]       = "sync-invalidate-xmpp-login";

// Allows insecure XMPP connections for sync (for testing).
const char kSyncAllowInsecureXmppConnection[] =
    "sync-allow-insecure-xmpp-connection";

}  // namespace switches
}  // namespace invalidation
