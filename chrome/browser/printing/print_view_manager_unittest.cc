// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/print_preview_test.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/test/test_renderer_host.h"

namespace printing {

using PrintViewManagerTest = PrintPreviewTest;

TEST_F(PrintViewManagerTest, PrintSubFrameAndDestroy) {
  chrome::NewTab(browser());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  content::RenderFrameHost* sub_frame =
      content::RenderFrameHostTester::For(web_contents->GetMainFrame())
          ->AppendChild("child");

  PrintViewManager* print_view_manager =
      PrintViewManager::FromWebContents(web_contents);
  ASSERT_TRUE(print_view_manager);
  EXPECT_FALSE(print_view_manager->print_preview_rfh());

  print_view_manager->PrintPreviewNow(sub_frame, false);
  EXPECT_TRUE(print_view_manager->print_preview_rfh());

  content::RenderFrameHostTester::For(sub_frame)->Detach();
  EXPECT_FALSE(print_view_manager->print_preview_rfh());
}

}  // namespace printing
