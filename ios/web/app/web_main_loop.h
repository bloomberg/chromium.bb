// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_APP_WEB_MAIN_LOOP_H_
#define IOS_WEB_APP_WEB_MAIN_LOOP_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace base {
class MessageLoop;
class PowerMonitor;
class SystemMonitor;
}  // namespace base

namespace net {
class NetworkChangeNotifier;
}  // namespace net

namespace web {
class CookieNotificationBridge;
class WebMainParts;
class WebThreadImpl;

// Implements the main web loop stages called from WebMainRunner.
// See comments in web_main_parts.h for additional info.
class WebMainLoop {
 public:
  explicit WebMainLoop();
  virtual ~WebMainLoop();

  void Init();

  void EarlyInitialization();
  void MainMessageLoopStart();

  // Creates and starts running the tasks needed to complete startup.
  void CreateStartupTasks();

  // Performs the shutdown sequence, starting with PostMainMessageLoopRun
  // through stopping threads to PostDestroyThreads.
  void ShutdownThreadsAndCleanUp();

  int GetResultCode() const { return result_code_; }

 private:
  void InitializeMainThread();

  // Called just before creating the threads
  int PreCreateThreads();

  // Creates all secondary threads.
  int CreateThreads();

  // Called right after the web threads have been started.
  int WebThreadsStarted();

  // Called just before attaching to the main message loop.
  int PreMainMessageLoopRun();

  // Members initialized on construction ---------------------------------------
  int result_code_;
  // True if the non-UI threads were created.
  bool created_threads_;

  // Members initialized in |MainMessageLoopStart()| ---------------------------
  std::unique_ptr<base::MessageLoop> main_message_loop_;
  std::unique_ptr<base::SystemMonitor> system_monitor_;
  std::unique_ptr<base::PowerMonitor> power_monitor_;
  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;

  // Destroy parts_ before main_message_loop_ (required) and before other
  // classes constructed in web (but after main_thread_).
  std::unique_ptr<WebMainParts> parts_;

  // Members initialized in |InitializeMainThread()| ---------------------------
  // This must get destroyed before other threads that are created in parts_.
  std::unique_ptr<WebThreadImpl> main_thread_;

  // Members initialized in |RunMainMessageLoopParts()| ------------------------
  std::unique_ptr<WebThreadImpl> db_thread_;
  std::unique_ptr<WebThreadImpl> file_user_blocking_thread_;
  std::unique_ptr<WebThreadImpl> file_thread_;
  std::unique_ptr<WebThreadImpl> cache_thread_;
  std::unique_ptr<WebThreadImpl> io_thread_;

  // Members initialized in |WebThreadsStarted()| --------------------------
  std::unique_ptr<CookieNotificationBridge> cookie_notification_bridge_;

  DISALLOW_COPY_AND_ASSIGN(WebMainLoop);
};

}  // namespace web

#endif  // IOS_WEB_APP_WEB_MAIN_LOOP_H_
