// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/mock_session_manager_client.h"

#include "base/logging.h"

namespace chromeos {

MockSessionManagerClient::MockSessionManagerClient() {}

MockSessionManagerClient::~MockSessionManagerClient() {}

void MockSessionManagerClient::RestartJob(int socket_fd,
                                          const std::vector<std::string>& argv,
                                          VoidDBusMethodCallback callback) {
  NOTREACHED() << "No mock for RestartJob";
}

}  // namespace chromeos
