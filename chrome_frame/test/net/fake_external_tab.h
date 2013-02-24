// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_NET_FAKE_EXTERNAL_TAB_H_
#define CHROME_FRAME_TEST_NET_FAKE_EXTERNAL_TAB_H_

#include <string>

#include "base/cancelable_callback.h"
#include "base/files/file_path.h"
#include "base/message_loop.h"
#include "base/process.h"
#include "base/win/scoped_handle.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome_frame/test/net/test_automation_provider.h"
#include "chrome_frame/test/test_server.h"
#include "chrome_frame/test_utils.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_test_suite.h"

class CommandLine;
class FakeBrowserProcessImpl;
class ProcessSingleton;

namespace net {
class ScopedCustomUrlRequestTestHttpHost;
}  // namespace net

namespace content {
class NotificationService;
}  // namespace content

namespace logging_win {
class FileLogger;
}  // namespace logging_win

namespace chrome_frame_test {
class IEConfigurator;
}  // namespace chrome_frame_test

class FakeExternalTab {
 public:
  FakeExternalTab();
  virtual ~FakeExternalTab();

  virtual void Initialize();
  virtual void InitializePostThreadsCreated();
  virtual void Shutdown();

  const base::FilePath& user_data() const {
    return user_data_dir_;
  }

  FakeBrowserProcessImpl* browser_process() const;

 private:
  scoped_ptr<FakeBrowserProcessImpl> browser_process_;
  base::FilePath overridden_user_dir_;
  base::FilePath user_data_dir_;
  scoped_ptr<content::NotificationService> notificaton_service_;

  DISALLOW_COPY_AND_ASSIGN(FakeExternalTab);
};

// The "master class" that spins the UI and test threads.
//
// In this weird test executable that pretends to almost be Chrome, it
// plays a similar role to ChromeBrowserMainParts, and must fulfill
// the existing contract between ChromeBrowserMainParts and
// BrowserProcessImpl, i.e. poking BrowserProcessImpl at certain
// lifetime events.
class CFUrlRequestUnittestRunner
    : public NetTestSuite,
      public TestAutomationProviderDelegate,
      public content::BrowserMainParts {
 public:
  CFUrlRequestUnittestRunner(int argc, char** argv);
  virtual ~CFUrlRequestUnittestRunner();

  virtual void StartChromeFrameInHostBrowser();

  virtual void ShutDownHostBrowser();

  // Overrides to not call icu initialize
  virtual void Initialize();
  virtual void Shutdown();

  // TestAutomationProviderDelegate.
  virtual void OnInitialTabLoaded();
  virtual void OnProviderDestroyed();

  void StartTests();

  // Borrowed from TestSuite::Initialize().
  void InitializeLogging();

  int test_result() const {
    return test_result_;
  }

  void set_crash_service(base::ProcessHandle handle) {
    crash_service_ = handle;
  }

  // content::BrowserMainParts implementation.
  virtual void PreEarlyInitialization() OVERRIDE;
  virtual int PreCreateThreads() OVERRIDE;
  virtual void PreMainMessageLoopRun() OVERRIDE;
  virtual bool MainMessageLoopRun(int* result_code) OVERRIDE;
  virtual void PostMainMessageLoopRun() OVERRIDE;
  virtual void PostDestroyThreads() OVERRIDE;

 protected:
  // This is the thread that runs all the UrlRequest tests.
  // Within its context, the Initialize() and Shutdown() routines above
  // will be called.
  static DWORD WINAPI RunAllUnittests(void* param);

  void TakeDownBrowser();

 protected:
  base::win::ScopedHandle test_thread_;
  base::ProcessHandle crash_service_;
  DWORD test_thread_id_;

  scoped_ptr<net::ScopedCustomUrlRequestTestHttpHost> override_http_host_;

  scoped_ptr<test_server::SimpleWebServer> test_http_server_;
  test_server::SimpleResponse chrome_frame_html_;

  // The fake chrome instance.
  scoped_ptr<FakeExternalTab> fake_chrome_;
  ScopedChromeFrameRegistrar registrar_;
  int test_result_;

 private:
  // Causes HTTP tests to run over an external address rather than 127.0.0.1.
  // See http://crbug.com/114369 .
  void OverrideHttpHost();
  void StartFileLogger();
  void StopFileLogger(bool print);
  void OnIEShutdownFailure();

  void CancelInitializationTimeout();
  void StartInitializationTimeout();
  void OnInitializationTimeout();

  bool ProcessSingletonNotificationCallback(
      const CommandLine& command_line,
      const base::FilePath& current_directory);

  bool launch_browser_;
  bool prompt_after_setup_;
  bool tests_ran_;
  scoped_ptr<ProcessSingleton> process_singleton_;
  base::CancelableClosure timeout_closure_;
  scoped_ptr<logging_win::FileLogger> file_logger_;
  base::FilePath log_file_;
  scoped_ptr<chrome_frame_test::IEConfigurator> ie_configurator_;

  DISALLOW_COPY_AND_ASSIGN(CFUrlRequestUnittestRunner);
};

#endif  // CHROME_FRAME_TEST_NET_FAKE_EXTERNAL_TAB_H_
