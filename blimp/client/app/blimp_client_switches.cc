// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/app/blimp_client_switches.h"

namespace blimp {
namespace switches {

// Specifies the blimplet scheme, IP-address and port to connect to, e.g.:
// --blimplet-host="tcp:127.0.0.1:25467".  Valid schemes are "ssl",
// "tcp", and "quic".
// TODO(nyquist): Add support for DNS-lookup. See http://crbug.com/576857.
const char kBlimpletEndpoint[] = "blimplet-endpoint";

}  // namespace switches
}  // namespace blimp
