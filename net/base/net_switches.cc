// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_switches.h"

namespace switches {

// This switch will take the JSON-formatted HSTS specification and load it
// as if it were a preloaded HSTS entry. It will take precedence over both
// website-specified rules and built-in rules.
// The JSON format is the same as that persisted in
// <profile_dir>/Default/TransportSecurity
const char kHstsHosts[]                  = "hsts-hosts";

}  // namespace switches

