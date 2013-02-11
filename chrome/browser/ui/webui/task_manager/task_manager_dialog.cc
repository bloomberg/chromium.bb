// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/task_manager/task_manager_dialog.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "grit/google_chrome_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/size.h"
#include "ui/web_dialogs/web_dialog_delegate.h"

#if defined(OS_CHROMEOS)
#include "ui/views/widget/widget.h"
#endif

namespace {

// The minimum size of task manager window in px.
const int kMinimumTaskManagerWidth = 640;
const int kMinimumTaskManagerHeight = 480;

}  // namespace

using content::BrowserThread;
using content::WebContents;
using content::WebUIMessageHandler;
using ui::WebDialogDelegate;

class TaskManagerDialogImpl : public WebDialogDelegate {
 public:
  TaskManagerDialogImpl();

  static void Show(bool is_background_page_mode);
  static TaskManagerDialogImpl* GetInstance();

 protected:
  friend struct DefaultSingletonTraits<TaskManagerDialogImpl>;
  virtual ~TaskManagerDialogImpl();

  void OnCloseDialog();

  // Overridden from WebDialogDelegate:
  virtual ui::ModalType GetDialogModalType() const OVERRIDE {
    return ui::MODAL_TYPE_NONE;
  }
  virtual string16 GetDialogTitle() const OVERRIDE {
    return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_TITLE);
  }
  virtual std::string GetDialogName() const OVERRIDE {
    return prefs::kTaskManagerWindowPlacement;
  }
  virtual GURL GetDialogContentURL() const OVERRIDE {
    std::string url_string(chrome::kChromeUITaskManagerURL);
    url_string += "?";
    if (browser_defaults::kShowCancelButtonInTaskManager)
      url_string += "showclose=1&";
    if (is_background_page_mode_)
      url_string += "background=1";
    return GURL(url_string);
  }
  virtual void GetWebUIMessageHandlers(
      std::vector<WebUIMessageHandler*>* handlers) const OVERRIDE {
  }
  virtual void GetDialogSize(gfx::Size* size) const OVERRIDE {
#if !defined(TOOLKIT_VIEWS)
    // If dialog's bounds are previously saved, use them.
    if (g_browser_process->local_state()) {
      const DictionaryValue* placement_pref =
          g_browser_process->local_state()->GetDictionary(
          prefs::kTaskManagerWindowPlacement);
      int width, height;
      if (placement_pref &&
          placement_pref->GetInteger("width", &width) &&
          placement_pref->GetInteger("height", &height)) {
        size->SetSize(std::max(1, width), std::max(1, height));
        return;
      }
    }

    // Otherwise set default size.
    size->SetSize(kMinimumTaskManagerWidth, kMinimumTaskManagerHeight);
#endif
  }
  virtual void GetMinimumDialogSize(gfx::Size* size) const OVERRIDE {
    size->SetSize(kMinimumTaskManagerWidth, kMinimumTaskManagerHeight);
  }
  virtual std::string GetDialogArgs() const OVERRIDE {
    return std::string();
  }
  virtual void OnDialogClosed(const std::string& json_retval) OVERRIDE {
    OnCloseDialog();
  }
  virtual void OnCloseContents(WebContents* source, bool* out_close_dialog)
      OVERRIDE {
    *out_close_dialog = true;
  }
  virtual bool ShouldShowDialogTitle() const OVERRIDE {
    return true;
  }
  virtual bool HandleContextMenu(
      const content::ContextMenuParams& params) OVERRIDE {
    return true;
  }
#if !defined(TOOLKIT_VIEWS)
  virtual void StoreDialogSize(const gfx::Size& dialog_size) OVERRIDE {
    // Store the dialog's bounds so that it can be restored with the same bounds
    // the next time it's opened.
    if (g_browser_process->local_state()) {
      DictionaryPrefUpdate update(g_browser_process->local_state(),
                                  prefs::kTaskManagerWindowPlacement);
      DictionaryValue* placement_pref = update.Get();
      placement_pref->SetInteger("width", dialog_size.width());
      placement_pref->SetInteger("height", dialog_size.height());
    }
  }
#endif

 private:
  void ShowDialog(bool is_background_page_mode);
  void OpenWebDialog();

  int show_count_;

  gfx::NativeWindow window_;
  bool is_background_page_mode_;

  DISALLOW_COPY_AND_ASSIGN(TaskManagerDialogImpl);
};

// ****************************************************

// static
TaskManagerDialogImpl* TaskManagerDialogImpl::GetInstance() {
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::FILE));
  return Singleton<TaskManagerDialogImpl>::get();
}

TaskManagerDialogImpl::TaskManagerDialogImpl()
    : show_count_(0),
      window_(NULL),
      is_background_page_mode_(false) {
}

TaskManagerDialogImpl::~TaskManagerDialogImpl() {
}

// static
void TaskManagerDialogImpl::Show(bool is_background_page_mode) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TaskManagerDialogImpl* dialog = TaskManagerDialogImpl::GetInstance();
  dialog->ShowDialog(is_background_page_mode);
}

void TaskManagerDialogImpl::ShowDialog(bool is_background_page_mode) {
  if (show_count_ > 0) {
#if defined(OS_CHROMEOS)
    // Closes current TaskManager and Opens new one.
    views::Widget::GetWidgetForNativeWindow(window_)->Close();
#else
    // TODO(yoshiki): Supports the other platforms.
    platform_util::ActivateWindow(window_);
    return;
#endif
  }
  is_background_page_mode_ = is_background_page_mode;
  OpenWebDialog();
  ++show_count_;
}

void TaskManagerDialogImpl::OnCloseDialog() {
  if (show_count_ > 0)
    --show_count_;
}

void TaskManagerDialogImpl::OpenWebDialog() {
  window_ = chrome::ShowWebDialog(NULL,
                                  ProfileManager::GetLastUsedProfile(),
                                  this);
}

// static
void TaskManagerDialog::Show() {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&TaskManagerDialogImpl::Show, false));
}

// static
void TaskManagerDialog::ShowBackgroundPages() {
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&TaskManagerDialogImpl::Show, true));
}
