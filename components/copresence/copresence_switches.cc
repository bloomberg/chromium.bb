// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/copresence/copresence_switches.h"

// TODO(ckehoe): Move these flags to the chrome://copresence page.

namespace switches {

// Directory to dump encoded tokens to, for debugging.
// If empty (the default), tokens are not dumped.
// If invalid (not a writable directory), Chrome will crash!
const char kCopresenceDumpTokensToDir[] = "copresence-dump-tokens-to-dir";

// Allow broadcast of audible audio tokens. Defaults to true.
const char kCopresenceEnableAudibleBroadcast[] =
    "copresence-enable-audible-broadcast";

// Allow broadcast of inaudible audio tokens. Defaults to true.
const char kCopresenceEnableInaudibleBroadcast[] =
    "copresence-enable-inaudible-broadcast";

// Address for calls to the Copresence server (via Apiary).
// Defaults to https://www.googleapis.com/copresence/v2/copresence.
const char kCopresenceServer[] = "copresence-server";

// Apiary tracing token for calls to the Copresence server.
const char kCopresenceTracingToken[] = "copresence-tracing-token";

}  // namespace switches
