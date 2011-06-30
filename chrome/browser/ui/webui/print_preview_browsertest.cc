// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/webui/web_ui_browsertest.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/test_tab_strip_model_observer.h"
#include "chrome/test/ui_test_utils.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// TODO(scr) migrate this to ui_test_utils?
// Cause Print on the currently selected tab of |browser|, blocking until the
// Print Preview tab shows up.
void PrintAndWaitForPrintPreviewTab(
    Browser* browser,
    TestTabStripModelObserver::LoadStartObserver* load_start_observer) {
  TestTabStripModelObserver tabstrip_observer(browser->tabstrip_model(),
                                              load_start_observer);
  TabContents* initiator_tab_contents = browser->GetSelectedTabContents();
  browser->Print();
  tabstrip_observer.WaitForObservation();
  EXPECT_NE(initiator_tab_contents, browser->GetSelectedTabContents());
}

}  // namespace

class PrintPreviewWebUITest
    : public WebUIBrowserTest,
      public TestTabStripModelObserver::LoadStartObserver {
 protected:
  // WebUIBrowserTest:
  virtual void SetUpOnMainThread() OVERRIDE {
    WebUIBrowserTest::SetUpOnMainThread();
    if (!HasPDFLib())
      skipTest(base::StringPrintf("%s:%d: No PDF Lib.", __FILE__, __LINE__));

    AddLibrary(FilePath(FILE_PATH_LITERAL("print_preview.js")));
    GURL print_preview_test_url = WebUITestDataPathToURL(
        FILE_PATH_LITERAL("print_preview_hello_world_test.html"));
    ui_test_utils::NavigateToURL(browser(), print_preview_test_url);
    PrintAndWaitForPrintPreviewTab(browser(), this);
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    WebUIBrowserTest::SetUpCommandLine(command_line);
#if !defined(GOOGLE_CHROME_BUILD) || defined(OS_CHROMEOS) || defined(OS_MACOSX)
    // Don't enable the flag for chrome builds, which should be on by default.
    command_line->AppendSwitch(switches::kEnablePrintPreview);
#else
    ASSERT_TRUE(switches::IsPrintPreviewEnabled());
#endif
  }

  // TestTabStripModelObserver::LoadStartObserver:
  virtual void OnLoadStart() OVERRIDE {
    PreLoadJavascriptLibraries(true);
  }

  bool HasPDFLib() const {
    FilePath pdf;
    return PathService::Get(chrome::FILE_PDF_PLUGIN, &pdf) &&
        file_util::PathExists(pdf);
  }
};

IN_PROC_BROWSER_TEST_F(PrintPreviewWebUITest, TestPrintPreview) {
  ASSERT_TRUE(RunJavascriptTest("testPrintPreview",
                                *Value::CreateBooleanValue(HasPDFLib())));
}
