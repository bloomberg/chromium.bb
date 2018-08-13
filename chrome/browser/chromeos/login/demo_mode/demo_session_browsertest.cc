// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"

#include "base/macros.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/login_manager_test.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"

namespace chromeos {

namespace {

constexpr char kFakeDeviceId[] = "device_id";
constexpr char kNonDemoDomain[] = "non-demo-mode.com";

}  // namespace

class DemoSessionDemoEnrolledDeviceTest : public LoginManagerTest {
 public:
  DemoSessionDemoEnrolledDeviceTest()
      : LoginManagerTest(true /*should_launch_browser*/),
        install_attributes_(StubInstallAttributes::CreateCloudManaged(
            DemoSetupController::kDemoModeDomain,
            kFakeDeviceId)) {}
  ~DemoSessionDemoEnrolledDeviceTest() override = default;

 private:
  const ScopedStubInstallAttributes install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(DemoSessionDemoEnrolledDeviceTest);
};

IN_PROC_BROWSER_TEST_F(DemoSessionDemoEnrolledDeviceTest, IsDemoMode) {
  EXPECT_TRUE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::EnrollmentType::kOnline,
            DemoSession::GetEnrollmentType());
}

class DemoSessionNonDemoEnrolledDeviceTest : public LoginManagerTest {
 public:
  DemoSessionNonDemoEnrolledDeviceTest()
      : LoginManagerTest(true /*should_launch_browser*/),
        install_attributes_(
            StubInstallAttributes::CreateCloudManaged(kNonDemoDomain,
                                                      kFakeDeviceId)) {}
  ~DemoSessionNonDemoEnrolledDeviceTest() override = default;

 private:
  ScopedStubInstallAttributes install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(DemoSessionNonDemoEnrolledDeviceTest);
};

IN_PROC_BROWSER_TEST_F(DemoSessionNonDemoEnrolledDeviceTest, NotDemoMode) {
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::EnrollmentType::kUnenrolled,
            DemoSession::GetEnrollmentType());
}

class DemoSessionConsumerDeviceTest : public LoginManagerTest {
 public:
  DemoSessionConsumerDeviceTest()
      : LoginManagerTest(true /*should_launch_browser*/),
        install_attributes_(StubInstallAttributes::CreateConsumerOwned()) {}
  ~DemoSessionConsumerDeviceTest() override = default;

 private:
  ScopedStubInstallAttributes install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(DemoSessionConsumerDeviceTest);
};

IN_PROC_BROWSER_TEST_F(DemoSessionConsumerDeviceTest, NotDemoMode) {
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::EnrollmentType::kUnenrolled,
            DemoSession::GetEnrollmentType());
}

class DemoSessionUnownedDeviceTest : public LoginManagerTest {
 public:
  DemoSessionUnownedDeviceTest()
      : LoginManagerTest(true /*should_launch_browser*/),
        install_attributes_(StubInstallAttributes::CreateUnset()) {}
  ~DemoSessionUnownedDeviceTest() override = default;

 private:
  ScopedStubInstallAttributes install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(DemoSessionUnownedDeviceTest);
};

IN_PROC_BROWSER_TEST_F(DemoSessionUnownedDeviceTest, NotDemoMode) {
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::EnrollmentType::kUnenrolled,
            DemoSession::GetEnrollmentType());
}

class DemoSessionActiveDirectoryDeviceTest : public LoginManagerTest {
 public:
  DemoSessionActiveDirectoryDeviceTest()
      : LoginManagerTest(true /*should_launch_browser*/),
        install_attributes_(StubInstallAttributes::CreateActiveDirectoryManaged(
            DemoSetupController::kDemoModeDomain,
            kFakeDeviceId)) {}
  ~DemoSessionActiveDirectoryDeviceTest() override = default;

 private:
  ScopedStubInstallAttributes install_attributes_;

  DISALLOW_COPY_AND_ASSIGN(DemoSessionActiveDirectoryDeviceTest);
};

IN_PROC_BROWSER_TEST_F(DemoSessionActiveDirectoryDeviceTest, NotDemoMode) {
  EXPECT_FALSE(DemoSession::IsDeviceInDemoMode());
  EXPECT_EQ(DemoSession::EnrollmentType::kUnenrolled,
            DemoSession::GetEnrollmentType());
}

}  // namespace chromeos
