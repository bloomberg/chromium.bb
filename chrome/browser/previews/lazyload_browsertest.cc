// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class LazyLoadBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kLazyImageLoading,
        {{"restrict-lazy-load-images-to-data-saver-only", "false"},
         {"automatic-lazy-load-images-enabled", "true"}});
    InProcessBrowserTest::SetUp();
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(LazyLoadBrowserTest, CSSBackgroundImageDeferred) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/lazyload/css-background-image.html"));

  base::RunLoop().RunUntilIdle();
  // Navigate away to finish the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Verify that nothing is recorded for the image bucket.
  EXPECT_GE(0, histogram_tester.GetBucketCount(
                   "DataUse.ContentType.UserTrafficKB",
                   data_use_measurement::DataUseUserData::IMAGE));
}

// TODO(rajendrant): Add a test that checks if the deferred image is loaded when
// user scrolls near it.

IN_PROC_BROWSER_TEST_F(LazyLoadBrowserTest, CSSPseudoBackgroundImageLoaded) {
  ASSERT_TRUE(embedded_test_server()->Start());
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/lazyload/css-pseudo-background-image.html"));

  base::RunLoop().RunUntilIdle();
  // Navigate away to finish the histogram recording.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Verify that the image bucket has substantial kilobytes recorded.
  EXPECT_GE(30 /* KB */, histogram_tester.GetBucketCount(
                             "DataUse.ContentType.UserTrafficKB",
                             data_use_measurement::DataUseUserData::IMAGE));
}
