// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/test/automation_client_mock.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/common/automation_messages.h"
#include "chrome_frame/custom_sync_call_context.h"
#include "chrome_frame/navigation_constraints.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/test_scrubber.h"
#include "net/base/net_errors.h"

#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

using testing::_;
using testing::CreateFunctor;
using testing::Return;

namespace {

#ifndef NDEBUG
const base::TimeDelta kChromeLaunchTimeout = base::TimeDelta::FromSeconds(15);
#else
const base::TimeDelta kChromeLaunchTimeout = base::TimeDelta::FromSeconds(10);
#endif

const int kSaneAutomationTimeoutMs = 10 * 1000;

}  // namespace

MATCHER_P(LaunchParamProfileEq, profile_name, "Check for profile name") {
  return arg->profile_name().compare(profile_name) == 0;
}

void MockProxyFactory::GetServerImpl(ChromeFrameAutomationProxy* pxy,
                                     void* proxy_id,
                                     AutomationLaunchResult result,
                                     LaunchDelegate* d,
                                     ChromeFrameLaunchParams* params,
                                     void** automation_server_id) {
  *automation_server_id = proxy_id;
  loop_->PostDelayedTask(FROM_HERE,
      base::Bind(&LaunchDelegate::LaunchComplete,
                 base::Unretained(d), pxy, result),
      base::TimeDelta::FromMilliseconds(params->launch_timeout()) / 2);
}

MATCHER_P(MsgType, msg_type, "IPC::Message::type()") {
  const IPC::Message& m = arg;
  return (m.type() == msg_type);
}

MATCHER_P(EqNavigationInfoUrl, url, "IPC::NavigationInfo matcher") {
  if (url.is_valid() && url != arg.url)
    return false;
  // TODO(stevet): other members
  return true;
}

// Could be implemented as MockAutomationProxy member (we have WithArgs<>!)
ACTION_P4(HandleCreateTab, tab_handle, external_tab_container, tab_wnd,
          session_id) {
  // arg0 - message
  // arg1 - callback
  // arg2 - key
  CreateExternalTabContext::output_type input_args(tab_wnd,
                                                   external_tab_container,
                                                   tab_handle,
                                                   session_id);
  CreateExternalTabContext* context =
      reinterpret_cast<CreateExternalTabContext*>(arg1);
  DispatchToMethod(context, &CreateExternalTabContext::Completed, input_args);
  delete context;
}

ACTION_P4(InitiateNavigation, client, url, referrer, constraints) {
  client->InitiateNavigation(url, referrer, constraints);
}

// ChromeFrameAutomationClient tests that launch Chrome.
class CFACWithChrome : public testing::Test {
 protected:
  static void SetUpTestCase();
  static void TearDownTestCase();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  static base::FilePath profile_path_;
  scoped_refptr<ChromeFrameAutomationClient> client_;
  scoped_refptr<ChromeFrameLaunchParams> launch_params_;
  chrome_frame_test::TimedMsgLoop loop_;
};

// static
base::FilePath CFACWithChrome::profile_path_;

// static
void CFACWithChrome::SetUpTestCase() {
  GetChromeFrameProfilePath(L"Adam.N.Epilinter", &profile_path_);
}

// static
void CFACWithChrome::TearDownTestCase() {
  profile_path_.clear();
}

void CFACWithChrome::SetUp() {
  chrome_frame_test::OverrideDataDirectoryForThisTest(profile_path_.value());
  client_ = new ChromeFrameAutomationClient();
  GURL empty;
  launch_params_ = new ChromeFrameLaunchParams(
      empty, empty, profile_path_, profile_path_.BaseName().value(), L"",
      false, false, false);
  launch_params_->set_version_check(false);
  launch_params_->set_launch_timeout(kSaneAutomationTimeoutMs);
}

void CFACWithChrome::TearDown() {
  client_->Uninitialize();
}
