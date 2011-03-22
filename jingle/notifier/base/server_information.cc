// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/server_information.h"

namespace notifier {

ServerInformation::ServerInformation(
    const net::HostPortPair& server, bool special_port_magic)
    : server(server), special_port_magic(special_port_magic) {}

ServerInformation::ServerInformation() : special_port_magic(false) {}

}  // namespace notifier
