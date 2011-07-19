// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/gaia/gaia_auth_consumer.h"

GaiaAuthConsumer::ClientLoginResult::ClientLoginResult()
    : two_factor(false) {
}

GaiaAuthConsumer::ClientLoginResult::ClientLoginResult(
    const std::string& new_sid,
    const std::string& new_lsid,
    const std::string& new_token,
    const std::string& new_data)
    : sid(new_sid),
      lsid(new_lsid),
      token(new_token),
      data(new_data),
      two_factor(false) {}

GaiaAuthConsumer::ClientLoginResult::~ClientLoginResult() {}

bool GaiaAuthConsumer::ClientLoginResult::operator==(
    const ClientLoginResult &b) const {
  return sid == b.sid &&
      lsid == b.lsid &&
      token == b.token &&
      data == b.data &&
      two_factor == b.two_factor;
}
