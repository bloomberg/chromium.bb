// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "printing/print_job_constants.h"

namespace {

const unsigned char blob1[] =
    "12346102356120394751634516591348710478123649165419234519234512349134";

size_t GetConstrainedWindowCount(TabContentsWrapper* tab) {
  return tab->constrained_window_tab_helper()->constrained_window_count();
}

}  // namespace

typedef BrowserWithTestWindowTest PrintPreviewUIUnitTest;

// Test crashes on Aura due to initiator tab's native view having no parent.
// http://crbug.com/104284
#if defined(USE_AURA)
#define MAYBE_PrintPreviewDraftPages DISABLED_PrintPreviewDraftPages
#define MAYBE_PrintPreviewData DISABLED_PrintPreviewData
#define MAYBE_GetCurrentPrintPreviewStatus DISABLED_GetCurrentPrintPreviewStatus
#else
#define MAYBE_PrintPreviewData PrintPreviewData
#define MAYBE_PrintPreviewDraftPages PrintPreviewDraftPages
#define MAYBE_GetCurrentPrintPreviewStatus GetCurrentPrintPreviewStatus
#endif
// Create/Get a preview tab for initiator tab.
TEST_F(PrintPreviewUIUnitTest, MAYBE_PrintPreviewData) {
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnablePrintPreview);
  ASSERT_TRUE(browser());
  BrowserList::SetLastActive(browser());
  ASSERT_TRUE(BrowserList::GetLastActive());

  browser()->NewTab();
  TabContentsWrapper* initiator_tab =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(initiator_tab);
  EXPECT_EQ(0U, GetConstrainedWindowCount(initiator_tab));

  printing::PrintPreviewTabController* controller =
      printing::PrintPreviewTabController::GetInstance();
  ASSERT_TRUE(controller);

  initiator_tab->print_view_manager()->PrintPreviewNow();
  TabContentsWrapper* preview_tab =
      controller->GetOrCreatePreviewTab(initiator_tab);

  EXPECT_NE(initiator_tab, preview_tab);
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1U, GetConstrainedWindowCount(initiator_tab));

  PrintPreviewUI* preview_ui =
      reinterpret_cast<PrintPreviewUI*>(preview_tab->web_ui());
  ASSERT_TRUE(preview_ui != NULL);

  scoped_refptr<RefCountedBytes> data;
  preview_ui->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX,
      &data);
  EXPECT_EQ(NULL, data.get());

  std::vector<unsigned char> preview_data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<RefCountedBytes> dummy_data(new RefCountedBytes(preview_data));

  preview_ui->SetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX,
      dummy_data.get());
  preview_ui->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX,
      &data);
  EXPECT_EQ(dummy_data->size(), data->size());
  EXPECT_EQ(dummy_data.get(), data.get());

  // This should not cause any memory leaks.
  dummy_data = new RefCountedBytes();
  preview_ui->SetPrintPreviewDataForIndex(printing::FIRST_PAGE_INDEX,
                                          dummy_data);

  // Clear the preview data.
  preview_ui->ClearAllPreviewData();

  preview_ui->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX,
      &data);
  EXPECT_EQ(NULL, data.get());
}

