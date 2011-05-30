// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/browser_with_test_window_test.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace {

const unsigned char blob1[] =
    "12346102356120394751634516591348710478123649165419234519234512349134";

}  // namespace

typedef BrowserWithTestWindowTest PrintPreviewUITest;

// Create/Get a preview tab for initiator tab.
TEST_F(PrintPreviewUITest, PrintPreviewData) {
#if !defined(GOOGLE_CHROME_BUILD) || defined(OS_CHROMEOS)
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnablePrintPreview);
#endif
  ASSERT_TRUE(browser());
  BrowserList::SetLastActive(browser());
  ASSERT_TRUE(BrowserList::GetLastActive());

  browser()->NewTab();
  TabContents* initiator_tab = browser()->GetSelectedTabContents();
  ASSERT_TRUE(initiator_tab);

  scoped_refptr<printing::PrintPreviewTabController>
      controller(new printing::PrintPreviewTabController());
  ASSERT_TRUE(controller);

  TabContents* preview_tab = controller->GetOrCreatePreviewTab(initiator_tab);

  EXPECT_NE(initiator_tab, preview_tab);
  EXPECT_EQ(2, browser()->tab_count());

  PrintPreviewUI* preview_ui =
      reinterpret_cast<PrintPreviewUI*>(preview_tab->web_ui());
  ASSERT_TRUE(preview_ui != NULL);

  scoped_refptr<RefCountedBytes> data(new RefCountedBytes);
  preview_ui->GetPrintPreviewData(&data);
  EXPECT_EQ(NULL, data->front());
  EXPECT_EQ(0U, data->size());

  std::vector<unsigned char> preview_data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<RefCountedBytes> dummy_data(new RefCountedBytes(preview_data));

  preview_ui->SetPrintPreviewData(dummy_data.get());
  preview_ui->GetPrintPreviewData(&data);
  EXPECT_EQ(dummy_data->size(), data->size());
  EXPECT_EQ(dummy_data.get(), data.get());

  // This should not cause any memory leaks.
  dummy_data = new RefCountedBytes();
  preview_ui->SetPrintPreviewData(dummy_data);
}
