// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_FRAME_TEST_CHROME_FRAME_AUTOMATION_MOCK_H_
#define CHROME_FRAME_TEST_CHROME_FRAME_AUTOMATION_MOCK_H_

#include <string>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "chrome_frame/chrome_frame_automation.h"
#include "chrome_frame/chrome_frame_plugin.h"
#include "chrome_frame/navigation_constraints.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/test_scrubber.h"
#include "chrome_frame/test/test_with_web_server.h"
#include "chrome_frame/utils.h"

template <typename T>
class AutomationMockDelegate
    : public CWindowImpl<T>,
      public ChromeFramePlugin<T> {
 public:
  AutomationMockDelegate(MessageLoop* caller_message_loop,
      int launch_timeout, bool perform_version_check,
      const std::wstring& profile_name,
      const std::wstring& language,
      bool incognito, bool is_widget_mode)
      : caller_message_loop_(caller_message_loop), is_connected_(false),
        navigation_result_(false),
        mock_server_(1337, L"127.0.0.1",
            chrome_frame_test::GetTestDataFolder()) {

    // Endeavour to only kill off Chrome Frame derived Chrome processes.
    KillAllNamedProcessesWithArgument(
        UTF8ToWide(chrome_frame_test::kChromeImageName),
        UTF8ToWide(switches::kChromeFrame));

    mock_server_.ExpectAndServeAnyRequests(CFInvocation(CFInvocation::NONE));

    base::FilePath profile_path;
    GetChromeFrameProfilePath(profile_name, &profile_path);
    chrome_frame_test::OverrideDataDirectoryForThisTest(profile_path.value());

    automation_client_ = new ChromeFrameAutomationClient;
    GURL empty;
    scoped_refptr<ChromeFrameLaunchParams> clp(
        new ChromeFrameLaunchParams(empty, empty, profile_path, profile_name,
            language, incognito, is_widget_mode, false, false));
    clp->set_launch_timeout(launch_timeout);
    clp->set_version_check(perform_version_check);
    automation_client_->Initialize(this, clp);
  }
  ~AutomationMockDelegate() {
    if (automation_client_.get()) {
      automation_client_->Uninitialize();
      automation_client_ = NULL;
    }
    if (IsWindow())
      DestroyWindow();
  }

  // Navigate external tab to the specified url through automation
  bool Navigate(const std::string& url) {
    NavigationConstraintsImpl navigation_constraints;
    url_ = GURL(url);
    bool result = automation_client_->InitiateNavigation(
        url, std::string(), &navigation_constraints);
    if (!result)
      OnLoadFailed(0, url);
    return result;
  }

  // Navigate the external to a 'file://' url for unit test files
  bool NavigateRelativeFile(const std::wstring& file) {
    base::FilePath cf_source_path;
    PathService::Get(base::DIR_SOURCE_ROOT, &cf_source_path);
    std::wstring file_url(L"file://");
    file_url.append(cf_source_path.Append(
        FILE_PATH_LITERAL("chrome_frame")).Append(
            FILE_PATH_LITERAL("test")).Append(
                FILE_PATH_LITERAL("data")).Append(file).value());
    return Navigate(WideToUTF8(file_url));
  }

  bool NavigateRelative(const std::wstring& relative_url) {
    return Navigate(WideToUTF8(mock_server_.Resolve(relative_url.c_str())));
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

  virtual void OnLoad(const GURL& url) {
    if (url_ == url) {
      navigation_result_ = true;
    } else {
      QuitMessageLoop();
    }
  }

  virtual void OnLoadFailed(int error_code, const std::string& url) {
    navigation_result_ = false;
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
    caller_message_loop_->PostTask(FROM_HERE, MessageLoop::QuitClosure());
  }

 private:
  testing::StrictMock<MockWebServer> mock_server_;
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
      : Base(caller_message_loop, launch_timeout, true, L"", L"", false,
             false) {
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
      : Base(caller_message_loop, launch_timeout, true, L"", L"", false,
             false) {
  }
  virtual void OnLoad(const GURL& url) {
    Base::OnLoad(url);
    QuitMessageLoop();
  }
};

class AutomationMockPostMessage
    : public AutomationMockDelegate<AutomationMockPostMessage> {
 public:
  typedef AutomationMockDelegate<AutomationMockPostMessage> Base;
  AutomationMockPostMessage(MessageLoop* caller_message_loop,
                            int launch_timeout)
      : Base(caller_message_loop, launch_timeout, true, L"", L"", false,
             false),
        postmessage_result_(false) {}
  bool postmessage_result() const {
    return postmessage_result_;
  }
  virtual void OnLoad(const GURL& url) {
    Base::OnLoad(url);
    if (navigation_result()) {
      automation()->ForwardMessageFromExternalHost("Test", "null", "*");
    }
  }
  virtual void OnMessageFromChromeFrame(const std::string& message,
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
      : Base(caller_message_loop, launch_timeout, true, L"", L"", false,
             false),
        request_start_result_(false) {
    if (automation()) {
      automation()->set_use_chrome_network(false);
    }
  }
  bool request_start_result() const {
    return request_start_result_;
  }
  virtual void OnRequestStart(int request_id,
                              const AutomationURLRequest& request) {
    request_start_result_ = true;
    QuitMessageLoop();
  }
  virtual void OnLoad(const GURL& url) {
    Base::OnLoad(url);
  }
 private:
  bool request_start_result_;
};

#endif  // CHROME_FRAME_TEST_CHROME_FRAME_AUTOMATION_MOCK_H_
