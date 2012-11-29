// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/window_snapshot/window_snapshot.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "ui/compositor/compositor_setup.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/size.h"

using ::testing::AllOf;
using ::testing::Le;
using ::testing::Ge;

// Test fixture for the MapsGL endurance tests.
//
// This runs the MapsGL harness one or more times, and checks the
// value of certain pixels at the end of the run in order to make sure
// that the rendering actually occurred as we expected it to. Which
// pixels are checked, their expected values and tolerances are all
// encoded in a JSON file accompanying the test.
//
// Pass the command line argument --save-test-failures to save the PNG
// of any failing test runs. Currently there is only one test and it
// will write its output to "single-run-basic-output.png" in the
// current working directory.
//
// TODO(kbr): Add more documentation on adding to and modifying these
// tests.
class MapsGLEnduranceTest : public InProcessBrowserTest {
 public:
  MapsGLEnduranceTest() {
  }

  virtual void SetUpInProcessBrowserTestFixture() {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_));
    test_data_dir_ = test_data_dir_.AppendASCII("gpu");

    ui::DisableTestCompositor();
  }

  void RunSingleTest(const gfx::Size& tab_container_size,
                     const std::string& url,
                     const std::string& json_test_expectations_filename,
                     const std::string& failure_filename_prefix) {
    std::vector<SinglePixelExpectation> expectations;
    FilePath test_expectations_path =
        test_data_dir().AppendASCII(json_test_expectations_filename);
    if (!ReadTestExpectations(test_expectations_path, &expectations)) {
      LOG(ERROR) << "Failed to read test expectations from file "
                 << test_expectations_path.value();
      return;
    }

#if defined(OS_WIN)
    ASSERT_TRUE(tracing::BeginTracing("-test_*"));
#endif

    browser()->window()->Activate();
    gfx::Rect new_bounds = GetNewTabContainerBounds(tab_container_size);
    browser()->window()->SetBounds(new_bounds);

    content::DOMMessageQueue message_queue;
    ui_test_utils::NavigateToURL(browser(), GURL(url));

    // Wait for notification that the test completed.
    std::string message;
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
    message_queue.ClearQueue();
    // TODO(kbr): figure out why this is escaped
    EXPECT_EQ("\"FINISHED\"", message);

    // Take a snapshot of the web page and compare it to the test
    // expectations.
    SkBitmap bitmap;
    ASSERT_TRUE(TabSnapShotToImage(&bitmap));

    bool all_pixels_match =
        CompareToExpectedResults(bitmap, expectations);

    if (!all_pixels_match &&
        CommandLine::ForCurrentProcess()->HasSwitch("save-test-failures")) {
      std::vector<unsigned char> output;
      if (!gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, true, &output)) {
        LOG(ERROR) << "Re-encode PNG failed";
      } else {
        FilePath output_path;
        output_path = output_path.AppendASCII(
            failure_filename_prefix + "-output.png");
        if (file_util::WriteFile(
                output_path,
                reinterpret_cast<char*>(&*output.begin()), output.size()) < 0) {
          LOG(ERROR) << "Write PNG to disk failed";
        }
      }
    }

#if defined(OS_WIN)
    // For debugging the flaky test, this prints out a trace of what happened on
    // failure.
    std::string trace_events;
    ASSERT_TRUE(tracing::EndTracing(&trace_events));
    if (!all_pixels_match)
      fprintf(stderr, "\n\nTRACE JSON:\n\n%s\n\n", trace_events.c_str());
