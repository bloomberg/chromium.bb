// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/activity_log/uma_policy.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/common/manifest_constants.h"
#include "net/dns/mock_host_resolver.h"

using extensions::UmaPolicy;

const char* kGooglePrefix = "ExtensionActivity.Google";
const char* kNonGooglePrefix = "ExtensionActivity";

// These tests need to ensure that all of the extension JavaScript completes
// before the histograms are checked. To accomplish this, the test relies on
// some JavaScript in chrome/test/data/extensions/api_test/uma_policy/:
// * When the test navigates to opener.com, opener.js will use window.open() to
//   pop open a new window with the appropriate URL for the test case. This
//   ensures that the testing framework never reuses a window that's still
//   running a previous test case.
// * The test extension code in content_script.js tells background.js when it's
//   done. When it's finished, background.js closes the blocker.com window. So
//   blocker.com will remain open (and block) until the tests complete.
class ActivityLogUmaPolicyTest : public ExtensionApiTest {
};

std::string ConcatNames(const char* prefix, int status_num) {
  return base::StringPrintf(
      "%s.%s",
      prefix,
      extensions::UmaPolicy::GetHistogramName(
          static_cast<extensions::UmaPolicy::PageStatus>(status_num)));
}

// TODO(felt): These are disabled due to crbug.com/294500, since they fail
// due to a blink bug. The fix went in to Blink on Thursday and should roll
// on Monday 9/23.
// These are all sequential navigations, so they should each be logged
// independently.
IN_PROC_BROWSER_TEST_F(ActivityLogUmaPolicyTest, DISABLED_SequentialNavs) {
  host_resolver()->AddRule("*", "127.0.0.1");
  StartEmbeddedTestServer();

  const extensions::Extension* ext =
      LoadExtension(test_data_dir_.AppendASCII("uma_policy"));
  ASSERT_TRUE(ext);

  ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("http://www.opener.com/#google"), NEW_WINDOW,
        ui_test_utils::BROWSER_TEST_NONE);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("http://www.blocker.com"), 2);
  ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("http://www.opener.com/#google?q=a"), NEW_WINDOW,
        ui_test_utils::BROWSER_TEST_NONE);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("http://www.blocker.com"), 2);
  ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("http://www.opener.com/#google?q=b"), NEW_WINDOW,
        ui_test_utils::BROWSER_TEST_NONE);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("http://www.blocker.com"), 2);
  ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("http://www.opener.com/#cnn?q=a"), NEW_WINDOW,
        ui_test_utils::BROWSER_TEST_NONE);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("http://www.blocker.com"), 2);
  ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("http://www.opener.com/#cnn?q=b"), NEW_WINDOW,
        ui_test_utils::BROWSER_TEST_NONE);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("http://www.blocker.com"), 2);

  for (int i = UmaPolicy::NONE + 1; i < UmaPolicy::MAX_STATUS - 2; ++i) {
    base::HistogramBase* google_histogram = base::Histogram::FactoryGet(
        ConcatNames(kGooglePrefix, i),
        1, 100, 50, base::HistogramBase::kNoFlags);
    scoped_ptr<base::HistogramSamples> google_samples =
        google_histogram->SnapshotSamples();
    EXPECT_EQ(3, google_samples->TotalCount());
    EXPECT_EQ(3, google_samples->GetCount(1));

    base::HistogramBase* cnn_histogram = base::Histogram::FactoryGet(
        ConcatNames(kNonGooglePrefix, i),
        1, 100, 50, base::HistogramBase::kNoFlags);
    scoped_ptr<base::HistogramSamples> cnn_samples =
        cnn_histogram->SnapshotSamples();
    if (ConcatNames(kNonGooglePrefix, i) != "ExtensionActivity.ContentScript" &&
        ConcatNames(kNonGooglePrefix, i) != "ExtensionActivity.ReadDom") {
      // There's a content script on opener.com that checks the location.
      // The test is not set up to accurately record opener.com histograms due
      // to the possibility of race conditions in the testing framework, so we
      // can't check those values.
      EXPECT_EQ(2, cnn_samples->GetCount(1));
    }
  }
}

