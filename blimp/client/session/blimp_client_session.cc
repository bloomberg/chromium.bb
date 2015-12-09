// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/session/blimp_client_session.h"

#include "blimp/net/browser_connection_handler.h"

namespace blimp {

BlimpClientSession::BlimpClientSession()
    : connection_handler_(new BrowserConnectionHandler) {}

BlimpClientSession::~BlimpClientSession() {}

}  // namespace blimp
