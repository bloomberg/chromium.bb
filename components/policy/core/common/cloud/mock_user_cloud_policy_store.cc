// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/mock_user_cloud_policy_store.h"
#include "base/sequenced_task_runner.h"

namespace policy {

MockUserCloudPolicyStore::MockUserCloudPolicyStore()
    : UserCloudPolicyStore(base::FilePath(),
                           base::FilePath(),
                           scoped_refptr<base::SequencedTaskRunner>()) {}

MockUserCloudPolicyStore::~MockUserCloudPolicyStore() {}

}  // namespace policy