// Two windows are open at once with the same google.com TLD.
// However, they should be treated separately because they have different URLs.
IN_PROC_BROWSER_TEST_F(
    ActivityLogUmaPolicyTest, DISABLED_ParallelDistinctNavs) {
  host_resolver()->AddRule("*", "127.0.0.1");
  StartEmbeddedTestServer();

  const extensions::Extension* ext =
      LoadExtension(test_data_dir_.AppendASCII("uma_policy"));
  ASSERT_TRUE(ext);

  ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("http://www.opener.com/#google?p=a"), NEW_WINDOW,
        ui_test_utils::BROWSER_TEST_NONE);
  ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("http://www.opener.com/#google?p=b"), NEW_WINDOW,
        ui_test_utils::BROWSER_TEST_NONE);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("http://www.blocker.com"), 2);

  for (int i = UmaPolicy::NONE + 1; i < UmaPolicy::MAX_STATUS - 2; ++i) {
    base::HistogramBase* google_histogram = base::Histogram::FactoryGet(
        ConcatNames(kGooglePrefix, i),
        1, 100, 50, base::HistogramBase::kNoFlags);
    scoped_ptr<base::HistogramSamples> google_samples =
        google_histogram->SnapshotSamples();
    EXPECT_EQ(2, google_samples->GetCount(1));
  }
}

// Two windows are open at once with the same Google URLs.
// They should be treated the same.
IN_PROC_BROWSER_TEST_F(ActivityLogUmaPolicyTest, DISABLED_Google_ParallelSame) {
  host_resolver()->AddRule("*", "127.0.0.1");
  StartEmbeddedTestServer();

  const extensions::Extension* ext =
      LoadExtension(test_data_dir_.AppendASCII("uma_policy"));
  ASSERT_TRUE(ext);

  ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("http://www.opener.com/#googlea"), NEW_WINDOW,
        ui_test_utils::BROWSER_TEST_NONE);
  ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("http://www.opener.com/#googleb"), NEW_WINDOW,
        ui_test_utils::BROWSER_TEST_NONE);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("http://www.blocker.com"), 2);

  for (int i = UmaPolicy::NONE + 1; i < UmaPolicy::MAX_STATUS - 2; ++i) {
    base::HistogramBase* google_histogram = base::Histogram::FactoryGet(
        ConcatNames(kGooglePrefix, i),
        1, 100, 50, base::HistogramBase::kNoFlags);
    scoped_ptr<base::HistogramSamples> google_samples =
        google_histogram->SnapshotSamples();
    EXPECT_EQ(1, google_samples->GetCount(1));
  }
}

// Two windows are open at once with the same non-Google URLs.
// They should be treated the same.
IN_PROC_BROWSER_TEST_F(ActivityLogUmaPolicyTest,
    DISABLED_NonGoogle_ParallelSame) {
  host_resolver()->AddRule("*", "127.0.0.1");
  StartEmbeddedTestServer();

  const extensions::Extension* ext =
      LoadExtension(test_data_dir_.AppendASCII("uma_policy"));
  ASSERT_TRUE(ext);

  ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("http://www.opener.com/#cnna"), NEW_WINDOW,
        ui_test_utils::BROWSER_TEST_NONE);
  ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("http://www.opener.com/#cnnb"), NEW_WINDOW,
        ui_test_utils::BROWSER_TEST_NONE);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("http://www.blocker.com"), 2);

  for (int i = UmaPolicy::NONE + 1; i < UmaPolicy::MAX_STATUS - 2; ++i) {
    base::HistogramBase* cnn_histogram = base::Histogram::FactoryGet(
        ConcatNames(kNonGooglePrefix, i),
        1, 100, 50, base::HistogramBase::kNoFlags);
    scoped_ptr<base::HistogramSamples> cnn_samples =
        cnn_histogram->SnapshotSamples();
    if (ConcatNames(kNonGooglePrefix, i) != "ExtensionActivity.ContentScript" &&
        ConcatNames(kNonGooglePrefix, i) != "ExtensionActivity.ReadDom") {
      // There's a content script on opener.com that checks the location.
      // The test is not set up to accurately record opener.com histograms due
      // to the possibility of race conditions in the testing framework, so we
      // can't check those values.
      EXPECT_EQ(1, cnn_samples->GetCount(1));
    }
  }
}

