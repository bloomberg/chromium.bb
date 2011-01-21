// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Window message source implementation.
#include "ceee/ie/plugin/bho/window_message_source.h"

#include <algorithm>

#include "base/logging.h"
#include "ceee/common/window_utils.h"
#include "ceee/common/windows_constants.h"
#include "ceee/ie/common/ceee_module_util.h"

WindowMessageSource::MessageSourceMap WindowMessageSource::message_source_map_;
base::Lock WindowMessageSource::lock_;

WindowMessageSource::WindowMessageSource()
    : create_thread_id_(::GetCurrentThreadId()),
      get_message_hook_(NULL),
      call_wnd_proc_ret_hook_(NULL) {
}

WindowMessageSource::~WindowMessageSource() {
  DCHECK(get_message_hook_ == NULL);
  DCHECK(call_wnd_proc_ret_hook_ == NULL);
  DCHECK(GetEntryFromMap(create_thread_id_) == NULL);
}

bool WindowMessageSource::Initialize() {
  if (!AddEntryToMap(create_thread_id_, this))
    return false;

  get_message_hook_ = ::SetWindowsHookEx(WH_GETMESSAGE, GetMessageHookProc,
                                         NULL, create_thread_id_);
  if (get_message_hook_ == NULL) {
    TearDown();
    return false;
  }
  ceee_module_util::RegisterHookForSafetyNet(get_message_hook_);

  call_wnd_proc_ret_hook_ = ::SetWindowsHookEx(WH_CALLWNDPROCRET,
                                               CallWndProcRetHookProc, NULL,
                                               create_thread_id_);
  if (call_wnd_proc_ret_hook_ == NULL) {
    TearDown();
    return false;
  }
  ceee_module_util::RegisterHookForSafetyNet(call_wnd_proc_ret_hook_);
  return true;
}

void WindowMessageSource::TearDown() {
  if (get_message_hook_ != NULL) {
    ceee_module_util::UnregisterHookForSafetyNet(get_message_hook_);
    ::UnhookWindowsHookEx(get_message_hook_);
    get_message_hook_ = NULL;
  }

  if (call_wnd_proc_ret_hook_ != NULL) {
    ceee_module_util::UnregisterHookForSafetyNet(call_wnd_proc_ret_hook_);
    ::UnhookWindowsHookEx(call_wnd_proc_ret_hook_);
    call_wnd_proc_ret_hook_ = NULL;
  }

  RemoveEntryFromMap(create_thread_id_);
}

void WindowMessageSource::RegisterSink(Sink* sink) {
  DCHECK(create_thread_id_ == ::GetCurrentThreadId());

  if (sink == NULL)
    return;

  std::vector<Sink*>::iterator iter = std::find(sinks_.begin(), sinks_.end(),
                                                sink);
  if (iter == sinks_.end())
    sinks_.push_back(sink);
}

void WindowMessageSource::UnregisterSink(Sink* sink) {
  DCHECK(create_thread_id_ == ::GetCurrentThreadId());

  if (sink == NULL)
    return;

  std::vector<Sink*>::iterator iter = std::find(sinks_.begin(), sinks_.end(),
                                                sink);
  if (iter != sinks_.end())
    sinks_.erase(iter);
}

// static
LRESULT CALLBACK WindowMessageSource::GetMessageHookProc(int code,
                                                         WPARAM wparam,
                                                         LPARAM lparam) {
  if (code == HC_ACTION && wparam == PM_REMOVE) {
    MSG* message_info = reinterpret_cast<MSG*>(lparam);
    if (message_info != NULL) {
      if ((message_info->message >= WM_MOUSEFIRST &&
           message_info->message <= WM_MOUSELAST) ||
          (message_info->message >= WM_KEYFIRST &&
           message_info->message <= WM_KEYLAST)) {
        WindowMessageSource* source = GetEntryFromMap(::GetCurrentThreadId());
        if (source != NULL)
          source->OnHandleMessage(message_info);
      }
    }
  }

  return ::CallNextHookEx(NULL, code, wparam, lparam);
}

