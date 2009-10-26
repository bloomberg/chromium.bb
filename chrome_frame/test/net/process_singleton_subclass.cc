// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/net/process_singleton_subclass.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome/browser/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_frame/test/net/test_automation_provider.h"
#include "chrome_frame/function_stub.h"
#include "testing/gtest/include/gtest/gtest.h"

ProcessSingletonSubclass::ProcessSingletonSubclass(
    ProcessSingletonSubclassDelegate* delegate)
    : stub_(NULL), delegate_(delegate), original_wndproc_(NULL) {
}

ProcessSingletonSubclass::~ProcessSingletonSubclass() {
  if (stub_) {
    stub_->BypassStub(reinterpret_cast<void*>(original_wndproc_));
  }
}

bool ProcessSingletonSubclass::Subclass(const FilePath& user_data_dir) {
  DCHECK(stub_ == NULL);
  DCHECK(original_wndproc_ == NULL);
  HWND hwnd = FindWindowEx(HWND_MESSAGE, NULL, chrome::kMessageWindowClass,
                           user_data_dir.ToWStringHack().c_str());
  if (!::IsWindow(hwnd))
    return false;

  // The window must be in this process for us to be able to subclass it.
  DWORD pid = 0;
  ::GetWindowThreadProcessId(hwnd, &pid);
  EXPECT_EQ(pid, ::GetCurrentProcessId());

  original_wndproc_ = reinterpret_cast<WNDPROC>(::GetWindowLongPtr(hwnd,
      GWLP_WNDPROC));
  stub_ = FunctionStub::Create(reinterpret_cast<uintptr_t>(this),
                               &SubclassWndProc);
  DCHECK(stub_);
  ::SetWindowLongPtr(hwnd, GWLP_WNDPROC,
      reinterpret_cast<LONG_PTR>(stub_->code()));
  return true;
}

// static
LRESULT ProcessSingletonSubclass::SubclassWndProc(ProcessSingletonSubclass* me,
                                                  HWND hwnd, UINT msg,
                                                  WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_COPYDATA:
      return me->OnCopyData(hwnd, reinterpret_cast<HWND>(wp),
                            reinterpret_cast<COPYDATASTRUCT*>(lp));
    default:
      break;
  }

  return me->original_wndproc_(hwnd, msg, wp, lp);
}

// static
LRESULT ProcessSingletonSubclass::OnCopyData(HWND hwnd, HWND from_hwnd,
                                             const COPYDATASTRUCT* cds) {
  // We should have enough room for the shortest command (min_message_size)
  // and also be a multiple of wchar_t bytes. The shortest command
  // possible is L"START\0\0" (empty current directory and command line).
  static const int kMinMessageSize = sizeof(L"START\0");
  EXPECT_TRUE(kMinMessageSize <= cds->cbData);

  if (kMinMessageSize > cds->cbData)
    return TRUE;

  // We split the string into 4 parts on NULLs.
  const wchar_t* begin = reinterpret_cast<const wchar_t*>(cds->lpData);
  const wchar_t* end = begin + (cds->cbData / sizeof(wchar_t));
  const wchar_t kNull = L'\0';
  const wchar_t* eos = wmemchr(begin, kNull, end - begin);
  EXPECT_NE(eos, end);
  if (lstrcmpW(begin, L"START") == 0) {
    begin = eos + 1;
    EXPECT_TRUE(begin <= end);
    eos = wmemchr(begin, kNull, end - begin);
    EXPECT_NE(eos, end);

    // Get current directory.
    const wchar_t* cur_dir = begin;
    begin = eos + 1;
    EXPECT_TRUE(begin <= end);
    eos = wmemchr(begin, kNull, end - begin);
    // eos might be equal to end at this point.

    // Get command line.
    std::wstring cmd_line(begin, static_cast<size_t>(end - begin));

    CommandLine parsed_command_line = CommandLine::FromString(cmd_line);
    std::string channel_id(WideToASCII(parsed_command_line.GetSwitchValue(
        switches::kAutomationClientChannelID)));
    EXPECT_FALSE(channel_id.empty());

    delegate_->OnConnectAutomationProviderToChannel(channel_id);
  }
  return TRUE;
}
