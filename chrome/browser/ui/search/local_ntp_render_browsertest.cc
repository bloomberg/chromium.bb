// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace {

// Where the captures are stored.
const base::FilePath& GetTestDataDir() {
  CR_DEFINE_STATIC_LOCAL(base::FilePath, dir, ());
  if (dir.empty()) {
    base::PathService::Get(base::DIR_SOURCE_ROOT, &dir);
    dir = dir.AppendASCII("components")
              .AppendASCII("test")
              .AppendASCII("data")
              .AppendASCII("ntp")
              .AppendASCII("render");
  }
  return dir;
}

class LocalNTPRenderTest : public InProcessBrowserTest {
 public:
  LocalNTPRenderTest() {
    // Making sure we are running with the Local NTP.
    feature_list_.InitWithFeatures({features::kUseGoogleLocalNtp}, {});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // This is required for the output to be rendered, then captured.
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
  }

  // Will write |bitmap| as a png in the test data directory with |filename|.
  void OnCapturedBitmap(const std::string& filename, const SkBitmap& bitmap) {
    std::vector<unsigned char> bitmap_data;
    bool result =
        gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &bitmap_data);
    DCHECK(result);
    base::ScopedAllowBlockingForTesting allow_blocking;
    EXPECT_EQ(base::checked_cast<int>(bitmap_data.size()),
              base::WriteFile(GetTestDataDir().AppendASCII(filename),
                              reinterpret_cast<char*>(bitmap_data.data()),
                              bitmap_data.size()));
    run_loop_->Quit();
  }

 protected:
  // Capture run loop operations.
  void ResetRunLoop() { run_loop_ = std::make_unique<base::RunLoop>(); }
  void StartRunLoop() { run_loop_->Run(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

IN_PROC_BROWSER_TEST_F(LocalNTPRenderTest, 1200x800_DefaultMV) {
  const int kViewportWidth = 1200;
  const int kViewportHeight = 800;
  const std::string kCaptureFilename("1200x800_DefaultMV.png");

  // Open an NTP and wait until it has fully loaded (tiles may animate, so we
  // give this some delay).
  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser(),
                                                        /*delay=*/1000);
  content::WebContents* active_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderWidgetHost* render_widget_host =
      active_tab->GetRenderViewHost()->GetWidget();
  content::RenderWidgetHostView* view = render_widget_host->GetView();
  ASSERT_TRUE(view && view->IsSurfaceAvailableForCopy());

  // Resize the view to the desired size.
  view->SetSize(gfx::Size(kViewportWidth, kViewportHeight));

  gfx::Rect copy_rect = gfx::Rect(view->GetViewBounds().size());
  ASSERT_TRUE(!copy_rect.IsEmpty());
  ASSERT_TRUE(view->IsScrollOffsetAtTop());

  ResetRunLoop();
  view->CopyFromSurface(
      copy_rect, copy_rect.size(),
      base::BindOnce(&LocalNTPRenderTest::OnCapturedBitmap,
                     base::Unretained(this), kCaptureFilename));
  StartRunLoop();
}

}  // namespace
