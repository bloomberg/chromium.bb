// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_client.h"

base::FieldTrial::Probability
PasswordManagerClient::GetProbabilityForExperiment(
    const std::string& experiment_name) {
  return 0;
}

bool PasswordManagerClient::IsPasswordSyncEnabled() { return false; }
