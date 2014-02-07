// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_delegate.h"

base::FieldTrial::Probability
PasswordManagerDelegate::GetProbabilityForExperiment(
    const std::string& experiment_name) {
  return 0;
}

bool PasswordManagerDelegate::IsPasswordSyncEnabled() {
  return false;
}
