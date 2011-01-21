// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of instance count mixin base class.
#include "ceee/testing/utils/instance_count_mixin.h"
#include "base/logging.h"
#include "gtest/gtest.h"

namespace {
// Set of existing instances.
testing::InstanceCountMixinBase::InstanceSet instances;
};

namespace testing {

base::Lock InstanceCountMixinBase::lock_;

InstanceCountMixinBase::InstanceCountMixinBase() {
  base::AutoLock lock(lock_);

  instances.insert(this);
}

InstanceCountMixinBase::~InstanceCountMixinBase() {
  base::AutoLock lock(lock_);

  EXPECT_EQ(1, instances.erase(this));
}

void InstanceCountMixinBase::GetDescription(std::string* description) const {
  *description =  "Override GetDescription in your test classes\n"
      "to get useful information about the leaked objects.";
}

void InstanceCountMixinBase::LogLeakedInstances() {
  base::AutoLock lock(lock_);

  InstanceSet::const_iterator it(instances.begin());
  InstanceSet::const_iterator end(instances.end());

  for (; it != end; ++it) {
    std::string description;
    (*it)->GetDescription(&description);
    LOG(ERROR) << "Leaked: " << description.c_str();
  }
}

typedef InstanceCountMixinBase::InstanceSet InstanceSet;

size_t InstanceCountMixinBase::all_instance_count() {
  base::AutoLock lock(lock_);
  return instances.size();
}

InstanceSet::const_iterator InstanceCountMixinBase::begin() {
  return instances.begin();
}

InstanceSet::const_iterator InstanceCountMixinBase::end() {
  return instances.end();
}

}  // namespace testing
