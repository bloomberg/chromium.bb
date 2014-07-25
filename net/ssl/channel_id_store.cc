// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/channel_id_store.h"

namespace net {

ChannelIDStore::ChannelID::ChannelID() {
}

ChannelIDStore::ChannelID::ChannelID(
    const std::string& server_identifier,
    base::Time creation_time,
    base::Time expiration_time,
    const std::string& private_key,
    const std::string& cert)
    : server_identifier_(server_identifier),
      creation_time_(creation_time),
      expiration_time_(expiration_time),
      private_key_(private_key),
      cert_(cert) {}

ChannelIDStore::ChannelID::~ChannelID() {}

void ChannelIDStore::InitializeFrom(const ChannelIDList& list) {
  for (ChannelIDList::const_iterator i = list.begin(); i != list.end();
      ++i) {
    SetChannelID(i->server_identifier(), i->creation_time(),
                 i->expiration_time(), i->private_key(), i->cert());
  }
}

}  // namespace net
