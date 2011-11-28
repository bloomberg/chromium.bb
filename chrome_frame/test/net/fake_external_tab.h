// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_NET_FAKE_EXTERNAL_TAB_H_
#define CHROME_FRAME_TEST_NET_FAKE_EXTERNAL_TAB_H_
#pragma once

#include <string>

#include "base/file_path.h"
#include "base/message_loop.h"
#include "base/win/scoped_handle.h"
#include "chrome/app/scoped_ole_initializer.h"
#include "chrome/browser/browser_process_impl.h"
#include "chrome_frame/test/net/process_singleton_subclass.h"
#include "chrome_frame/test/net/test_automation_provider.h"
#include "chrome_frame/test/test_server.h"
#include "chrome_frame/test_utils.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_test_suite.h"

class ProcessSingleton;

namespace content {
class NotificationService;
}

class FakeExternalTab {
 public:
  FakeExternalTab();
  virtual ~FakeExternalTab();

  virtual void Initialize();
  virtual void Shutdown();

  const FilePath& user_data() const {
    return user_data_dir_;
  }

  MessageLoopForUI* ui_loop() {
    return &loop_;
  }

 protected:
  MessageLoopForUI loop_;
  scoped_ptr<BrowserProcess> browser_process_;
  FilePath overridden_user_dir_;
  FilePath user_data_dir_;
  scoped_ptr<ProcessSingleton> process_singleton_;
  scoped_ptr<content::NotificationService> notificaton_service_;
};

// The "master class" that spins the UI and test threads.
class CFUrlRequestUnittestRunner
    : public NetTestSuite,
      public ProcessSingletonSubclassDelegate,
      public TestAutomationProviderDelegate {
 public:
  CFUrlRequestUnittestRunner(int argc, char** argv);
  ~CFUrlRequestUnittestRunner();

  virtual void StartChromeFrameInHostBrowser();

  virtual void ShutDownHostBrowser();

  // Overrides to not call icu initialize
  virtual void Initialize();
  virtual void Shutdown();

  // ProcessSingletonSubclassDelegate.
  virtual void OnConnectAutomationProviderToChannel(
      const std::string& channel_id);

  // TestAutomationProviderDelegate.
  virtual void OnInitialTabLoaded();

  void RunMainUIThread();

  void StartTests();

  // Borrowed from TestSuite::Initialize().
  static void InitializeLogging();

  int test_result() const {
    return test_result_;
  }

 protected:
  // This is the thread that runs all the UrlRequest tests.
  // Within its context, the Initialize() and Shutdown() routines above
  // will be called.
  static DWORD WINAPI RunAllUnittests(void* param);

  static void TakeDownBrowser(CFUrlRequestUnittestRunner* me);

 protected:
  base::win::ScopedHandle test_thread_;
  DWORD test_thread_id_;

  scoped_ptr<test_server::SimpleWebServer> test_http_server_;
  test_server::SimpleResponse chrome_frame_html_;

  // The fake chrome instance.  This instance owns the UI message loop
  // on the main thread.
  FakeExternalTab fake_chrome_;
  scoped_ptr<ProcessSingletonSubclass> pss_subclass_;
  scoped_ptr<content::DeprecatedBrowserThread> main_thread_;
  ScopedChromeFrameRegistrar registrar_;
  int test_result_;
};

#endif  // CHROME_FRAME_TEST_NET_FAKE_EXTERNAL_TAB_H_
