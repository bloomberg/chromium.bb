// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/activity_log/activity_log_policy.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/extension.h"
#include "url/gurl.h"

namespace extensions {

ActivityLogPolicy::ActivityLogPolicy(Profile* profile) {
  CHECK(profile && "Null ptr dereference");
  profile_base_path_ = profile->GetPath();
}

ActivityLogPolicy::~ActivityLogPolicy() {
}

std::string ActivityLogPolicy::GetKey(KeyType) const {
  return std::string();
}

}  // namespace extensions
