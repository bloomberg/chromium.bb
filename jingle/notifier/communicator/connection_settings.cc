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

void ConnectionSettings::FillXmppClientSettings(
    buzz::XmppClientSettings* xcs) const {
  DCHECK(xcs);
  xcs->set_protocol(protocol_);
  xcs->set_server(server_);
  xcs->set_proxy(proxy_.type);
  if (proxy_.type != talk_base::PROXY_NONE) {
    xcs->set_proxy_host(proxy_.address.IPAsString());
    xcs->set_proxy_port(proxy_.address.port());
  }
  if ((proxy_.type != talk_base::PROXY_NONE) && !proxy_.username.empty()) {
    xcs->set_use_proxy_auth(true);
    xcs->set_proxy_user(proxy_.username);
    xcs->set_proxy_pass(proxy_.password);
  } else {
    xcs->set_use_proxy_auth(false);
  }
}

void ConnectionSettingsList::AddPermutations(const std::string& hostname,
                                             const std::vector<uint32>& iplist,
                                             int16 port,
                                             bool special_port_magic,
                                             bool try_ssltcp_first,
                                             bool proxy_only) {
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
    PermuteForAddress(server, special_port_magic, try_ssltcp_first, proxy_only,
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
                        proxy_only, &list_temp);
    }
  }

  // Add this list to the instance list
  while (list_temp.size() != 0) {
    list_.push_back(list_temp[0]);
    list_temp.pop_front();
  }
}


void ConnectionSettingsList::PermuteForAddress(
    const talk_base::SocketAddress& server,
    bool special_port_magic,
    bool try_ssltcp_first,
    bool proxy_only,
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
    // HTTPS proxies usually require port 443, so try it first. In addition,
    // when certain tests like the sync integration tests are run on the
    // chromium builders, we try the SSLTCP port (443) first because the XMPP
    // port (5222) is blocked.
    if ((template_.proxy().type == talk_base::PROXY_HTTPS) ||
        (template_.proxy().type == talk_base::PROXY_UNKNOWN) ||
        try_ssltcp_first) {
      list_temp->push_front(settings);
    } else {
      list_temp->push_back(settings);
    }
  }

  if (!proxy_only) {
    // Try without the proxy
    if (template_.proxy().type != talk_base::PROXY_NONE) {
      ConnectionSettings settings(template_);
      settings.mutable_proxy()->type = talk_base::PROXY_NONE;
      list_temp->push_back(settings);

      if (special_port_magic) {
        settings.set_protocol(cricket::PROTO_SSLTCP);
        settings.mutable_server()->SetPort(443);
        if (try_ssltcp_first) {
          list_temp->push_front(settings);
        } else {
          list_temp->push_back(settings);
        }
      }
    }
  }
}
}  // namespace notifier
