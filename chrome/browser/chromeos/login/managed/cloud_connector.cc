// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/cloud_connector.h"

#include "chrome/browser/chromeos/login/user_manager.h"

namespace chromeos {

CloudConnector::Delegate::~Delegate() {
}

CloudConnector::CloudConnector(CloudConnector::Delegate* delegate)
    : delegate_(delegate) {
}

CloudConnector::~CloudConnector() {
}

void CloudConnector::GenerateNewUserId() {
  // TODO(antrim) : replace with actual implementation once one exist.
  std::string id = UserManager::Get()->GenerateUniqueLocallyManagedUserId();
  delegate_->NewUserIdGenerated(id);
}

}  // namespace chromeos
