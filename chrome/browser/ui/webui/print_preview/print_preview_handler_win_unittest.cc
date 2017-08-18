// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/print_preview/print_preview_handler.h"

#include <commdlg.h>
#include <windows.h>

#include "base/run_loop.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/printing/print_preview_dialog_controller.h"
#include "chrome/browser/printing/print_preview_test.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/shell_dialogs/select_file_dialog_win.h"
#include "ui/shell_dialogs/select_file_policy.h"

using content::WebContents;

namespace {

class FakePrintPreviewHandler;
bool GetOpenFileNameImpl(OPENFILENAME* ofn);
bool GetSaveFileNameImpl(FakePrintPreviewHandler* handler, OPENFILENAME* ofn);

class FakePrintPreviewHandler : public PrintPreviewHandler {
 public:
  explicit FakePrintPreviewHandler(content::WebUI* web_ui)
      : init_called_(false), save_failed_(false) {
    set_web_ui(web_ui);
  }

  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override {
    // Since we always cancel the dialog as soon as it is initialized, this
    // should never be called.
    NOTREACHED();
  }

  void FileSelectionCanceled(void* params) override {
    save_failed_ = true;
    run_loop_.Quit();
  }

  void StartPrintToPdf() {
    PrintToPdf();
    run_loop_.Run();
  }

  bool save_failed() const { return save_failed_; }

  bool init_called() const { return init_called_; }

  void set_init_called() { init_called_ = true; }

 private:
  // Simplified version of select file to avoid checking preferences and sticky
  // settings in the test
  void SelectFile(const base::FilePath& default_filename,
                  bool prompt_user) override {
    ui::SelectFileDialog::FileTypeInfo file_type_info;
    file_type_info.extensions.resize(1);
    file_type_info.extensions[0].push_back(FILE_PATH_LITERAL("pdf"));
    select_file_dialog_ = ui::CreateWinSelectFileDialog(
        this, nullptr /*policy already checked*/,
        base::Bind(GetOpenFileNameImpl), base::Bind(GetSaveFileNameImpl, this));
    select_file_dialog_->SelectFile(
        ui::SelectFileDialog::SELECT_SAVEAS_FILE, base::string16(),
        default_filename, &file_type_info, 0, base::FilePath::StringType(),
        platform_util::GetTopLevel(web_ui()->GetWebContents()->GetNativeView()),
        nullptr);
  }

  bool init_called_;
  bool save_failed_;
  base::RunLoop run_loop_;
};

// Hook function to cancel the dialog when it is successfully initialized.
UINT_PTR CALLBACK PrintPreviewHandlerTestHookFunction(HWND hdlg,
                                                      UINT message,
                                                      WPARAM wparam,
                                                      LPARAM lparam) {
  if (message != WM_INITDIALOG)
    return 0;
  OPENFILENAME* ofn = reinterpret_cast<OPENFILENAME*>(lparam);
  FakePrintPreviewHandler* handler =
      reinterpret_cast<FakePrintPreviewHandler*>(ofn->lCustData);
  handler->set_init_called();
  PostMessage(GetParent(hdlg), WM_COMMAND, MAKEWPARAM(IDCANCEL, 0), 0);
  return 1;
}

bool GetOpenFileNameImpl(OPENFILENAME* ofn) {
  return ::GetOpenFileName(ofn);
}

bool GetSaveFileNameImpl(FakePrintPreviewHandler* handler, OPENFILENAME* ofn) {
  // Modify ofn so that the hook function will be called.
  ofn->Flags |= OFN_ENABLEHOOK;
  ofn->lpfnHook = PrintPreviewHandlerTestHookFunction;
  ofn->lCustData = reinterpret_cast<LPARAM>(handler);
  return ::GetSaveFileName(ofn);
}

}  // namespace

class PrintPreviewHandlerTest : public PrintPreviewTest {
 public:
  PrintPreviewHandlerTest() : preview_ui_(nullptr) {}
  ~PrintPreviewHandlerTest() override {}

  void SetUp() override {
    PrintPreviewTest::SetUp();

    // Create a new tab
    chrome::NewTab(browser());
  }

 protected:
  void CreateUIAndHandler() {
    WebContents* initiator =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(initiator);

    // Get print preview UI
    printing::PrintPreviewDialogController* controller =
        printing::PrintPreviewDialogController::GetInstance();
    ASSERT_TRUE(controller);
    printing::PrintViewManager* print_view_manager =
        printing::PrintViewManager::FromWebContents(initiator);
    print_view_manager->PrintPreviewNow(initiator->GetMainFrame(), false);
    WebContents* preview_dialog =
        controller->GetOrCreatePreviewDialog(initiator);
    ASSERT_TRUE(preview_dialog);
    preview_ui_ = static_cast<PrintPreviewUI*>(
        preview_dialog->GetWebUI()->GetController());
    ASSERT_TRUE(preview_ui_);

    preview_handler_ =
        base::MakeUnique<FakePrintPreviewHandler>(preview_dialog->GetWebUI());
  }

  std::unique_ptr<FakePrintPreviewHandler> preview_handler_;
  PrintPreviewUI* preview_ui_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrintPreviewHandlerTest);
};

TEST_F(PrintPreviewHandlerTest, TestSaveAsPdf) {
  CreateUIAndHandler();
  preview_ui_->SetInitiatorTitle(L"111111111111111111111.html");
  preview_handler_->StartPrintToPdf();
  EXPECT_TRUE(preview_handler_->init_called());
  EXPECT_TRUE(preview_handler_->save_failed());
}

TEST_F(PrintPreviewHandlerTest, TestSaveAsPdfLongFileName) {
  CreateUIAndHandler();
  preview_ui_->SetInitiatorTitle(
      L"11111111111111111111111111111111111111111111111111111111111111111111111"
      L"11111111111111111111111111111111111111111111111111111111111111111111111"
      L"11111111111111111111111111111111111111111111111111111111111111111111111"
      L"11111111111111111111111111111111111111111111111111111111111111111111111"
      L"1111111111111111111111111111111111111111111111111.html");
  preview_handler_->StartPrintToPdf();
  EXPECT_TRUE(preview_handler_->init_called());
  EXPECT_TRUE(preview_handler_->save_failed());
}