#endif
  }

  const FilePath& test_data_dir() const {
    return test_data_dir_;
  }

 private:
  struct SinglePixelExpectation {
    SkIPoint location;
    SkColor color;
    int tolerance;
  };

  FilePath test_data_dir_;

  // Test expectations are expressed in the following JSON format:
  // [
  //   { "location": [ 25, 50 ],     // x, y (upper left origin)
  //     "color": [ 127, 127, 127 ], // red, green, blue
  //     "tolerance": 3
  //   }, ...
  // ]
  bool ReadTestExpectations(const FilePath& json_test_expectations_path,
                            std::vector<SinglePixelExpectation>* expectations) {
    std::string json_contents;
    if (!file_util::ReadFileToString(
            json_test_expectations_path, &json_contents)) {
      DLOG(ERROR) << "ReadFileToString failed for "
                  << json_test_expectations_path.value();
      return false;
    }
    scoped_ptr<Value> root;
    int error_code;
    std::string error_msg;
    root.reset(base::JSONReader::ReadAndReturnError(
        json_contents, base::JSON_ALLOW_TRAILING_COMMAS,
        &error_code, &error_msg));
    if (root.get() == NULL) {
      DLOG(ERROR) << "Root was NULL: error code "
                  << error_code << ", error message " << error_msg;
      return false;
    }
    ListValue* root_list;
    if (!root->GetAsList(&root_list)) {
      DLOG(ERROR) << "Root was not a list (type == " << root->GetType() << ")";
      return false;
    }
    for (size_t ii = 0; ii < root_list->GetSize(); ++ii) {
      DictionaryValue* entry;
      if (!root_list->GetDictionary(ii, &entry)) {
        DLOG(ERROR) << "Root entry " << ii << " was not a dictionary";
        return false;
      }
      ListValue* location;
      if (!entry->GetList("location", &location)) {
        DLOG(ERROR) << "Root entry " << ii << "'s location was not a list";
        return false;
      }
      if (location->GetSize() != 2) {
        DLOG(ERROR) << "Root entry " << ii << "'s location list not length 2";
        return false;
      }
      int x, y;
      if (!location->GetInteger(0, &x) ||
          !location->GetInteger(1, &y)) {
        DLOG(ERROR) << "Root entry " << ii << "'s location list not integers";
        return false;
      }
      ListValue* color;
      if (!entry->GetList("color", &color)) {
        DLOG(ERROR) << "Root entry " << ii << "'s color was not a list";
        return false;
      }
      if (color->GetSize() != 3) {
        DLOG(ERROR) << "Root entry " << ii << "'s color list not length 3";
        return false;
      }
      int red, green, blue;
      if (!color->GetInteger(0, &red) ||
          !color->GetInteger(1, &green) ||
          !color->GetInteger(2, &blue)) {
        DLOG(ERROR) << "Root entry " << ii << "'s color list not integers";
        return false;
      }
      int tolerance;
      if (!entry->GetInteger("tolerance", &tolerance)) {
        DLOG(ERROR) << "Root entry " << ii << "'s tolerance not an integer";
        return false;
      }
      SinglePixelExpectation expectation;
      expectation.location = SkIPoint::Make(x, y);
      expectation.color = SkColorSetRGB(red, green, blue);
      expectation.tolerance = tolerance;
      expectations->push_back(expectation);
    }
    return true;
  }

  // Take snapshot of the current tab, encode it as PNG, and save to a SkBitmap.
  bool TabSnapShotToImage(SkBitmap* bitmap) {
    CHECK(bitmap);
    std::vector<unsigned char> png;

    gfx::Rect root_bounds = browser()->window()->GetBounds();
    gfx::Rect tab_contents_bounds;
    chrome::GetActiveWebContents(browser())->GetContainerBounds(
        &tab_contents_bounds);

    gfx::Rect snapshot_bounds(tab_contents_bounds.x() - root_bounds.x(),
                              tab_contents_bounds.y() - root_bounds.y(),
                              tab_contents_bounds.width(),
                              tab_contents_bounds.height());

    gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();
    if (!chrome::GrabWindowSnapshotForUser(native_window, &png,
                                           snapshot_bounds)) {
      LOG(ERROR) << "browser::GrabWindowSnapShot() failed";
      return false;
    }

    if (!gfx::PNGCodec::Decode(reinterpret_cast<unsigned char*>(&*png.begin()),
                               png.size(), bitmap)) {
      LOG(ERROR) << "Decode PNG to a SkBitmap failed";
      return false;
    }
    return true;
  }

  // Returns a gfx::Rect representing the bounds that the browser window should
  // have if the tab contents have the desired size.
  gfx::Rect GetNewTabContainerBounds(const gfx::Size& desired_size) {
    gfx::Rect container_rect;
    chrome::GetActiveWebContents(
        browser())->GetContainerBounds(&container_rect);
    // Size cannot be negative, so use a point.
    gfx::Point correction(
        desired_size.width() - container_rect.size().width(),
        desired_size.height() - container_rect.size().height());

    gfx::Rect window_rect = browser()->window()->GetRestoredBounds();
    gfx::Size new_size = window_rect.size();
    new_size.Enlarge(correction.x(), correction.y());
    window_rect.set_size(new_size);
    return window_rect;
  }

  bool CompareColorChannel(uint8_t value,
                           uint8_t expected,
                           int tolerance) {
    int32_t signed_value = value;
    int32_t signed_expected = expected;
    EXPECT_THAT(signed_value, AllOf(
        Ge(signed_expected - tolerance),
        Le(signed_expected + tolerance)));
    return (signed_value >= signed_expected - tolerance &&
            signed_value <= signed_expected + tolerance);
  }

  bool CompareToExpectedResults(
      const SkBitmap& bitmap,
      const std::vector<SinglePixelExpectation>& expectations) {
    SkAutoLockPixels lock_bitmap(bitmap);
    bool result = true;
    for (size_t ii = 0; ii < expectations.size(); ++ii) {
      const SinglePixelExpectation& expectation = expectations[ii];
      SkColor color = bitmap.getColor(expectation.location.x(),
                                      expectation.location.y());
      result &= CompareColorChannel(SkColorGetR(color),
                                    SkColorGetR(expectation.color),
                                    expectation.tolerance);
      result &= CompareColorChannel(SkColorGetG(color),
                                    SkColorGetG(expectation.color),
                                    expectation.tolerance);
      result &= CompareColorChannel(SkColorGetB(color),
                                    SkColorGetB(expectation.color),
                                    expectation.tolerance);
    }
    return result;
  }

  FRIEND_TEST_ALL_PREFIXES(MapsGLEnduranceTest, TestParseExpectations);

  DISALLOW_COPY_AND_ASSIGN(MapsGLEnduranceTest);
};

