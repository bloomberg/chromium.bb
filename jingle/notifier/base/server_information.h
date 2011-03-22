// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A utility struct for storing the information for an XMPP server.

#ifndef JINGLE_NOTIFIER_BASE_SERVER_INFORMATION_H_
#define JINGLE_NOTIFIER_BASE_SERVER_INFORMATION_H_

#include <vector>

#include "net/base/host_port_pair.h"

namespace notifier {

struct ServerInformation {
  ServerInformation(const net::HostPortPair& server,
                    bool special_port_magic);
  ServerInformation();

  net::HostPortPair server;
  bool special_port_magic;
};

typedef std::vector<ServerInformation> ServerList;

}  // namespace notifier

#endif  // JINGLE_NOTIFIER_BASE_SERVER_INFORMATION_H_
