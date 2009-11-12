// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_FRAME_TEST_CHROME_FRAME_AUTOMATION_MOCK_H_
#define CHROME_FRAME_TEST_CHROME_FRAME_AUTOMATION_MOCK_H_

#include <string>

#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/chrome_frame_plugin.h"
#include "chrome_frame/test/http_server.h"

template <typename T>
class AutomationMockDelegate
    : public CWindowImpl<T>,
      public ChromeFramePlugin<T> {
 public:
  AutomationMockDelegate(MessageLoop* caller_message_loop,
      int launch_timeout, bool perform_version_check,
      const std::wstring& profile_name,
      const std::wstring& extra_chrome_arguments, bool incognito)
      : caller_message_loop_(caller_message_loop), is_connected_(false) {
    test_server_.SetUp();
    automation_client_ = new ChromeFrameAutomationClient;
    automation_client_->Initialize(this, launch_timeout, perform_version_check,
        profile_name, extra_chrome_arguments, incognito);
  }
  ~AutomationMockDelegate() {
    if (automation_client_.get()) {
      automation_client_->Uninitialize();
      automation_client_ = NULL;
    }
    if (IsWindow())
      DestroyWindow();

    test_server_.TearDown();
  }

  // Navigate external tab to the specified url through automation
  bool Navigate(const std::string& url) {
    url_ = GURL(url);
    return automation_client_->InitiateNavigation(url, std::string(), false);
  }

  // Navigate the external to a 'file://' url for unit test files
  bool NavigateRelativeFile(const std::wstring& file) {
    FilePath cf_source_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &cf_source_path);
    std::wstring file_url(L"file://");
    file_url.append(cf_source_path.Append(
        FILE_PATH_LITERAL("chrome_frame")).Append(
            FILE_PATH_LITERAL("test")).Append(
                FILE_PATH_LITERAL("data")).Append(file).value());
    return Navigate(WideToUTF8(file_url));
  }

  bool NavigateRelative(const std::wstring& relative_url) {
    return Navigate(test_server_.Resolve(relative_url.c_str()).spec());
  }

  virtual void OnAutomationServerReady() {
    if (automation_client_.get()) {
      Create(NULL, 0, NULL, WS_OVERLAPPEDWINDOW);
      DCHECK(IsWindow());
      is_connected_ = true;
    } else {
      NOTREACHED();
    }
  }

  virtual void OnAutomationServerLaunchFailed() {
    QuitMessageLoop();
  }

  virtual void OnLoad(int tab_handle, const GURL& url) {
    if (url_ == url) {
      navigation_result_ = true;
    } else {
      QuitMessageLoop();
    }
  }

  virtual void OnLoadFailed(int error_code, const std::string& url) {
    QuitMessageLoop();
  }

  ChromeFrameAutomationClient* automation() {
    return automation_client_.get();
  }
  const GURL& url() const {
    return url_;
  }
  bool is_connected() const {
    return is_connected_;
  }
  bool navigation_result() const {
    return navigation_result_;
  }

  BEGIN_MSG_MAP(AutomationMockDelegate)
  END_MSG_MAP()

 protected:
  void QuitMessageLoop() {
    // Quit on the caller message loop has to be called on the caller
    // thread.
    caller_message_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask);
  }

 private:
  ChromeFrameHTTPServer test_server_;
  MessageLoop* caller_message_loop_;
  GURL url_;
  bool is_connected_;
  bool navigation_result_;
};

class AutomationMockLaunch
    : public AutomationMockDelegate<AutomationMockLaunch> {
 public:
  typedef AutomationMockDelegate<AutomationMockLaunch> Base;
  AutomationMockLaunch(MessageLoop* caller_message_loop,
                       int launch_timeout)
      : Base(caller_message_loop, launch_timeout, true, L"", L"", false) {
  }
  virtual void OnAutomationServerReady() {
    Base::OnAutomationServerReady();
    QuitMessageLoop();
  }
  bool launch_result() const {
    return is_connected();
  }
};

class AutomationMockNavigate
    : public AutomationMockDelegate<AutomationMockNavigate> {
 public:
  typedef AutomationMockDelegate<AutomationMockNavigate> Base;
  AutomationMockNavigate(MessageLoop* caller_message_loop,
                         int launch_timeout)
      : Base(caller_message_loop, launch_timeout, true, L"", L"", false) {
  }
  virtual void OnLoad(int tab_handle, const GURL& url) {
    Base::OnLoad(tab_handle, url);
    QuitMessageLoop();
  }
};

class AutomationMockPostMessage
    : public AutomationMockDelegate<AutomationMockPostMessage> {
 public:
  typedef AutomationMockDelegate<AutomationMockPostMessage> Base;
  AutomationMockPostMessage(MessageLoop* caller_message_loop,
                            int launch_timeout)
      : Base(caller_message_loop, launch_timeout, true, L"", L"", false),
        postmessage_result_(false) {}
  bool postmessage_result() const {
    return postmessage_result_;
  }
  virtual void OnLoad(int tab_handle, const GURL& url) {
    Base::OnLoad(tab_handle, url);
    if (navigation_result()) {
      automation()->ForwardMessageFromExternalHost("Test", "null", "*");
    }
  }
  virtual void OnMessageFromChromeFrame(int tab_handle,
                                        const std::string& message,
                                        const std::string& origin,
                                        const std::string& target) {
    postmessage_result_ = true;
    QuitMessageLoop();
  }
 private:
  bool postmessage_result_;
};

class AutomationMockHostNetworkRequestStart
    : public AutomationMockDelegate<AutomationMockHostNetworkRequestStart> {
 public:
  typedef AutomationMockDelegate<AutomationMockHostNetworkRequestStart> Base;
  AutomationMockHostNetworkRequestStart(MessageLoop* caller_message_loop,
      int launch_timeout)
      : Base(caller_message_loop, launch_timeout, true, L"", L"", false),
        request_start_result_(false) {
    if (automation()) {
      automation()->set_use_chrome_network(false);
    }
  }
  bool request_start_result() const {
    return request_start_result_;
  }
  virtual void OnRequestStart(int tab_handle, int request_id,
                              const IPC::AutomationURLRequest& request) {
    request_start_result_ = true;
    QuitMessageLoop();
  }
  virtual void OnLoad(int tab_handle, const GURL& url) {
    Base::OnLoad(tab_handle, url);
  }
 private:
  bool request_start_result_;
};


#endif  // CHROME_FRAME_TEST_CHROME_FRAME_AUTOMATION_MOCK_H_

