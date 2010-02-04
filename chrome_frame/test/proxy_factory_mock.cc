// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome_frame/test/proxy_factory_mock.h"

#define GMOCK_MUTANT_INCLUDE_LATE_OBJECT_BINDING
#include "testing/gmock_mutant.h"

using testing::CreateFunctor;
using testing::_;

template <> struct RunnableMethodTraits<MockProxyFactory> {
  void RetainCallee(MockProxyFactory* obj) {}
  void ReleaseCallee(MockProxyFactory* obj) {}
};

TEST(ProxyFactoryTest, CreateDestroy) {
  ProxyFactory f;
  LaunchDelegateMock d;
  EXPECT_CALL(d, LaunchComplete(testing::NotNull(), testing::_)).Times(1);

  ChromeFrameLaunchParams params;
  params.automation_server_launch_timeout = 0;
  params.profile_name = L"Adam.N.Epilinter";
  params.extra_chrome_arguments = L"";
  params.perform_version_check = false;
  params.incognito_mode = false;

  void* id = NULL;
  f.GetAutomationServer(&d, params, &id);
  f.ReleaseAutomationServer(id);
}

TEST(ProxyFactoryTest, CreateSameProfile) {
  ProxyFactory f;
  LaunchDelegateMock d;
  EXPECT_CALL(d, LaunchComplete(testing::NotNull(), testing::_)).Times(2);

  ChromeFrameLaunchParams params;
  params.automation_server_launch_timeout = 0;
  params.profile_name = L"Dr. Gratiano Forbeson";
  params.extra_chrome_arguments = L"";
  params.perform_version_check = false;
  params.incognito_mode = false;

  void* i1 = NULL;
  void* i2 = NULL;

  f.GetAutomationServer(&d, params, &i1);
  f.GetAutomationServer(&d, params, &i2);

  EXPECT_EQ(i1, i2);
  f.ReleaseAutomationServer(i2);
  f.ReleaseAutomationServer(i1);
}

TEST(ProxyFactoryTest, CreateDifferentProfiles) {
  ProxyFactory f;
  LaunchDelegateMock d;
  EXPECT_CALL(d, LaunchComplete(testing::NotNull(), testing::_)).Times(2);

  ChromeFrameLaunchParams params1;
  params1.automation_server_launch_timeout = 0;
  params1.profile_name = L"Adam.N.Epilinter";
  params1.extra_chrome_arguments = L"";
  params1.perform_version_check = false;
  params1.incognito_mode = false;

  ChromeFrameLaunchParams params2;
  params2.automation_server_launch_timeout = 0;
  params2.profile_name = L"Dr. Gratiano Forbeson";
  params2.extra_chrome_arguments = L"";
  params2.perform_version_check = false;
  params2.incognito_mode = false;

  void* i1 = NULL;
  void* i2 = NULL;

  f.GetAutomationServer(&d, params1, &i1);
  f.GetAutomationServer(&d, params2, &i2);

  EXPECT_NE(i1, i2);
  f.ReleaseAutomationServer(i2);
  f.ReleaseAutomationServer(i1);
}

