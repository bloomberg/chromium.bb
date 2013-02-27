// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/test_utils.h"

namespace {

bool IsShowingWebContentsModalDialog(content::WebContents* tab) {
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(tab);
  return web_contents_modal_dialog_manager->IsShowingDialog();
}

class PrintPreviewTest : public InProcessBrowserTest {
 public:
  PrintPreviewTest() {}

#if !defined(GOOGLE_CHROME_BUILD)
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kEnablePrintPreview);
  }
#endif
};

class WebContentsDestroyedWatcher : public content::WebContentsObserver {
 public:
  WebContentsDestroyedWatcher(content::WebContents* web_contents,
                              const base::Closure& callback)
      : content::WebContentsObserver(web_contents),
        callback_(callback) {}

  virtual void WebContentsDestroyed(content::WebContents* web_contents) {
    callback_.Run();
  }

 private:
  base::Closure callback_;
};

#if defined(OS_MACOSX)  // http://crbug.com/178510 hangs on Mac.
#define MAYBE_InitiatorTabGetsFocusOnPrintPreviewDialogClose \
    DISABLED_InitiatorTabGetsFocusOnPrintPreviewDialogClose
#else
#define MAYBE_InitiatorTabGetsFocusOnPrintPreviewDialogClose \
    InitiatorTabGetsFocusOnPrintPreviewDialogClose
#endif

IN_PROC_BROWSER_TEST_F(PrintPreviewTest,
                       MAYBE_InitiatorTabGetsFocusOnPrintPreviewDialogClose) {
  content::WebContents* initiator_tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  printing::PrintPreviewDialogController* controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(controller);

  printing::PrintViewManager* print_view_manager =
      printing::PrintViewManager::FromWebContents(initiator_tab);
  print_view_manager->PrintPreviewNow(false);

  // Don't call GetOrCreatePreviewDialog since that would create the dialog now.
  // The problem with doing so is that if we close it right away (which this
  // test does), then when the IPC comes back from the renderer in the normal
  // printing workflow that would cause the dialog to get created again (and it
  // would be dispatched, racily, in the second nested message loop below).
  scoped_refptr<content::MessageLoopRunner> dialog_runner =
      new content::MessageLoopRunner;
  controller->set_print_preview_tab_created_callback_for_testing(
      dialog_runner->QuitClosure());
  dialog_runner->Run();

  content::WebContents* preview_dialog =
      controller->GetPrintPreviewForContents(initiator_tab);

  EXPECT_NE(initiator_tab, preview_dialog);
  EXPECT_TRUE(IsShowingWebContentsModalDialog(initiator_tab));
  EXPECT_FALSE(initiator_tab->GetRenderWidgetHostView()->HasFocus());

  PrintPreviewUI* preview_ui = static_cast<PrintPreviewUI*>(
      preview_dialog->GetWebUI()->GetController());
  ASSERT_TRUE(preview_ui != NULL);

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  WebContentsDestroyedWatcher watcher(preview_dialog, runner->QuitClosure());

  preview_ui->OnPrintPreviewDialogClosed();

  runner->Run();

  EXPECT_FALSE(IsShowingWebContentsModalDialog(initiator_tab));
  EXPECT_TRUE(initiator_tab->GetRenderWidgetHostView()->HasFocus());
}
}  // namespace
