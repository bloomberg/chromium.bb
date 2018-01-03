// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/chrome_navigation_data.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/previews/core/previews_user_data.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeNavigationDataTest : public testing::Test {
 public:
  ChromeNavigationDataTest() {}
  ~ChromeNavigationDataTest() override {}
};

TEST_F(ChromeNavigationDataTest, AddingDataReductionProxyData) {
  std::unique_ptr<ChromeNavigationData> data(new ChromeNavigationData());
  data_reduction_proxy::DataReductionProxyData* data_reduction_proxy_data =
      new data_reduction_proxy::DataReductionProxyData();
  data->SetDataReductionProxyData(base::WrapUnique(data_reduction_proxy_data));
  EXPECT_EQ(data_reduction_proxy_data, data->GetDataReductionProxyData());
}

TEST_F(ChromeNavigationDataTest, AddingPreviewsUserData) {
  ChromeNavigationData data;
  EXPECT_FALSE(data.previews_user_data());
  data.set_previews_user_data(std::make_unique<previews::PreviewsUserData>(1u));
  EXPECT_TRUE(data.previews_user_data());
}

TEST_F(ChromeNavigationDataTest, Serialization) {
  // data_reduction_proxy
  {
    ChromeNavigationData chrome_navigation_data;
    ChromeNavigationData clone_a(chrome_navigation_data.ToValue());
    chrome_navigation_data.SetDataReductionProxyData(
        std::make_unique<data_reduction_proxy::DataReductionProxyData>());
    ChromeNavigationData clone_b(chrome_navigation_data.ToValue());

    EXPECT_FALSE(clone_a.GetDataReductionProxyData());
    EXPECT_TRUE(clone_b.GetDataReductionProxyData());
  }

  // preview_user_data
  {
    ChromeNavigationData chrome_navigation_data;
    ChromeNavigationData clone_a(chrome_navigation_data.ToValue());
    chrome_navigation_data.set_previews_user_data(
        std::make_unique<previews::PreviewsUserData>(1u));
    ChromeNavigationData clone_b(chrome_navigation_data.ToValue());

    EXPECT_FALSE(clone_a.previews_user_data());
    EXPECT_TRUE(clone_b.previews_user_data());
  }

  // preview_state
  {
    ChromeNavigationData chrome_navigation_data;
    ChromeNavigationData clone_a(chrome_navigation_data.ToValue());
    chrome_navigation_data.set_previews_state(content::PREVIEWS_OFF);
    ChromeNavigationData clone_b(chrome_navigation_data.ToValue());

    EXPECT_EQ(content::PREVIEWS_UNSPECIFIED, clone_a.previews_state());
    EXPECT_EQ(content::PREVIEWS_OFF, clone_b.previews_state());
  }
}
