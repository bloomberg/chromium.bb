// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/print_messages.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "url/gurl.h"
#include "ipc/ipc_message_macros.h"

using content::WebContents;
using content::WebContentsObserver;

class RequestPrintPreviewObserver : public WebContentsObserver {
 public:
  explicit RequestPrintPreviewObserver(WebContents* dialog)
      : WebContentsObserver(dialog) {
  }
  virtual ~RequestPrintPreviewObserver() {}

  void set_quit_closure(const base::Closure& quit_closure) {
    quit_closure_ = quit_closure;
  }

 private:
  // content::WebContentsObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    IPC_BEGIN_MESSAGE_MAP(RequestPrintPreviewObserver, message)
      IPC_MESSAGE_HANDLER(PrintHostMsg_RequestPrintPreview,
                          OnRequestPrintPreview)
      IPC_MESSAGE_UNHANDLED(break;)
    IPC_END_MESSAGE_MAP();
    return false;  // Report not handled so the real handler receives it.
  }

  void OnRequestPrintPreview(
      const PrintHostMsg_RequestPrintPreview_Params& /* params */) {
    base::MessageLoop::current()->PostTask(FROM_HERE, quit_closure_);
  }

  base::Closure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(RequestPrintPreviewObserver);
};

class PrintPreviewDialogClonedObserver : public WebContentsObserver {
 public:
  explicit PrintPreviewDialogClonedObserver(WebContents* dialog)
      : WebContentsObserver(dialog) {
  }
  virtual ~PrintPreviewDialogClonedObserver() {}

  RequestPrintPreviewObserver* request_preview_dialog_observer() {
    return request_preview_dialog_observer_.get();
  }

 private:
  // content::WebContentsObserver implementation.
  virtual void DidCloneToNewWebContents(
      WebContents* old_web_contents,
      WebContents* new_web_contents) OVERRIDE {
    request_preview_dialog_observer_.reset(
        new RequestPrintPreviewObserver(new_web_contents));
  }

  scoped_ptr<RequestPrintPreviewObserver> request_preview_dialog_observer_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDialogClonedObserver);
};

class PrintPreviewDialogDestroyedObserver : public WebContentsObserver {
 public:
  explicit PrintPreviewDialogDestroyedObserver(WebContents* dialog)
      : WebContentsObserver(dialog),
        dialog_destroyed_(false) {
  }
  virtual ~PrintPreviewDialogDestroyedObserver() {}

  bool dialog_destroyed() const { return dialog_destroyed_; }

 private:
  // content::WebContentsObserver implementation.
  virtual void WebContentsDestroyed() OVERRIDE {
    dialog_destroyed_ = true;
  }

  bool dialog_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDialogDestroyedObserver);
};

class PrintPreviewDialogControllerBrowserTest : public InProcessBrowserTest {
 public:
  PrintPreviewDialogControllerBrowserTest() : initiator_(NULL) {}
  virtual ~PrintPreviewDialogControllerBrowserTest() {}

  WebContents* initiator() {
    return initiator_;
  }

  void PrintPreview() {
    base::RunLoop run_loop;
    request_preview_dialog_observer()->set_quit_closure(run_loop.QuitClosure());
    chrome::Print(browser());
    run_loop.Run();
  }

  WebContents* GetPrintPreviewDialog() {
    printing::PrintPreviewDialogController* dialog_controller =
        printing::PrintPreviewDialogController::GetInstance();
    return dialog_controller->GetPrintPreviewForContents(initiator_);
  }

 private:
  virtual void SetUpOnMainThread() OVERRIDE {
    WebContents* first_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(first_tab);

    // Open a new tab so |cloned_tab_observer_| can see it first and attach a
    // RequestPrintPreviewObserver to it before the real
    // PrintPreviewMessageHandler gets created. Thus enabling
    // RequestPrintPreviewObserver to get messages first for the purposes of
    // this test.
    cloned_tab_observer_.reset(new PrintPreviewDialogClonedObserver(first_tab));
    chrome::DuplicateTab(browser());

    initiator_ = browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(initiator_);
    ASSERT_NE(first_tab, initiator_);
  }

  virtual void TearDownOnMainThread() OVERRIDE {
    cloned_tab_observer_.reset();
    initiator_ = NULL;
  }

  RequestPrintPreviewObserver* request_preview_dialog_observer() {
    return cloned_tab_observer_->request_preview_dialog_observer();
  }

  scoped_ptr<PrintPreviewDialogClonedObserver> cloned_tab_observer_;
  WebContents* initiator_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDialogControllerBrowserTest);
};

// Test to verify that when a initiator navigates, we can create a new preview
// dialog for the new tab contents.
// http://crbug.com/377337
#if defined(OS_WIN)
#define MAYBE_NavigateFromInitiatorTab DISABLED_NavigateFromInitiatorTab
#else
#define MAYBE_NavigateFromInitiatorTab NavigateFromInitiatorTab
#endif
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       MAYBE_NavigateFromInitiatorTab) {
  // print for the first time.
  PrintPreview();

  // Get the preview dialog for the initiator tab.
  WebContents* preview_dialog = GetPrintPreviewDialog();

  // Check a new print preview dialog got created.
  ASSERT_TRUE(preview_dialog);
  ASSERT_NE(initiator(), preview_dialog);

  // Navigate in the initiator tab. Make sure navigating destroys the print
  // preview dialog.
  PrintPreviewDialogDestroyedObserver dialog_destroyed_observer(preview_dialog);
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(dialog_destroyed_observer.dialog_destroyed());

  // Try printing again.
  PrintPreview();

  // Get the print preview dialog for the initiator tab.
  WebContents* new_preview_dialog = GetPrintPreviewDialog();

  // Check a new preview dialog got created.
  EXPECT_TRUE(new_preview_dialog);
}

// Test to verify that after reloading the initiator, it creates a new print
// preview dialog.
// http://crbug.com/377337
#if defined(OS_WIN)
#define MAYBE_ReloadInitiatorTab DISABLED_ReloadInitiatorTab
#else
#define MAYBE_ReloadInitiatorTab ReloadInitiatorTab
#endif
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       MAYBE_ReloadInitiatorTab) {
  // print for the first time.
  PrintPreview();

  WebContents* preview_dialog = GetPrintPreviewDialog();

  // Check a new print preview dialog got created.
  ASSERT_TRUE(preview_dialog);
  ASSERT_NE(initiator(), preview_dialog);

  // Reload the initiator. Make sure reloading destroys the print preview
  // dialog.
  PrintPreviewDialogDestroyedObserver dialog_destroyed_observer(preview_dialog);
  chrome::Reload(browser(), CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  ASSERT_TRUE(dialog_destroyed_observer.dialog_destroyed());

  // Try printing again.
  PrintPreview();

  // Create a preview dialog for the initiator tab.
  WebContents* new_preview_dialog = GetPrintPreviewDialog();
  EXPECT_TRUE(new_preview_dialog);
}
