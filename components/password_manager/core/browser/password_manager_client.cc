// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_client.h"

namespace password_manager {

base::FieldTrial::Probability
PasswordManagerClient::GetProbabilityForExperiment(
    const std::string& experiment_name) {
  return 0;
}

bool PasswordManagerClient::IsPasswordSyncEnabled() { return false; }

}  // namespace password_manager
