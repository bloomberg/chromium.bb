// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"
#include "base/run_loop.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "net/base/net_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

class RenderWidgetHostBrowserTest : public ContentBrowserTest {
 public:
  RenderWidgetHostBrowserTest() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ASSERT_TRUE(PathService::Get(DIR_TEST_DATA, &test_dir_));
  }

  void GetSnapshotFromRendererCallback(const base::Closure& quit_closure,
                                       bool* snapshot_valid,
                                       bool success,
                                       const SkBitmap& bitmap) {
    quit_closure.Run();
    EXPECT_EQ(success, true);

    const int row_bytes = bitmap.rowBytesAsPixels();
    SkColor* pixels = reinterpret_cast<SkColor*>(bitmap.getPixels());
    for (int i = 0; i < bitmap.width(); ++i) {
      for (int j = 0; j < bitmap.height(); ++j) {
        if (pixels[j * row_bytes + i] != SK_ColorRED) {
          return;
        }
      }
    }
    *snapshot_valid = true;
  }

 protected:
  base::FilePath test_dir_;
};

IN_PROC_BROWSER_TEST_F(RenderWidgetHostBrowserTest,
                       GetSnapshotFromRendererTest) {
  base::RunLoop run_loop;

  NavigateToURL(shell(), GURL(net::FilePathToFileURL(
      test_dir_.AppendASCII("rwh_simple.html"))));

  bool snapshot_valid = false;
  RenderViewHost* const rwh = shell()->web_contents()->GetRenderViewHost();
  rwh->GetSnapshotFromRenderer(gfx::Rect(), base::Bind(
      &RenderWidgetHostBrowserTest::GetSnapshotFromRendererCallback,
      base::Unretained(this),
      run_loop.QuitClosure(),
      &snapshot_valid));
  run_loop.Run();

  EXPECT_EQ(snapshot_valid, true);
}

}  // namespace content
