// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/sync/notifier/communicator/login_settings.h"

#include "chrome/browser/sync/notifier/communicator/connection_options.h"
#include "chrome/browser/sync/notifier/communicator/xmpp_connection_generator.h"
#include "talk/base/common.h"
#include "talk/base/socketaddress.h"
#include "talk/xmpp/xmppclientsettings.h"

namespace notifier {

LoginSettings::LoginSettings(const buzz::XmppClientSettings& user_settings,
                             const ConnectionOptions& options,
                             std::string lang,
                             ServerInformation* server_list,
                             int server_count,
                             talk_base::FirewallManager* firewall,
                             bool no_gaia_auth,
                             bool proxy_only)
    :  proxy_only_(proxy_only),
       no_gaia_auth_(no_gaia_auth),
       firewall_(firewall),
       lang_(lang),
       server_list_(new ServerInformation[server_count]),
       server_count_(server_count),
       user_settings_(new buzz::XmppClientSettings(user_settings)),
       connection_options_(new ConnectionOptions(options)) {
  // Note: firewall may be NULL
  ASSERT(server_list != 0);
  ASSERT(server_count > 0);
  for (int i = 0; i < server_count_; ++i) {
    server_list_[i] = server_list[i];
  }
}

// defined so that the destructors are executed here (and
// the corresponding classes don't need to be included in
// the header file)
LoginSettings::~LoginSettings() {
}

void LoginSettings::set_server_override(
    const talk_base::SocketAddress& server) {
  server_override_.reset(new ServerInformation());
  server_override_->server = server;
  server_override_->special_port_magic = server_list_[0].special_port_magic;
}

void LoginSettings::clear_server_override() {
  server_override_.reset();
}
}  // namespace notifier
