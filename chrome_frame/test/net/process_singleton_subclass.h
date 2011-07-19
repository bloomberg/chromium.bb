// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_TEST_NET_PROCESS_SINGLETON_SUBCLASS_H_
#define CHROME_FRAME_TEST_NET_PROCESS_SINGLETON_SUBCLASS_H_

#include <windows.h>

#include <string>

class FilePath;
struct FunctionStub;

class ProcessSingletonSubclassDelegate {
 public:
  virtual void OnConnectAutomationProviderToChannel(
      const std::string& channel_id) = 0;
};

class ProcessSingletonSubclass {
 public:
  explicit ProcessSingletonSubclass(ProcessSingletonSubclassDelegate* delegate);
  ~ProcessSingletonSubclass();

  bool Subclass(const FilePath& user_data_dir);

 protected:
  static LRESULT CALLBACK SubclassWndProc(ProcessSingletonSubclass* me,
      HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  LRESULT OnCopyData(HWND hwnd, HWND from_hwnd, const COPYDATASTRUCT* cds);
 protected:
  FunctionStub* stub_;
  ProcessSingletonSubclassDelegate* delegate_;
  WNDPROC original_wndproc_;
};

#endif  // CHROME_FRAME_TEST_NET_PROCESS_SINGLETON_SUBCLASS_H_
