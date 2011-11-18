// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"
#include "chrome_frame/crash_reporting/crash_metrics.h"
#include "chrome_frame/test/proxy_factory_mock.h"

#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

using testing::CreateFunctor;
using testing::_;

TEST(ProxyFactoryTest, CreateDestroy) {
  CrashMetricsReporter::GetInstance()->set_active(true);

  ProxyFactory f;
  LaunchDelegateMock d;
  EXPECT_CALL(d, LaunchComplete(testing::NotNull(), testing::_)).Times(1);

  GURL empty;
  FilePath profile_path;
  scoped_refptr<ChromeFrameLaunchParams> params(
      new ChromeFrameLaunchParams(empty, empty, profile_path,
          L"Adam.N.Epilinter", L"", L"", false, false, false));
  params->set_launch_timeout(0);
  params->set_version_check(false);

  void* id = NULL;
  f.GetAutomationServer(&d, params, &id);
  f.ReleaseAutomationServer(id, &d);
}

TEST(ProxyFactoryTest, CreateSameProfile) {
  CrashMetricsReporter::GetInstance()->set_active(true);
  ProxyFactory f;
  LaunchDelegateMock d;
  LaunchDelegateMock d2;
  EXPECT_CALL(d, LaunchComplete(testing::NotNull(), testing::_)).Times(1);
  EXPECT_CALL(d2, LaunchComplete(testing::NotNull(), testing::_)).Times(1);

  GURL empty;
  FilePath profile_path;
  scoped_refptr<ChromeFrameLaunchParams> params(
      new ChromeFrameLaunchParams(empty, empty, profile_path,
          L"Dr. Gratiano Forbeson", L"", L"", false, false, false));
  params->set_launch_timeout(0);
  params->set_version_check(false);

  void* i1 = NULL;
  void* i2 = NULL;

  f.GetAutomationServer(&d, params, &i1);
  f.GetAutomationServer(&d2, params, &i2);

  EXPECT_EQ(i1, i2);
  f.ReleaseAutomationServer(i2, &d2);
  f.ReleaseAutomationServer(i1, &d);
}

TEST(ProxyFactoryTest, CreateDifferentProfiles) {
  CrashMetricsReporter::GetInstance()->set_active(true);
  ProxyFactory f;
  LaunchDelegateMock d;
  EXPECT_CALL(d, LaunchComplete(testing::NotNull(), testing::_)).Times(2);

  GURL empty;
  FilePath profile_path;
  scoped_refptr<ChromeFrameLaunchParams> params1(
      new ChromeFrameLaunchParams(empty, empty, profile_path,
          L"Adam.N.Epilinter", L"", L"", false, false, false));
  params1->set_launch_timeout(0);
  params1->set_version_check(false);

  scoped_refptr<ChromeFrameLaunchParams> params2(
      new ChromeFrameLaunchParams(empty, empty, profile_path,
          L"Dr. Gratiano Forbeson", L"", L"", false, false, false));
  params2->set_launch_timeout(0);
  params2->set_version_check(false);

  void* i1 = NULL;
  void* i2 = NULL;

  f.GetAutomationServer(&d, params1, &i1);
  f.GetAutomationServer(&d, params2, &i2);

  EXPECT_NE(i1, i2);
  f.ReleaseAutomationServer(i2, &d);
  f.ReleaseAutomationServer(i1, &d);
}

// This test has been disabled because it crashes randomly on the builders.
// http://code.google.com/p/chromium/issues/detail?id=81039
TEST(ProxyFactoryTest, DISABLED_FastCreateDestroy) {
  CrashMetricsReporter::GetInstance()->set_active(true);
  ProxyFactory f;
  LaunchDelegateMock* d1 = new LaunchDelegateMock();
  LaunchDelegateMock* d2 = new LaunchDelegateMock();

  GURL empty;
  FilePath profile_path;
  scoped_refptr<ChromeFrameLaunchParams> params(
      new ChromeFrameLaunchParams(empty, empty, profile_path,
          L"Dr. Gratiano Forbeson", L"", L"", false, false, false));
  params->set_launch_timeout(10000);
  params->set_version_check(false);

  void* i1 = NULL;
  base::WaitableEvent launched(true, false);
  EXPECT_CALL(*d1, LaunchComplete(testing::NotNull(), AUTOMATION_SUCCESS))
      .WillOnce(testing::InvokeWithoutArgs(&launched,
                                           &base::WaitableEvent::Signal));
  f.GetAutomationServer(d1, params, &i1);
  // Wait for launch
  ASSERT_TRUE(launched.TimedWait(base::TimeDelta::FromSeconds(10)));

  // Expect second launch to succeed too
  EXPECT_CALL(*d2, LaunchComplete(testing::NotNull(), AUTOMATION_SUCCESS))
      .Times(1);

  // Boost thread priority so we call ReleaseAutomationServer before
  // LaunchComplete callback have a chance to be executed.
  ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
  void* i2 = NULL;
  f.GetAutomationServer(d2, params, &i2);
  EXPECT_EQ(i1, i2);
  f.ReleaseAutomationServer(i2, d2);
  delete d2;

  ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_NORMAL);
  f.ReleaseAutomationServer(i1, d1);
  delete d1;
}
