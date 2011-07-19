// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <deque>
#include <string>
#include <vector>

#include "base/logging.h"
#include "jingle/notifier/communicator/connection_settings.h"
#include "talk/base/helpers.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace notifier {

class RandomGenerator {
 public:
  int operator()(int ceiling) {
    return static_cast<int>(talk_base::CreateRandomId() % ceiling);
  }
};

ConnectionSettings::ConnectionSettings() : protocol_(cricket::PROTO_TCP) {}

ConnectionSettings::~ConnectionSettings() {}

void ConnectionSettings::FillXmppClientSettings(
    buzz::XmppClientSettings* xcs) const {
  DCHECK(xcs);
  xcs->set_protocol(protocol_);
  xcs->set_server(server_);
  xcs->set_proxy(talk_base::PROXY_NONE);
  xcs->set_use_proxy_auth(false);
}

ConnectionSettingsList::ConnectionSettingsList() {}

ConnectionSettingsList::~ConnectionSettingsList() {}

void ConnectionSettingsList::AddPermutations(const std::string& hostname,
                                             const std::vector<uint32>& iplist,
                                             uint16 port,
                                             bool special_port_magic,
                                             bool try_ssltcp_first) {
  // randomize the list. This ensures the iplist isn't always
  // evaluated in the order returned by DNS
  std::vector<uint32> iplist_random = iplist;
  RandomGenerator rg;
  std::random_shuffle(iplist_random.begin(), iplist_random.end(), rg);

  // Put generated addresses in a new deque, then append on the list_, since
  // there are order dependencies and AddPermutations() may be called more
  // than once.
  std::deque<ConnectionSettings> list_temp;

  // Permute addresses for this server. In some cases we haven't resolved the
  // to ip addresses.
  talk_base::SocketAddress server(hostname, port);
  if (iplist_random.empty()) {
    // We couldn't pre-resolve the hostname, so let's hope it will resolve
    // further down the pipeline (by a proxy, for example).
    PermuteForAddress(server, special_port_magic, try_ssltcp_first,
                      &list_temp);
  } else {
    // Generate a set of possibilities for each server address.
    // Don't do permute duplicates.
    for (size_t index = 0; index < iplist_random.size(); ++index) {
      if (std::find(iplist_seen_.begin(), iplist_seen_.end(),
                    iplist_random[index]) != iplist_seen_.end()) {
        continue;
      }
      iplist_seen_.push_back(iplist_random[index]);
      server.SetResolvedIP(iplist_random[index]);
      PermuteForAddress(server, special_port_magic, try_ssltcp_first,
                        &list_temp);
    }
  }

  // Add this list to the instance list
  while (!list_temp.empty()) {
    list_.push_back(list_temp[0]);
    list_temp.pop_front();
  }
}


void ConnectionSettingsList::PermuteForAddress(
    const talk_base::SocketAddress& server,
    bool special_port_magic,
    bool try_ssltcp_first,
    std::deque<ConnectionSettings>* list_temp) {
  DCHECK(list_temp);
  *(template_.mutable_server()) = server;

  // Use all of the original settings
  list_temp->push_back(template_);

  // Try alternate port
  if (special_port_magic) {
    ConnectionSettings settings(template_);
    settings.set_protocol(cricket::PROTO_SSLTCP);
    settings.mutable_server()->SetPort(443);
    if (try_ssltcp_first) {
      list_temp->push_front(settings);
    } else {
      list_temp->push_back(settings);
    }
  }
}
}  // namespace notifier
