// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/notifications/blink_notification_service_impl.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/test/mock_resource_context.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "content/test/mock_platform_notification_service.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/modules/notifications/notification_service.mojom.h"
#include "third_party/WebKit/public/platform/modules/permissions/permission_status.mojom.h"

namespace content {

namespace {

const int kFakeRenderProcessId = 1;
const char kTestOrigin[] = "https://example.com";
const int64_t kFakeServiceWorkerRegistrationId = 1234;

class MockNonPersistentNotificationListener
    : public blink::mojom::NonPersistentNotificationListener {
 public:
  MockNonPersistentNotificationListener() : binding_(this) {}
  ~MockNonPersistentNotificationListener() override = default;

  blink::mojom::NonPersistentNotificationListenerPtr GetPtr() {
    blink::mojom::NonPersistentNotificationListenerPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  // NonPersistentNotificationListener interface.
  void OnShow() override {}
  void OnClick() override {}
  void OnClose() override {}

 private:
  mojo::Binding<blink::mojom::NonPersistentNotificationListener> binding_;
};

}  // anonymous namespace

// This is for overriding the Platform Notification Service with a mock one.
class NotificationBrowserClient : public TestContentBrowserClient {
 public:
  NotificationBrowserClient(
      MockPlatformNotificationService* mock_platform_service)
      : platform_notification_service_(mock_platform_service) {}

  PlatformNotificationService* GetPlatformNotificationService() override {
    return platform_notification_service_;
  }

 private:
  MockPlatformNotificationService* platform_notification_service_;
};

class BlinkNotificationServiceImplTest : public ::testing::Test {
 public:
  // Using REAL_IO_THREAD would give better coverage for thread safety, but
  // at time of writing EmbeddedWorkerTestHelper didn't seem to support that.
  BlinkNotificationServiceImplTest()
      : thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP),
        notification_browser_client_(&mock_platform_service_) {
    SetBrowserClientForTesting(&notification_browser_client_);
  }

  ~BlinkNotificationServiceImplTest() override = default;

  // ::testing::Test overrides.
  void SetUp() override {
    notification_context_ = new PlatformNotificationContextImpl(
        base::FilePath(), &browser_context_,
        nullptr /* service_worker_context */);
    notification_context_->Initialize();

    // Wait for notification context to be initialized to avoid TSAN detecting
    // a memory race in tests - in production the PlatformNotificationContext
    // will be initialized long before it is read from so this is fine.
    RunAllTasksUntilIdle();

    blink::mojom::NotificationServicePtr notification_service_ptr;
    notification_service_ = std::make_unique<BlinkNotificationServiceImpl>(
        notification_context_.get(), &browser_context_, &resource_context_,
        kFakeRenderProcessId, url::Origin::Create(GURL(kTestOrigin)),
        mojo::MakeRequest(&notification_service_ptr));
  }

  void DidGetPermissionStatus(
      blink::mojom::PermissionStatus permission_status) {
    permission_callback_result_ = permission_status;
  }

  blink::mojom::PermissionStatus GetPermissionCallbackResult() {
    return permission_callback_result_;
  }

  void DidDisplayPersistentNotification(
      base::OnceClosure quit_closure,
      blink::mojom::PersistentNotificationError error) {
    display_persistent_callback_result_ = error;
    std::move(quit_closure).Run();
  }

  void DidGetDisplayedNotifications(
      base::OnceClosure quit_closure,
      std::unique_ptr<std::set<std::string>> notification_ids,
      bool supports_synchronization) {
    get_displayed_callback_result_ = *notification_ids;
    std::move(quit_closure).Run();
  }

