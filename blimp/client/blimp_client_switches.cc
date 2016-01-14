// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/blimp_client_switches.h"

namespace blimp {
namespace switches {

// Specifies the blimplet IP-address to connect to, e.g.:
// --blimplet-host="127.0.0.1".
// TODO(nyquist): Add support for DNS-lookup. See http://crbug.com/576857.
const char kBlimpletHost[] = "blimplet-host";

// Specifies the blimplet port to connect to, e.g.:
// --blimplet-tcp-port=25467.
const char kBlimpletTCPPort[] = "blimplet-tcp-port";

}  // namespace switches
}  // namespace blimp