void WindowMessageSource::OnHandleMessage(const MSG* message_info) {
  DCHECK(create_thread_id_ == ::GetCurrentThreadId());
  DCHECK(message_info != NULL);
  MessageType type = IsWithinTabContentWindow(message_info->hwnd) ?
      TAB_CONTENT_WINDOW : BROWSER_UI_SAME_THREAD;

  for (std::vector<Sink*>::iterator iter = sinks_.begin(); iter != sinks_.end();
       ++iter) {
    (*iter)->OnHandleMessage(type, message_info);
  }
}

// static
LRESULT CALLBACK WindowMessageSource::CallWndProcRetHookProc(int code,
                                                             WPARAM wparam,
                                                             LPARAM lparam) {
  if (code == HC_ACTION) {
    CWPRETSTRUCT* message_info = reinterpret_cast<CWPRETSTRUCT*>(lparam);
    if (message_info != NULL && message_info->message == WM_NCDESTROY) {
      WindowMessageSource* source = GetEntryFromMap(::GetCurrentThreadId());
      if (source != NULL)
        source->OnWindowNcDestroy(message_info->hwnd);
    }
  }

  return ::CallNextHookEx(NULL, code, wparam, lparam);
}

void WindowMessageSource::OnWindowNcDestroy(HWND window) {
  DCHECK(create_thread_id_ == ::GetCurrentThreadId());
  tab_content_window_map_.erase(window);
}

bool WindowMessageSource::IsWithinTabContentWindow(HWND window) {
  DCHECK(create_thread_id_ == ::GetCurrentThreadId());

  if (window == NULL)
    return false;

  // Look up the cache to see whether we have already examined this window
  // handle and got the answer.
  TabContentWindowMap::const_iterator iter =
      tab_content_window_map_.find(window);
  if (iter != tab_content_window_map_.end())
    return iter->second;

  // Examine whether the window or one of its ancestors is the tab content
  // window.
  std::vector<HWND> self_and_ancestors;
  bool is_within_tab_content_window = false;
  do {
    self_and_ancestors.push_back(window);

    if (window_utils::IsWindowClass(window,
                                    windows::kIeTabContentWindowClass)) {
      is_within_tab_content_window = true;
      break;
    }

    window = ::GetAncestor(window, GA_PARENT);
    if (window == NULL || !window_utils::IsWindowThread(window))
      break;

    TabContentWindowMap::const_iterator iter =
        tab_content_window_map_.find(window);
    if (iter != tab_content_window_map_.end()) {
      is_within_tab_content_window = iter->second;
      break;
    }
  } while (true);

  // Add the windows that we have examined into the cache.
  for (std::vector<HWND>::const_iterator iter = self_and_ancestors.begin();
       iter != self_and_ancestors.end(); ++iter) {
    tab_content_window_map_.insert(
        std::make_pair<HWND, bool>(*iter, is_within_tab_content_window));
  }
  return is_within_tab_content_window;
}

// static
bool WindowMessageSource::AddEntryToMap(DWORD thread_id,
                                        WindowMessageSource* source) {
  DCHECK(source != NULL);

  base::AutoLock auto_lock(lock_);
  MessageSourceMap::const_iterator iter = message_source_map_.find(thread_id);
  if (iter != message_source_map_.end())
    return false;

  message_source_map_.insert(
      std::make_pair<DWORD, WindowMessageSource*>(thread_id, source));
  return true;
}

// static
WindowMessageSource* WindowMessageSource::GetEntryFromMap(DWORD thread_id) {
  base::AutoLock auto_lock(lock_);
  MessageSourceMap::const_iterator iter = message_source_map_.find(thread_id);
  return iter == message_source_map_.end() ? NULL : iter->second;
}

// static
void WindowMessageSource::RemoveEntryFromMap(DWORD thread_id) {
  base::AutoLock auto_lock(lock_);
  message_source_map_.erase(thread_id);
}