// This runs with multiple extensions installed.
IN_PROC_BROWSER_TEST_F(ActivityLogUmaPolicyTest, DISABLED_MultipleExtensions) {
  host_resolver()->AddRule("*", "127.0.0.1");
  StartEmbeddedTestServer();

  const extensions::Extension* ext =
      LoadExtension(test_data_dir_.AppendASCII("uma_policy"));
  ASSERT_TRUE(ext);

  const char* script2 =
      "document.createElement('script');"
      "document.createElement('iframe');"
      "document.createElement('div');"
      "document.createElement('embed');"
      "document.createElement('object');";

  const char* manifest =
      "{"
      "  \"name\": \"Activity Log UMA Policy Test Extension\","
      "  \"version\": \"0.%s\","
      "  \"description\": \"Testing the histogramming\","
      "  \"content_scripts\": ["
      "      {"
      "        \"matches\": "
      "            [\"http://www.google.com/*\","
      "             \"http://www.cnn.com/*\"],"
      "        \"js\": [\"content_script.js\"]"
      "      }"
      "    ],"
      "  \"manifest_version\": 2"
      "}";

  extensions::TestExtensionDir dir2;
  dir2.WriteManifest(base::StringPrintf(manifest, "2"));
  dir2.WriteFile(FILE_PATH_LITERAL("content_script.js"), script2);
  LoadExtension(dir2.unpacked_path());

  ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("http://www.opener.com/#google"), NEW_WINDOW,
        ui_test_utils::BROWSER_TEST_NONE);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("http://www.blocker.com"), 2);
  ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("http://www.opener.com/#cnn?q=b"), NEW_WINDOW,
        ui_test_utils::BROWSER_TEST_NONE);
  ui_test_utils::NavigateToURLBlockUntilNavigationsComplete(
      browser(), GURL("http://www.blocker.com"), 2);

  const char* subset_one[] = {
      "CreatedLink",
      "InnerHtml",
      "DocumentWrite"
  };

  const char* subset_two[] = {
      "ContentScript",
      "CreatedScript",
      "CreatedIframe",
      "CreatedDiv",
      "CreatedEmbed",
      "CreatedObject",
      "InvokedDomMethod"
  };

  // These were only touched by one of the scripts.
  for (size_t i = 0; i < arraysize(subset_one); ++i) {
    base::HistogramBase* google_histogram = base::Histogram::FactoryGet(
        std::string(kGooglePrefix) + "." + std::string(subset_one[i]),
        1, 100, 50, base::HistogramBase::kNoFlags);
    scoped_ptr<base::HistogramSamples> google_samples =
        google_histogram->SnapshotSamples();
    EXPECT_EQ(1, google_samples->GetCount(1));

    base::HistogramBase* cnn_histogram = base::Histogram::FactoryGet(
        std::string(kNonGooglePrefix) + "." + std::string(subset_one[i]),
        1, 100, 50, base::HistogramBase::kNoFlags);
    scoped_ptr<base::HistogramSamples> cnn_samples =
        cnn_histogram->SnapshotSamples();
    EXPECT_EQ(1, cnn_samples->GetCount(1));
  }

  // These were touched by both scripts.
  for (size_t i = 0; i < arraysize(subset_two); ++i) {
    base::HistogramBase* google_histogram = base::Histogram::FactoryGet(
        std::string(kGooglePrefix) + "." + std::string(subset_two[i]),
        1, 100, 50, base::HistogramBase::kNoFlags);
    scoped_ptr<base::HistogramSamples> google_samples =
        google_histogram->SnapshotSamples();
    EXPECT_EQ(1, google_samples->GetCount(2));

    base::HistogramBase* cnn_histogram = base::Histogram::FactoryGet(
        std::string(kNonGooglePrefix) + "." + std::string(subset_two[i]),
        1, 100, 50, base::HistogramBase::kNoFlags);
    scoped_ptr<base::HistogramSamples> cnn_samples =
        cnn_histogram->SnapshotSamples();
    EXPECT_EQ(1, cnn_samples->GetCount(2));
  }

}
