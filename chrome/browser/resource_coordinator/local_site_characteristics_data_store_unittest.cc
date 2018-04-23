// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store.h"

#include "base/macros.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace resource_coordinator {

using LocalSiteCharacteristicsDataStoreTest = testing::Test;

TEST_F(LocalSiteCharacteristicsDataStoreTest, EndToEnd) {
  const std::string kOrigin = "foo.com";
  LocalSiteCharacteristicsDataStore data_store;
  EXPECT_TRUE(data_store.origin_data_map_for_testing().empty());
  auto reader = data_store.GetReaderForOrigin(kOrigin);
  EXPECT_NE(nullptr, reader.get());
  EXPECT_EQ(1U, data_store.origin_data_map_for_testing().size());

  internal::LocalSiteCharacteristicsDataImpl* impl =
      data_store.origin_data_map_for_testing().find(kOrigin)->second;
  EXPECT_NE(nullptr, impl);

  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureUsageUnknown,
            reader->UpdatesTitleInBackground());
  impl->NotifySiteLoaded();
  impl->NotifyUpdatesTitleInBackground();
  impl->NotifySiteUnloaded();
  EXPECT_EQ(SiteFeatureUsage::kSiteFeatureInUse,
            reader->UpdatesTitleInBackground());

  auto reader_copy = data_store.GetReaderForOrigin(kOrigin);
  EXPECT_EQ(1U, data_store.origin_data_map_for_testing().size());
  reader_copy.reset();

  reader.reset();
  EXPECT_TRUE(data_store.origin_data_map_for_testing().empty());
}

}  // namespace resource_coordinator
