// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_CHROME_FRAME_DELEGATE_H_
#define CHROME_FRAME_CHROME_FRAME_DELEGATE_H_
#pragma once

#include <atlbase.h>
#include <atlwin.h>
#include <queue>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/synchronization/lock.h"
#include "base/task.h"
#include "chrome/common/automation_constants.h"
#include "ipc/ipc_message.h"

class GURL;
struct AttachExternalTabParams;
struct AutomationURLRequest;
struct MiniContextMenuParams;
struct NavigationInfo;

namespace net {
class URLRequestStatus;
}

// A common interface supported by all the browser specific ChromeFrame
// implementations.
class ChromeFrameDelegate {
 public:
  typedef HWND WindowType;

  virtual WindowType GetWindow() const = 0;
  virtual void GetBounds(RECT* bounds) = 0;
  virtual std::string GetDocumentUrl() = 0;
  virtual void OnAutomationServerReady() = 0;
  virtual void OnAutomationServerLaunchFailed(
      AutomationLaunchResult reason, const std::string& server_version) = 0;
  virtual void OnExtensionInstalled(
      const FilePath& path,
      void* user_data,
      AutomationMsg_ExtensionResponseValues response) = 0;
  virtual void OnGetEnabledExtensionsComplete(
      void* user_data,
      const std::vector<FilePath>& extension_directories) = 0;
  virtual bool OnMessageReceived(const IPC::Message& msg) = 0;
  virtual void OnChannelError() = 0;

  // This remains in interface since we call it if Navigate()
  // returns immediate error.
  virtual void OnLoadFailed(int error_code, const std::string& url) = 0;

  // Returns true if this instance is alive and well for processing automation
  // messages.
  virtual bool IsValid() const = 0;

  // To be called when the top-most window of an application hosting
  // ChromeFrame is moved.
  virtual void OnHostMoved() = 0;

 protected:
  virtual ~ChromeFrameDelegate() {}
};

// Disable refcounting of ChromeFrameDelegate.
DISABLE_RUNNABLE_METHOD_REFCOUNT(ChromeFrameDelegate);

extern UINT kAutomationServerReady;
extern UINT kMessageFromChromeFrame;

class ChromeFrameDelegateImpl : public ChromeFrameDelegate {
 public:
  virtual WindowType GetWindow() { return NULL; }
  virtual void GetBounds(RECT* bounds) {}
  virtual std::string GetDocumentUrl() { return std::string(); }
  virtual void OnAutomationServerReady() {}
  virtual void OnAutomationServerLaunchFailed(
      AutomationLaunchResult reason, const std::string& server_version) {}
  virtual void OnExtensionInstalled(
      const FilePath& path,
      void* user_data,
      AutomationMsg_ExtensionResponseValues response) {}
  virtual void OnGetEnabledExtensionsComplete(
      void* user_data,
      const std::vector<FilePath>& extension_directories) {}
  virtual void OnLoadFailed(int error_code, const std::string& url) {}
  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError() {}

  static bool IsTabMessage(const IPC::Message& message);

  virtual bool IsValid() const {
    return true;
  }

  virtual void OnHostMoved() {}

 protected:
  // Protected methods to be overriden.
  virtual void OnNavigationStateChanged(
      int flags, const NavigationInfo& nav_info) {}
  virtual void OnUpdateTargetUrl(const std::wstring& new_target_url) {}
  virtual void OnAcceleratorPressed(const MSG& accel_message) {}
  virtual void OnTabbedOut(bool reverse) {}
  virtual void OnOpenURL(
      const GURL& url, const GURL& referrer, int open_disposition) {}
  virtual void OnDidNavigate(const NavigationInfo& navigation_info) {}
  virtual void OnNavigationFailed(int error_code, const GURL& gurl) {}
  virtual void OnLoad(const GURL& url) {}
  virtual void OnMessageFromChromeFrame(const std::string& message,
                                        const std::string& origin,
                                        const std::string& target) {}
  virtual void OnHandleContextMenu(HANDLE menu_handle, int align_flags,
                                   const MiniContextMenuParams& params) {}
  virtual void OnRequestStart(
      int request_id, const AutomationURLRequest& request) {}
  virtual void OnRequestRead(int request_id, int bytes_to_read) {}
  virtual void OnRequestEnd(int request_id,
                            const net::URLRequestStatus& status) {}
  virtual void OnDownloadRequestInHost(int request_id) {}
  virtual void OnSetCookieAsync(const GURL& url, const std::string& cookie) {}
  virtual void OnAttachExternalTab(
      const AttachExternalTabParams& attach_params) {}
  virtual void OnGoToHistoryEntryOffset(int offset) {}

  virtual void OnGetCookiesFromHost(const GURL& url, int cookie_id) {}
  virtual void OnCloseTab() {}
};

// This interface enables tasks to be marshaled to desired threads.
class TaskMarshaller {  // NOLINT
 public:
  virtual void PostTask(const tracked_objects::Location& from_here,
                        Task* task) = 0;
};

// T is expected to be something CWindowImpl derived, or at least to have
// PostMessage(UINT, WPARAM) method. Do not forget to CHAIN_MSG_MAP
template <class T> class TaskMarshallerThroughWindowsMessages
    : public TaskMarshaller {
 public:
  TaskMarshallerThroughWindowsMessages() {}
  virtual void PostTask(const tracked_objects::Location& from_here,
                        Task* task) {
    task->SetBirthPlace(from_here);
    T* this_ptr = static_cast<T*>(this);
    if (this_ptr->IsWindow()) {
      this_ptr->AddRef();
      PushTask(task);
      this_ptr->PostMessage(MSG_EXECUTE_TASK, reinterpret_cast<WPARAM>(task));
    } else {
      DVLOG(1) << "Dropping MSG_EXECUTE_TASK message for destroyed window.";
      delete task;
    }
  }

 protected:
  ~TaskMarshallerThroughWindowsMessages() {
    DeleteAllPendingTasks();
  }

  void DeleteAllPendingTasks() {
    base::AutoLock lock(lock_);
    DVLOG_IF(1, !pending_tasks_.empty()) << "Destroying "
                                         << pending_tasks_.size()
                                         << " pending tasks";
    while (!pending_tasks_.empty()) {
      Task* task = pending_tasks_.front();
      pending_tasks_.pop();
      delete task;
    }
  }

  BEGIN_MSG_MAP(PostMessageMarshaller)
    MESSAGE_HANDLER(MSG_EXECUTE_TASK, ExecuteTask)
  END_MSG_MAP()

 private:
  enum { MSG_EXECUTE_TASK = WM_APP + 6 };
  inline LRESULT ExecuteTask(UINT, WPARAM wparam, LPARAM,
                             BOOL& handled) {  // NOLINT
    Task* task = reinterpret_cast<Task*>(wparam);
    if (task && PopTask(task)) {
      task->Run();
      delete task;
    }

    T* this_ptr = static_cast<T*>(this);
    this_ptr->Release();
    return 0;
  }

  inline void PushTask(Task* task) {
    base::AutoLock lock(lock_);
    pending_tasks_.push(task);
  }

  // If the given task is front of the queue, removes the task and returns true,
  // otherwise we assume this is an already destroyed task (but Window message
  // had remained in the thread queue).
  inline bool PopTask(Task* task) {
    base::AutoLock lock(lock_);
    if (!pending_tasks_.empty() && task == pending_tasks_.front()) {
      pending_tasks_.pop();
      return true;
    }

    return false;
  }

  base::Lock lock_;
  std::queue<Task*> pending_tasks_;
};

#endif  // CHROME_FRAME_CHROME_FRAME_DELEGATE_H_
