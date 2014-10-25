// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_view_model.h"
#include "athena/resource_manager/public/resource_manager.h"
#include "athena/test/chrome/athena_browsertest.h"
#include "athena/test/chrome/test_util.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/size.h"
#include "url/gurl.h"

namespace athena {

namespace {
// The test URL to navigate to.
const char kTestUrl[] = "chrome:about";
}

// Need to override the test class to make the test always draw its content.
class ContentProxyBrowserTest : public AthenaBrowserTest {
 public:
  ContentProxyBrowserTest() {}
  ~ContentProxyBrowserTest() override {}

  // AthenaBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Make sure that we draw the output - it's required for this test.
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
    AthenaBrowserTest::SetUpCommandLine(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentProxyBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ContentProxyBrowserTest, CreateContent) {
  const int kTimeoutMS = 12000;  // The timeout: 2 seconds.
  const int kIterationSleepMS = 5;  // The wait time in ms per iteration.
  const GURL gurl(kTestUrl);
  content::BrowserContext* context = test_util::GetBrowserContext();
  // Create an activity (and wait until it is loaded).
  // The size of its overview image should be empty since it is visible.
  Activity* activity1 =
      test_util::CreateTestWebActivity(context,
                                       base::UTF8ToUTF16("App1"),
                                       gurl);

  DCHECK_EQ(activity1->GetActivityViewModel()->GetOverviewModeImage()
                .size().ToString(),
            gfx::Size().ToString());

  // Create another activity. The size of all overview images should be empty
  // since they have the visible state.
  Activity* activity2 =
      test_util::CreateTestWebActivity(context,
                                       base::UTF8ToUTF16("App2"),
                                       gurl);
  DCHECK_EQ(activity1->GetActivityViewModel()->GetOverviewModeImage()
                .size().ToString(),
            gfx::Size().ToString());
  DCHECK_EQ(activity2->GetActivityViewModel()->GetOverviewModeImage()
                .size().ToString(),
            gfx::Size().ToString());

  // Turn the activity invisible which should create the ContentProxy.
  activity1->SetCurrentState(Activity::ACTIVITY_INVISIBLE);
  test_util::WaitUntilIdle();
  DCHECK_EQ(activity1->GetCurrentState(), Activity::ACTIVITY_INVISIBLE);

  // Wait until an image is loaded, but do not give more then 2 seconds.
  gfx::ImageSkia image;
  int iteration = 0;
  while (image.isNull()) {
    // TODO(skuhne): If we add an observer to track the creation, we should add
    // its use here.
    image = activity1->GetActivityViewModel()->GetOverviewModeImage();
    if (++iteration > kTimeoutMS / kIterationSleepMS) {
      LOG(ERROR) << "Timout on reading back the content image.";
      break;
    }
    test_util::WaitUntilIdle();
    usleep(1000 * kIterationSleepMS);
  }

  // Check that the image of the old activity has now a usable size.
  DCHECK_NE(activity1->GetActivityViewModel()->GetOverviewModeImage()
                .size().ToString(),
            gfx::Size().ToString());
  DCHECK_EQ(activity2->GetActivityViewModel()->GetOverviewModeImage()
                .size().ToString(),
            gfx::Size().ToString());

  // After the Activity gets entirely unloaded, the image should still be there.
  activity1->SetCurrentState(Activity::ACTIVITY_UNLOADED);
  test_util::WaitUntilIdle();
  DCHECK_EQ(activity1->GetCurrentState(), Activity::ACTIVITY_UNLOADED);

  DCHECK_NE(activity1->GetActivityViewModel()->GetOverviewModeImage()
                .size().ToString(),
            gfx::Size().ToString());

  // When it then becomes visible again, the image will be gone.
  activity1->SetCurrentState(Activity::ACTIVITY_VISIBLE);
  test_util::WaitUntilIdle();
  DCHECK_EQ(activity1->GetCurrentState(), Activity::ACTIVITY_VISIBLE);

  DCHECK_EQ(activity1->GetActivityViewModel()->GetOverviewModeImage()
                .size().ToString(),
            gfx::Size().ToString());
}

}  // namespace athena
