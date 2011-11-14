// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BROWSER_MAIN_LOOP_H_
#define CONTENT_BROWSER_BROWSER_MAIN_LOOP_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

class CommandLine;
class HighResolutionTimerManager;
class MessageLoop;
class SystemMessageWindowWin;

namespace base {
class SystemMonitor;
}

namespace net {
class NetworkChangeNotifier;
}

namespace content {

class BrowserMainParts;
class BrowserThreadImpl;
struct MainFunctionParams;

// Implements the main browser loop stages called from |BrowserMain()|.
// See comments in browser_main_parts.h for additional info.
class BrowserMainLoop {
 public:
  explicit BrowserMainLoop(const content::MainFunctionParams& parameters);
  virtual ~BrowserMainLoop();

  void Init();

  void EarlyInitialization();
  void InitializeToolkit();
  void MainMessageLoopStart();
  void RunMainMessageLoopParts(bool* completed_main_message_loop);
  void MainMessageLoopRun();

  int GetResultCode() const { return result_code_; }

 private:
  void InitializeMainThread();

  // Members initialized on construction ---------------------------------------
  const content::MainFunctionParams& parameters_;
  const CommandLine& parsed_command_line_;
  int result_code_;

  // Vector of BrowserMainParts set by CreateBrowserMainParts ------------------
  // The BrowserParts fucntions for each part are called in the order added.
  // They are released (destroyed) in the reverse order.
  std::vector<BrowserMainParts*> parts_list_;

  // Members initialized in |MainMessageLoopStart()| ---------------------------
  scoped_ptr<MessageLoop> main_message_loop_;
  scoped_ptr<base::SystemMonitor> system_monitor_;
  scoped_ptr<HighResolutionTimerManager> hi_res_timer_manager_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
#if defined(OS_WIN)
  scoped_ptr<SystemMessageWindowWin> system_message_window_;
#endif
  scoped_ptr<BrowserThreadImpl> main_thread_;

  DISALLOW_COPY_AND_ASSIGN(BrowserMainLoop);
};

}  // namespace content

#endif  // CONTENT_BROWSER_BROWSER_MAIN_LOOP_H_
