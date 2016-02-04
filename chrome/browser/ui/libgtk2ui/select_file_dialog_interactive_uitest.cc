// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtk2ui/select_file_dialog_impl_gtk2.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

using BrowserSelectFileDialogTest = InProcessBrowserTest;

// Spins a run loop until a Widget's activation reaches the desired state.
class WidgetActivationWaiter : public views::WidgetObserver {
 public:
  WidgetActivationWaiter(views::Widget* widget, bool active)
      : observed_(false), active_(active) {
    widget->AddObserver(this);
    EXPECT_NE(active, widget->IsActive());
  }

  void Wait() {
    if (!observed_)
      run_loop_.Run();
  }

 private:
  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override {
    if (active_ != active)
      return;

    observed_ = true;
    widget->RemoveObserver(this);
    if (run_loop_.running())
      run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  bool observed_;
  bool active_;

  DISALLOW_COPY_AND_ASSIGN(WidgetActivationWaiter);
};

namespace libgtk2ui {

// FilePicker opens a GtkFileChooser.
class FilePicker : public ui::SelectFileDialog::Listener {
 public:
  explicit FilePicker(BrowserWindow* window) {
    select_file_dialog_ = ui::SelectFileDialog::Create(
        this, new ChromeSelectFilePolicy(nullptr));

    gfx::NativeWindow parent_window = window->GetNativeWindow();
    ui::SelectFileDialog::FileTypeInfo file_types;
    file_types.allowed_paths = ui::SelectFileDialog::FileTypeInfo::ANY_PATH;
    const base::FilePath file_path;
    select_file_dialog_->SelectFile(ui::SelectFileDialog::SELECT_OPEN_FILE,
                                    base::string16(),
                                    file_path,
                                    &file_types,
                                    0,
                                    base::FilePath::StringType(),
                                    parent_window,
                                    nullptr);
  }

  ~FilePicker() override {
    select_file_dialog_->ListenerDestroyed();
  }

  void Close() {
    SelectFileDialogImplGTK* file_dialog =
        static_cast<SelectFileDialogImplGTK*>(select_file_dialog_.get());


    while (!file_dialog->dialogs_.empty())
      gtk_widget_destroy(*(file_dialog->dialogs_.begin()));
  }

  // SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override {}
 private:
  // Dialog box used for opening and saving files.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(FilePicker);
};

}  // namespace libgtk2ui
