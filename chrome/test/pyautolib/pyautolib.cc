// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_paths.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/automation_messages.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/automation/automation_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/pyautolib/pyautolib.h"
#include "googleurl/src/gurl.h"

// PyUITestSuiteBase
PyUITestSuiteBase::PyUITestSuiteBase(int argc, char** argv)
    : UITestSuite(argc, argv) {
}

PyUITestSuiteBase::~PyUITestSuiteBase() {
#if defined(OS_MACOSX)
  pool_.Recycle();
#endif
  Shutdown();
}

void PyUITestSuiteBase::InitializeWithPath(const base::FilePath& browser_dir) {
  SetBrowserDirectory(browser_dir);
  UITestSuite::Initialize();
}

void PyUITestSuiteBase::SetCrSourceRoot(const base::FilePath& path) {
  PathService::Override(base::DIR_SOURCE_ROOT, path);
}

// PyUITestBase
PyUITestBase::PyUITestBase(bool clear_profile, std::wstring homepage)
    : UITestBase() {
  set_clear_profile(clear_profile);
  set_homepage(base::WideToASCII(homepage));
  // We add this so that pyauto can execute javascript in the renderer and
  // read values back.
  dom_automation_enabled_ = true;
  message_loop_ = GetSharedMessageLoop(MessageLoop::TYPE_DEFAULT);
}

PyUITestBase::~PyUITestBase() {
}

// static, refer .h for why it needs to be static
MessageLoop* PyUITestBase::message_loop_ = NULL;

// static
MessageLoop* PyUITestBase::GetSharedMessageLoop(
  MessageLoop::Type msg_loop_type) {
  if (!message_loop_)   // Create a shared instance of MessageLoop
    message_loop_ = new MessageLoop(msg_loop_type);
  return message_loop_;
}

void PyUITestBase::Initialize(const base::FilePath& browser_dir) {
  UITestBase::SetBrowserDirectory(browser_dir);
}

ProxyLauncher* PyUITestBase::CreateProxyLauncher() {
  if (named_channel_id_.empty())
    return new AnonymousProxyLauncher(false);
  return new NamedProxyLauncher(named_channel_id_, false, false);
}

void PyUITestBase::SetUp() {
  UITestBase::SetUp();
}

void PyUITestBase::TearDown() {
  UITestBase::TearDown();
}

void PyUITestBase::SetLaunchSwitches() {
  // Clear the homepage because some of the pyauto tests don't work correctly
  // if a URL argument is passed.
  std::string homepage_original;
  std::swap(homepage_original, homepage_);

  UITestBase::SetLaunchSwitches();

  // However, we *do* want the --homepage switch.
  std::swap(homepage_original, homepage_);
  launch_arguments_.AppendSwitchASCII(switches::kHomePage, homepage_);
}

AutomationProxy* PyUITestBase::automation() const {
  AutomationProxy* automation_proxy = UITestBase::automation();
  if (!automation_proxy) {
    LOG(FATAL) << "The automation proxy is NULL.";
  }
  return automation_proxy;
}

std::string PyUITestBase::_SendJSONRequest(int window_index,
                                           const std::string& request,
                                           int timeout) {
  std::string response;
  bool success;
  AutomationProxy* automation_sender = automation();
  base::TimeTicks time = base::TimeTicks::Now();

  if (!automation_sender) {
    ErrorResponse("Automation proxy does not exist", request, false, &response);
  } else if (!automation_sender->channel()) {
    ErrorResponse("Chrome automation IPC channel was found already broken",
                  request, false, &response);
  } else if (!automation_sender->Send(
      new AutomationMsg_SendJSONRequest(window_index, request, &response,
                                        &success),
      timeout)) {
    RequestFailureResponse(request, base::TimeTicks::Now() - time,
                           base::TimeDelta::FromMilliseconds(timeout),
                           &response);
  }
  return response;
}

void PyUITestBase::ErrorResponse(
    const std::string& error_string,
    const std::string& request,
    bool is_timeout,
    std::string* response) {
  base::DictionaryValue error_dict;
  std::string error_msg = StringPrintf("%s for %s", error_string.c_str(),
                                       request.c_str());
  LOG(ERROR) << "Error during automation: " << error_msg;
  error_dict.SetString("error", error_msg);
  error_dict.SetBoolean("is_interface_error", true);
  error_dict.SetBoolean("is_interface_timeout", is_timeout);
  base::JSONWriter::Write(&error_dict, response);
}

void PyUITestBase::RequestFailureResponse(
    const std::string& request,
    const base::TimeDelta& duration,
    const base::TimeDelta& timeout,
    std::string* response) {
  // TODO(craigdh): Determine timeout directly from IPC's Send().
  if (duration >= timeout) {
    ErrorResponse(
        StringPrintf("Chrome automation timed out after %d seconds",
                     static_cast<int>(duration.InSeconds())),
        request, true, response);
  } else {
    // TODO(craigdh): Determine specific cause.
    ErrorResponse(
        "Chrome automation failed prior to timing out, did chrome crash?",
        request, false, response);
  }
}
