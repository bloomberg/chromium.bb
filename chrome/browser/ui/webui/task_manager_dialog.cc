// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/task_manager_dialog.h"

#include "base/memory/singleton.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "chrome/common/url_constants.h"
#include "grit/google_chrome_strings.h"
#include "ui/base/l10n/l10n_util.h"

class TaskManagerDialogImpl : public HtmlDialogUIDelegate {
 public:
  TaskManagerDialogImpl();

  static void Show();
  static TaskManagerDialogImpl* GetInstance();

 protected:
  friend struct DefaultSingletonTraits<TaskManagerDialogImpl>;
  virtual ~TaskManagerDialogImpl();

  void OnCloseDialog();

  // Overridden from HtmlDialogUIDelegate:
  virtual bool IsDialogModal() const {
    return false;
  }
  virtual std::wstring GetDialogTitle() const {
    return UTF16ToWide(l10n_util::GetStringUTF16(IDS_TASK_MANAGER_TITLE));
  }
  virtual GURL GetDialogContentURL() const {
    std::string url_string(chrome::kChromeUITaskManagerURL);
    return GURL(url_string);
  }
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const {
  }
  virtual void GetDialogSize(gfx::Size* size) const {
    size->SetSize(640, 480);
  }
  virtual std::string GetDialogArgs() const {
    return std::string();
  }
  virtual void OnDialogClosed(const std::string& json_retval) {
    OnCloseDialog();
  }
  virtual void OnCloseContents(TabContents* source, bool* out_close_dialog) {
    *out_close_dialog = true;
    OnCloseDialog();
  }
  virtual bool ShouldShowDialogTitle() const {
    return false;
  }
  virtual bool HandleContextMenu(const ContextMenuParams& params) {
    return true;
  }

 private:
  void ShowDialog();
  void OpenHtmlDialog();

  bool is_shown_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerDialogImpl);
};

// ****************************************************

// static
TaskManagerDialogImpl* TaskManagerDialogImpl::GetInstance() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return Singleton<TaskManagerDialogImpl>::get();
}

TaskManagerDialogImpl::TaskManagerDialogImpl() : is_shown_(false) {
}

TaskManagerDialogImpl::~TaskManagerDialogImpl() {
}

void TaskManagerDialogImpl::Show() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TaskManagerDialogImpl* dialog = TaskManagerDialogImpl::GetInstance();
  dialog->ShowDialog();
}

void TaskManagerDialogImpl::ShowDialog() {
  // TODO(yoshiki): Brings up existing UI when called with is_shown_ == TRUE
  if (!is_shown_) {
    is_shown_ = true;
    OpenHtmlDialog();
  }
}

void TaskManagerDialogImpl::OnCloseDialog() {
  if (is_shown_)
    is_shown_ = false;
}

void TaskManagerDialogImpl::OpenHtmlDialog() {
  Browser* browser = BrowserList::GetLastActive();
  browser->BrowserShowHtmlDialog(this, NULL);
}

// ****************************************************
//
// static
void TaskManagerDialog::Show() {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableFunction(&TaskManagerDialogImpl::Show));
}

