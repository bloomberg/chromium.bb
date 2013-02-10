// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome_frame/crash_reporting/crash_metrics.h"
#include "chrome_frame/test/chrome_frame_test_utils.h"
#include "chrome_frame/test/proxy_factory_mock.h"
#include "chrome_frame/test/test_scrubber.h"
#include "chrome_frame/utils.h"

#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

using testing::CreateFunctor;
using testing::_;

class ProxyFactoryTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE;

  ChromeFrameLaunchParams* MakeLaunchParams(const wchar_t* profile_name);

  ProxyFactory proxy_factory_;
  LaunchDelegateMock launch_delegate_mock_;
};

void ProxyFactoryTest::SetUp() {
  CrashMetricsReporter::GetInstance()->set_active(true);
}

ChromeFrameLaunchParams* ProxyFactoryTest::MakeLaunchParams(
    const wchar_t* profile_name) {
  GURL empty;
  base::FilePath profile_path;
  GetChromeFrameProfilePath(profile_name, &profile_path);
  chrome_frame_test::OverrideDataDirectoryForThisTest(profile_path.value());
  ChromeFrameLaunchParams* params =
      new ChromeFrameLaunchParams(empty, empty, profile_path,
                                  profile_path.BaseName().value(), L"", false,
                                  false, false, false);
  params->set_launch_timeout(0);
  params->set_version_check(false);
  return params;
}

TEST_F(ProxyFactoryTest, CreateDestroy) {
  EXPECT_CALL(launch_delegate_mock_,
              LaunchComplete(testing::NotNull(), testing::_)).Times(1);

  scoped_refptr<ChromeFrameLaunchParams> params(
      MakeLaunchParams(L"Adam.N.Epilinter"));

  void* id = NULL;
  proxy_factory_.GetAutomationServer(&launch_delegate_mock_, params, &id);
  proxy_factory_.ReleaseAutomationServer(id, &launch_delegate_mock_);
}

TEST_F(ProxyFactoryTest, CreateSameProfile) {
  LaunchDelegateMock d2;
  EXPECT_CALL(launch_delegate_mock_,
              LaunchComplete(testing::NotNull(), testing::_)).Times(1);
  EXPECT_CALL(d2, LaunchComplete(testing::NotNull(), testing::_)).Times(1);

  scoped_refptr<ChromeFrameLaunchParams> params(
      MakeLaunchParams(L"Dr. Gratiano Forbeson"));

  void* i1 = NULL;
  void* i2 = NULL;

  proxy_factory_.GetAutomationServer(&launch_delegate_mock_, params, &i1);
  proxy_factory_.GetAutomationServer(&d2, params, &i2);

  EXPECT_EQ(i1, i2);
  proxy_factory_.ReleaseAutomationServer(i2, &d2);
  proxy_factory_.ReleaseAutomationServer(i1, &launch_delegate_mock_);
}

TEST_F(ProxyFactoryTest, CreateDifferentProfiles) {
  LaunchDelegateMock d2;

  EXPECT_CALL(launch_delegate_mock_,
              LaunchComplete(testing::NotNull(), testing::_));
  EXPECT_CALL(d2, LaunchComplete(testing::NotNull(), testing::_));

  scoped_refptr<ChromeFrameLaunchParams> params1(
      MakeLaunchParams(L"Adam.N.Epilinter"));
  scoped_refptr<ChromeFrameLaunchParams> params2(
      MakeLaunchParams(L"Dr. Gratiano Forbeson"));

  void* i1 = NULL;
  void* i2 = NULL;

  proxy_factory_.GetAutomationServer(&launch_delegate_mock_, params1, &i1);
  proxy_factory_.GetAutomationServer(&d2, params2, &i2);

  EXPECT_NE(i1, i2);
  proxy_factory_.ReleaseAutomationServer(i2, &d2);
  proxy_factory_.ReleaseAutomationServer(i1, &launch_delegate_mock_);
}

// This test has been disabled because it crashes randomly on the builders.
// http://code.google.com/p/chromium/issues/detail?id=81039
TEST_F(ProxyFactoryTest, DISABLED_FastCreateDestroy) {
  LaunchDelegateMock* d1 = &launch_delegate_mock_;
  LaunchDelegateMock* d2 = new LaunchDelegateMock();

  scoped_refptr<ChromeFrameLaunchParams> params(
      MakeLaunchParams(L"Dr. Gratiano Forbeson"));
  params->set_launch_timeout(10000);

  void* i1 = NULL;
  base::WaitableEvent launched(true, false);
  EXPECT_CALL(*d1, LaunchComplete(testing::NotNull(), AUTOMATION_SUCCESS))
      .WillOnce(testing::InvokeWithoutArgs(&launched,
                                           &base::WaitableEvent::Signal));
  proxy_factory_.GetAutomationServer(d1, params, &i1);
  // Wait for launch
  ASSERT_TRUE(launched.TimedWait(base::TimeDelta::FromSeconds(10)));

  // Expect second launch to succeed too
  EXPECT_CALL(*d2, LaunchComplete(testing::NotNull(), AUTOMATION_SUCCESS))
      .Times(1);

  // Boost thread priority so we call ReleaseAutomationServer before
  // LaunchComplete callback have a chance to be executed.
  ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
  void* i2 = NULL;
  proxy_factory_.GetAutomationServer(d2, params, &i2);
  EXPECT_EQ(i1, i2);
  proxy_factory_.ReleaseAutomationServer(i2, d2);
  delete d2;

  ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_NORMAL);
  proxy_factory_.ReleaseAutomationServer(i1, d1);
}