// Set and get the individual draft pages.
TEST_F(PrintPreviewUIUnitTest, MAYBE_PrintPreviewDraftPages) {
#if !defined(GOOGLE_CHROME_BUILD) || defined(OS_CHROMEOS)
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnablePrintPreview);
#endif
  ASSERT_TRUE(browser());
  BrowserList::SetLastActive(browser());
  ASSERT_TRUE(BrowserList::GetLastActive());

  browser()->NewTab();
  TabContentsWrapper* initiator_tab =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(initiator_tab);

  printing::PrintPreviewTabController* controller =
      printing::PrintPreviewTabController::GetInstance();
  ASSERT_TRUE(controller);

  initiator_tab->print_view_manager()->PrintPreviewNow();
  TabContentsWrapper* preview_tab =
      controller->GetOrCreatePreviewTab(initiator_tab);

  EXPECT_NE(initiator_tab, preview_tab);
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1U, GetConstrainedWindowCount(initiator_tab));

  PrintPreviewUI* preview_ui =
      reinterpret_cast<PrintPreviewUI*>(preview_tab->web_ui());
  ASSERT_TRUE(preview_ui != NULL);

  scoped_refptr<RefCountedBytes> data;
  preview_ui->GetPrintPreviewDataForIndex(printing::FIRST_PAGE_INDEX, &data);
  EXPECT_EQ(NULL, data.get());

  std::vector<unsigned char> preview_data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<RefCountedBytes> dummy_data(new RefCountedBytes(preview_data));

  preview_ui->SetPrintPreviewDataForIndex(printing::FIRST_PAGE_INDEX,
                                          dummy_data.get());
  preview_ui->GetPrintPreviewDataForIndex(printing::FIRST_PAGE_INDEX, &data);
  EXPECT_EQ(dummy_data->size(), data->size());
  EXPECT_EQ(dummy_data.get(), data.get());

  // Set and get the third page data.
  preview_ui->SetPrintPreviewDataForIndex(printing::FIRST_PAGE_INDEX + 2,
                                          dummy_data.get());
  preview_ui->GetPrintPreviewDataForIndex(printing::FIRST_PAGE_INDEX + 2,
                                          &data);
  EXPECT_EQ(dummy_data->size(), data->size());
  EXPECT_EQ(dummy_data.get(), data.get());

  // Get the second page data.
  preview_ui->GetPrintPreviewDataForIndex(printing::FIRST_PAGE_INDEX + 1,
                                          &data);
  EXPECT_EQ(NULL, data.get());

  preview_ui->SetPrintPreviewDataForIndex(printing::FIRST_PAGE_INDEX + 1,
                                          dummy_data.get());
  preview_ui->GetPrintPreviewDataForIndex(printing::FIRST_PAGE_INDEX + 1,
                                          &data);
  EXPECT_EQ(dummy_data->size(), data->size());
  EXPECT_EQ(dummy_data.get(), data.get());

  // Clear the preview data.
  preview_ui->ClearAllPreviewData();
  preview_ui->GetPrintPreviewDataForIndex(printing::FIRST_PAGE_INDEX, &data);
  EXPECT_EQ(NULL, data.get());
}

// Test the browser-side print preview cancellation functionality.
TEST_F(PrintPreviewUIUnitTest, MAYBE_GetCurrentPrintPreviewStatus) {
#if !defined(GOOGLE_CHROME_BUILD) || defined(OS_CHROMEOS)
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnablePrintPreview);
#endif
  ASSERT_TRUE(browser());
  BrowserList::SetLastActive(browser());
  ASSERT_TRUE(BrowserList::GetLastActive());

  browser()->NewTab();
  TabContentsWrapper* initiator_tab =
      browser()->GetSelectedTabContentsWrapper();
  ASSERT_TRUE(initiator_tab);

  printing::PrintPreviewTabController* controller =
      printing::PrintPreviewTabController::GetInstance();
  ASSERT_TRUE(controller);

  initiator_tab->print_view_manager()->PrintPreviewNow();
  TabContentsWrapper* preview_tab =
      controller->GetOrCreatePreviewTab(initiator_tab);

  EXPECT_NE(initiator_tab, preview_tab);
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1U, GetConstrainedWindowCount(initiator_tab));

  PrintPreviewUI* preview_ui =
      reinterpret_cast<PrintPreviewUI*>(preview_tab->web_ui());
  ASSERT_TRUE(preview_ui != NULL);

  // Test with invalid |preview_ui_addr|.
  bool cancel = false;
  preview_ui->GetCurrentPrintPreviewStatus("invalid", 0, &cancel);
  EXPECT_TRUE(cancel);

  const int kFirstRequestId = 1000;
  const int kSecondRequestId = 1001;
  const std::string preview_ui_addr = preview_ui->GetPrintPreviewUIAddress();

  // Test with kFirstRequestId.
  preview_ui->OnPrintPreviewRequest(kFirstRequestId);
  cancel = true;
  preview_ui->GetCurrentPrintPreviewStatus(preview_ui_addr, kFirstRequestId,
                                           &cancel);
  EXPECT_FALSE(cancel);

  cancel = false;
  preview_ui->GetCurrentPrintPreviewStatus(preview_ui_addr, kSecondRequestId,
                                           &cancel);
  EXPECT_TRUE(cancel);

  // Test with kSecondRequestId.
  preview_ui->OnPrintPreviewRequest(kSecondRequestId);
  cancel = false;
  preview_ui->GetCurrentPrintPreviewStatus(preview_ui_addr, kFirstRequestId,
                                           &cancel);
  EXPECT_TRUE(cancel);

  cancel = true;
  preview_ui->GetCurrentPrintPreviewStatus(preview_ui_addr, kSecondRequestId,
                                           &cancel);
  EXPECT_FALSE(cancel);
}