IN_PROC_BROWSER_TEST_F(MapsGLEnduranceTest, TestParseExpectations) {
  std::vector<SinglePixelExpectation> expectations;
  ASSERT_TRUE(ReadTestExpectations(
      test_data_dir().AppendASCII("mapsgl_unittest_expectations.json"),
      &expectations));
  ASSERT_EQ(2u, expectations.size());
  // These values are hardcoded in the test data.
  EXPECT_EQ(25, expectations[0].location.x());
  EXPECT_EQ(50, expectations[0].location.y());
  EXPECT_EQ(64u, SkColorGetR(expectations[0].color));
  EXPECT_EQ(128u, SkColorGetG(expectations[0].color));
  EXPECT_EQ(192u, SkColorGetB(expectations[0].color));
  EXPECT_EQ(3, expectations[0].tolerance);

  EXPECT_EQ(1920, expectations[1].location.x());
  EXPECT_EQ(1200, expectations[1].location.y());
  EXPECT_EQ(0u, SkColorGetR(expectations[1].color));
  EXPECT_EQ(255u, SkColorGetG(expectations[1].color));
  EXPECT_EQ(127u, SkColorGetB(expectations[1].color));
  EXPECT_EQ(12, expectations[1].tolerance);
}

// This test is being marked MANUAL so that it does not run
// automatically yet, but can be run on demand with the --run-manual
// command line argument. More work is needed to get the test harness
// running on the bots, and to fix flakiness in the test.
IN_PROC_BROWSER_TEST_F(MapsGLEnduranceTest, MANUAL_SingleRunBasic) {
  // This expects the MapsGL python server to be running.
  RunSingleTest(gfx::Size(1024, 768),
                "http://localhost:8000/basic.html",
                "mapsgl_single_run_basic_expectations.json",
                "single-run-basic");
}
