// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/printing/background_printing_manager.h"
#include "chrome/browser/printing/print_preview_tab_controller.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/print_preview_handler.h"
#include "chrome/browser/ui/webui/print_preview_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "printing/print_job_constants.h"

typedef BrowserWithTestWindowTest PrintPreviewHandlerTest;

// When users hit print in the print preview tab, the print preview tab hides
// and the focus should return to the initiator tab.
TEST_F(PrintPreviewHandlerTest, ActivateInitiatorTabOnPrint) {
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

  TabContentsWrapper* preview_tab =
      controller->GetOrCreatePreviewTab(initiator_tab);
  EXPECT_EQ(2, browser()->tab_count());

  browser()->NewTab();
  EXPECT_EQ(3, browser()->tab_count());

  PrintPreviewUI* preview_ui =
      static_cast<PrintPreviewUI*>(preview_tab->web_ui());
  ASSERT_TRUE(preview_ui);

  // Set the minimal dummy settings to make the HandlePrint() code happy.
  DictionaryValue value;
  value.SetBoolean(printing::kSettingColor, false);
  value.SetBoolean(printing::kSettingPrintToPDF, false);

  // Put |value| in to |args| as a JSON string.
  std::string json_string;
  base::JSONWriter::Write(&value, false, &json_string);
  ListValue args;
  args.Append(new StringValue(json_string));  // |args| takes ownership.
  preview_ui->handler_->HandlePrint(&args);

  EXPECT_EQ(2, browser()->tab_count());
  EXPECT_EQ(initiator_tab, browser()->GetSelectedTabContentsWrapper());

  printing::BackgroundPrintingManager* bg_printing_manager =
      g_browser_process->background_printing_manager();
  EXPECT_TRUE(bg_printing_manager->HasPrintPreviewTab(preview_tab));
}
