// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/mock_user_cloud_policy_store.h"

namespace policy {

MockUserCloudPolicyStore::MockUserCloudPolicyStore()
    : UserCloudPolicyStore(NULL, base::FilePath()) {}

MockUserCloudPolicyStore::~MockUserCloudPolicyStore() {}

}  // namespace policy
