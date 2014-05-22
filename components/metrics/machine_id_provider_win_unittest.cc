// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/machine_id_provider.h"

#include "base/memory/ref_counted.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

TEST(MachineIdProviderTest, GetId) {
  scoped_refptr<MachineIdProvider> provider(
      MachineIdProvider::CreateInstance());
  std::string id1 = provider->GetMachineId();

  EXPECT_NE(std::string(), id1);

  std::string id2 = provider->GetMachineId();
  EXPECT_EQ(id1, id2);
}

}  // namespace metrics
