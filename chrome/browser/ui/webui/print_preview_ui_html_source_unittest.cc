// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/shared_memory.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/browser/ui/webui/print_preview_ui_html_source.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/testing_profile.h"

typedef BrowserWithTestWindowTest PrintPreviewUIHTMLSourceTest;

// Create/Get a preview tab for initiator tab.
TEST_F(PrintPreviewUIHTMLSourceTest, PrintPreviewData) {
  // TODO(thestig) Remove when print preview is enabled by default.
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnablePrintPreview);
  ASSERT_TRUE(browser());
  BrowserList::SetLastActive(browser());
  ASSERT_TRUE(BrowserList::GetLastActive());

  browser()->NewTab();
  TabContents* initiator_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(initiator_tab);

  scoped_refptr<printing::PrintPreviewTabController>
      controller(new printing::PrintPreviewTabController());
  ASSERT_TRUE(controller);

  TabContents* preview_tab = controller->GetOrCreatePreviewTab(
      initiator_tab, initiator_tab->controller().window_id().id());

  EXPECT_NE(initiator_tab, preview_tab);
  EXPECT_EQ(2, browser()->tab_count());

  PrintPreviewUI* preview_ui =
      reinterpret_cast<PrintPreviewUI*>(preview_tab->web_ui());
  ASSERT_TRUE(preview_ui != NULL);
  PrintPreviewUIHTMLSource* html_source = preview_ui->html_source();

  PrintPreviewUIHTMLSource::PrintPreviewData data;
  html_source->GetPrintPreviewData(&data);
  EXPECT_EQ(NULL, data.first);
  EXPECT_EQ(0U, data.second);

  PrintPreviewUIHTMLSource::PrintPreviewData dummy_data =
      std::make_pair(new base::SharedMemory(), 1234);

  html_source->SetPrintPreviewData(dummy_data);
  html_source->GetPrintPreviewData(&data);
  EXPECT_EQ(dummy_data, data);

  // This should not cause any memory leaks.
  dummy_data.first = new base::SharedMemory();
  html_source->SetPrintPreviewData(dummy_data);
}