  void DisplayPersistentNotificationSync() {
    base::RunLoop run_loop;
    notification_service_->DisplayPersistentNotification(
        kFakeServiceWorkerRegistrationId, PlatformNotificationData(),
        NotificationResources(),
        base::BindOnce(
            &BlinkNotificationServiceImplTest::DidDisplayPersistentNotification,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Synchronous wrapper of
  // PlatformNotificationService::GetDisplayedNotifications
  std::set<std::string> GetDisplayedNotifications() {
    base::RunLoop run_loop;
    mock_platform_service_.GetDisplayedNotifications(
        &browser_context_,
        base::Bind(
            &BlinkNotificationServiceImplTest::DidGetDisplayedNotifications,
            base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
    return get_displayed_callback_result_;
  }

 protected:
  TestBrowserThreadBundle thread_bundle_;  // Must be first member.

  std::unique_ptr<BlinkNotificationServiceImpl> notification_service_;

  TestBrowserContext browser_context_;

  MockPlatformNotificationService mock_platform_service_;

  MockNonPersistentNotificationListener non_persistent_notification_listener_;

  blink::mojom::PersistentNotificationError display_persistent_callback_result_;

 private:
  NotificationBrowserClient notification_browser_client_;

  scoped_refptr<PlatformNotificationContextImpl> notification_context_;

  blink::mojom::PermissionStatus permission_callback_result_ =
      blink::mojom::PermissionStatus::ASK;

  std::set<std::string> get_displayed_callback_result_;

  MockResourceContext resource_context_;

  DISALLOW_COPY_AND_ASSIGN(BlinkNotificationServiceImplTest);
};

TEST_F(BlinkNotificationServiceImplTest, GetPermissionStatus) {
  mock_platform_service_.SetPermission(blink::mojom::PermissionStatus::GRANTED);

  notification_service_->GetPermissionStatus(
      base::BindOnce(&BlinkNotificationServiceImplTest::DidGetPermissionStatus,
                     base::Unretained(this)));

  EXPECT_EQ(blink::mojom::PermissionStatus::GRANTED,
            GetPermissionCallbackResult());

  mock_platform_service_.SetPermission(blink::mojom::PermissionStatus::DENIED);

  notification_service_->GetPermissionStatus(
      base::BindOnce(&BlinkNotificationServiceImplTest::DidGetPermissionStatus,
                     base::Unretained(this)));

  EXPECT_EQ(blink::mojom::PermissionStatus::DENIED,
            GetPermissionCallbackResult());

  mock_platform_service_.SetPermission(blink::mojom::PermissionStatus::ASK);
  notification_service_->GetPermissionStatus(
      base::BindOnce(&BlinkNotificationServiceImplTest::DidGetPermissionStatus,
                     base::Unretained(this)));

  EXPECT_EQ(blink::mojom::PermissionStatus::ASK, GetPermissionCallbackResult());
}

TEST_F(BlinkNotificationServiceImplTest,
       DisplayNonPersistentNotificationWithPermission) {
  mock_platform_service_.SetPermission(blink::mojom::PermissionStatus::GRANTED);

  notification_service_->DisplayNonPersistentNotification(
      "token", PlatformNotificationData(), NotificationResources(),
      non_persistent_notification_listener_.GetPtr());

  // TODO(https://crbug.com/787459): Pass a callback to
  // DisplayNonPersistentNotification instead of waiting for all tasks to run
  // here; a callback parameter will be needed anyway to enable
  // non-persistent notification event acknowledgements - see bug.
  RunAllTasksUntilIdle();

  EXPECT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(BlinkNotificationServiceImplTest,
       DisplayNonPersistentNotificationWithoutPermission) {
  mock_platform_service_.SetPermission(blink::mojom::PermissionStatus::DENIED);

  notification_service_->DisplayNonPersistentNotification(
      "token", PlatformNotificationData(), NotificationResources(),
      non_persistent_notification_listener_.GetPtr());

  // TODO(https://crbug.com/787459): Pass a callback to
  // DisplayNonPersistentNotification instead of waiting for all tasks to run
  // here; a callback parameter will be needed anyway to enable
  // non-persistent notification event acknowledgements - see bug.
  RunAllTasksUntilIdle();

  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(BlinkNotificationServiceImplTest,
       DisplayPersistentNotificationWithPermission) {
  mock_platform_service_.SetPermission(blink::mojom::PermissionStatus::GRANTED);

  DisplayPersistentNotificationSync();

  EXPECT_EQ(blink::mojom::PersistentNotificationError::NONE,
            display_persistent_callback_result_);

  // Wait for service to receive the Display call.
  RunAllTasksUntilIdle();

  EXPECT_EQ(1u, GetDisplayedNotifications().size());
}

TEST_F(BlinkNotificationServiceImplTest,
       DisplayPersistentNotificationWithoutPermission) {
  mock_platform_service_.SetPermission(blink::mojom::PermissionStatus::DENIED);

  DisplayPersistentNotificationSync();

  EXPECT_EQ(blink::mojom::PersistentNotificationError::PERMISSION_DENIED,
            display_persistent_callback_result_);

  // Give Service a chance to receive any unexpected Display calls.
  RunAllTasksUntilIdle();

  EXPECT_EQ(0u, GetDisplayedNotifications().size());
}

TEST_F(BlinkNotificationServiceImplTest,
       DisplayMultiplePersistentNotifications) {
  mock_platform_service_.SetPermission(blink::mojom::PermissionStatus::GRANTED);

  DisplayPersistentNotificationSync();

  DisplayPersistentNotificationSync();

  // Wait for service to receive all the Display calls.
  RunAllTasksUntilIdle();

  EXPECT_EQ(2u, GetDisplayedNotifications().size());
}
}  // namespace content
