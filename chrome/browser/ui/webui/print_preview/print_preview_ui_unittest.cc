// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/printing/print_preview_unit_test_base.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "printing/print_job_constants.h"

using content::WebContents;
using content::WebContentsTester;

namespace {

const unsigned char blob1[] =
    "12346102356120394751634516591348710478123649165419234519234512349134";

size_t GetConstrainedWindowCount(TabContents* tab) {
  return tab->constrained_window_tab_helper()->constrained_window_count();
}

}  // namespace

class PrintPreviewUIUnitTest : public PrintPreviewUnitTestBase {
 public:
  PrintPreviewUIUnitTest() {}
  virtual ~PrintPreviewUIUnitTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    PrintPreviewUnitTestBase::SetUp();

    browser()->NewTab();
  }
};

// Create/Get a preview tab for initiator tab.
TEST_F(PrintPreviewUIUnitTest, PrintPreviewData) {
  TabContents* initiator_tab = browser()->GetActiveTabContents();
  ASSERT_TRUE(initiator_tab);
  EXPECT_EQ(0U, GetConstrainedWindowCount(initiator_tab));

  printing::PrintPreviewTabController* controller =
      printing::PrintPreviewTabController::GetInstance();
  ASSERT_TRUE(controller);

  initiator_tab->print_view_manager()->PrintPreviewNow();
  TabContents* preview_tab = controller->GetOrCreatePreviewTab(initiator_tab);

  EXPECT_NE(initiator_tab, preview_tab);
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1U, GetConstrainedWindowCount(initiator_tab));

  PrintPreviewUI* preview_ui = static_cast<PrintPreviewUI*>(
      preview_tab->web_contents()->GetWebUI()->GetController());
  ASSERT_TRUE(preview_ui != NULL);

  scoped_refptr<base::RefCountedBytes> data;
  preview_ui->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX,
      &data);
  EXPECT_EQ(NULL, data.get());

  std::vector<unsigned char> preview_data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<base::RefCountedBytes> dummy_data =
      new base::RefCountedBytes(preview_data);

  preview_ui->SetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX,
      dummy_data.get());
  preview_ui->GetPrintPreviewDataForIndex(
      printing::COMPLETE_PREVIEW_DOCUMENT_INDEX,
      &data);
  EXPECT_EQ(dummy_data->size(), data->size());
  EXPECT_EQ(dummy_data.get(), data.get());

  // This should not cause any memory leaks.
  dummy_data = new base::RefCountedBytes();
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
TEST_F(PrintPreviewUIUnitTest, PrintPreviewDraftPages) {
  TabContents* initiator_tab = browser()->GetActiveTabContents();
  ASSERT_TRUE(initiator_tab);

  printing::PrintPreviewTabController* controller =
      printing::PrintPreviewTabController::GetInstance();
  ASSERT_TRUE(controller);

  initiator_tab->print_view_manager()->PrintPreviewNow();
  TabContents* preview_tab = controller->GetOrCreatePreviewTab(initiator_tab);

  EXPECT_NE(initiator_tab, preview_tab);
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1U, GetConstrainedWindowCount(initiator_tab));

  PrintPreviewUI* preview_ui = static_cast<PrintPreviewUI*>(
      preview_tab->web_contents()->GetWebUI()->GetController());
  ASSERT_TRUE(preview_ui != NULL);

  scoped_refptr<base::RefCountedBytes> data;
  preview_ui->GetPrintPreviewDataForIndex(printing::FIRST_PAGE_INDEX, &data);
  EXPECT_EQ(NULL, data.get());

  std::vector<unsigned char> preview_data(blob1, blob1 + sizeof(blob1));
  scoped_refptr<base::RefCountedBytes> dummy_data =
      new base::RefCountedBytes(preview_data);

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
TEST_F(PrintPreviewUIUnitTest, GetCurrentPrintPreviewStatus) {
  TabContents* initiator_tab = browser()->GetActiveTabContents();
  ASSERT_TRUE(initiator_tab);

  printing::PrintPreviewTabController* controller =
      printing::PrintPreviewTabController::GetInstance();
  ASSERT_TRUE(controller);

  initiator_tab->print_view_manager()->PrintPreviewNow();
  TabContents* preview_tab = controller->GetOrCreatePreviewTab(initiator_tab);

  EXPECT_NE(initiator_tab, preview_tab);
  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1U, GetConstrainedWindowCount(initiator_tab));

  PrintPreviewUI* preview_ui = static_cast<PrintPreviewUI*>(
      preview_tab->web_contents()->GetWebUI()->GetController());
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

TEST_F(PrintPreviewUIUnitTest, InitiatorTabGetsFocusOnPrintPreviewTabClose) {
  EXPECT_EQ(1, browser()->tab_count());
  WebContents* initiator_contents =
      WebContentsTester::CreateTestWebContentsCountFocus(profile(), NULL);
  WebContentsTester* initiator_tester =
      WebContentsTester::For(initiator_contents);
  browser()->AddWebContents(initiator_contents,
                            NEW_FOREGROUND_TAB,
                            gfx::Rect(),
                            false);
  TabContents* initiator_tab = TabContents::FromWebContents(initiator_contents);
  ASSERT_TRUE(initiator_tab);
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(0, initiator_tester->GetNumberOfFocusCalls());

  printing::PrintPreviewTabController* controller =
      printing::PrintPreviewTabController::GetInstance();
  ASSERT_TRUE(controller);

  initiator_tab->print_view_manager()->PrintPreviewNow();
  TabContents* preview_tab = controller->GetOrCreatePreviewTab(initiator_tab);

  EXPECT_NE(initiator_tab, preview_tab);
  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(1U, GetConstrainedWindowCount(initiator_tab));
  EXPECT_EQ(0, initiator_tester->GetNumberOfFocusCalls());

  PrintPreviewUI* preview_ui = static_cast<PrintPreviewUI*>(
      preview_tab->web_contents()->GetWebUI()->GetController());
  ASSERT_TRUE(preview_ui != NULL);

  preview_ui->OnPrintPreviewTabClosed();

  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(0U, GetConstrainedWindowCount(initiator_tab));
  EXPECT_EQ(1, initiator_tester->GetNumberOfFocusCalls());
}
