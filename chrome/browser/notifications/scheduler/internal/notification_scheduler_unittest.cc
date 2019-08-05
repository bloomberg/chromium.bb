// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/notification_scheduler.h"

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/notifications/scheduler/internal/notification_scheduler_context.h"
#include "chrome/browser/notifications/scheduler/internal/scheduler_config.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_client_registrar.h"
#include "chrome/browser/notifications/scheduler/test/mock_background_task_coordinator.h"
#include "chrome/browser/notifications/scheduler/test/mock_display_agent.h"
#include "chrome/browser/notifications/scheduler/test/mock_display_decider.h"
#include "chrome/browser/notifications/scheduler/test/mock_impression_history_tracker.h"
#include "chrome/browser/notifications/scheduler/test/mock_notification_background_task_scheduler.h"
#include "chrome/browser/notifications/scheduler/test/mock_notification_scheduler_client.h"
#include "chrome/browser/notifications/scheduler/test/mock_scheduled_notification_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::InvokeWithoutArgs;
using ::testing::NiceMock;

namespace notifications {
namespace {

class NotificationSchedulerTest : public testing::Test {
 public:
  NotificationSchedulerTest()
      : registrar_(nullptr),
        impression_tracker_(nullptr),
        notification_manager_(nullptr),
        client_(nullptr) {}
  ~NotificationSchedulerTest() override = default;

  void SetUp() override {
    auto registrar = std::make_unique<NotificationSchedulerClientRegistrar>();
    auto task_coordinator =
        std::make_unique<test::MockBackgroundTaskCoordinator>();
    auto impression_tracker =
        std::make_unique<NiceMock<test::MockImpressionHistoryTracker>>();
    auto notification_manager =
        std::make_unique<NiceMock<test::MockScheduledNotificationManager>>();
    auto display_agent = std::make_unique<test::MockDisplayAgent>();
    auto display_decider = std::make_unique<test::MockDisplayDecider>();
    auto config = SchedulerConfig::Create();

    registrar_ = registrar.get();
    impression_tracker_ = impression_tracker.get();
    notification_manager_ = notification_manager.get();

    // Register mock clients.
    auto client = std::make_unique<test::MockNotificationSchedulerClient>();
    client_ = client.get();
    registrar_->RegisterClient(SchedulerClientType::kTest1, std::move(client));

    auto context = std::make_unique<NotificationSchedulerContext>(
        std::move(registrar), std::move(task_coordinator),
        std::move(impression_tracker), std::move(notification_manager),
        std::move(display_agent), std::move(display_decider),
        std::move(config));
    notification_scheduler_ = NotificationScheduler::Create(std::move(context));
  }

 protected:
  NotificationScheduler* scheduler() { return notification_scheduler_.get(); }

  test::MockImpressionHistoryTracker* impression_tracker() {
    return impression_tracker_;
  }

  test::MockScheduledNotificationManager* notification_manager() {
    return notification_manager_;
  }

  test::MockNotificationSchedulerClient* client() { return client_; }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  NotificationSchedulerClientRegistrar* registrar_;
  test::MockImpressionHistoryTracker* impression_tracker_;
  test::MockScheduledNotificationManager* notification_manager_;
  test::MockNotificationSchedulerClient* client_;

  std::unique_ptr<NotificationScheduler> notification_scheduler_;
  DISALLOW_COPY_AND_ASSIGN(NotificationSchedulerTest);
};

// Tests successful initialization flow.
TEST_F(NotificationSchedulerTest, InitSuccess) {
  EXPECT_CALL(*impression_tracker(), Init(_, _))
      .WillOnce(Invoke([](ImpressionHistoryTracker::Delegate* delegate,
                          ImpressionHistoryTracker::InitCallback callback) {
        std::move(callback).Run(true);
      }));

  EXPECT_CALL(*notification_manager(), Init(_, _))
      .WillOnce(Invoke([](ScheduledNotificationManager::Delegate* delegate,
                          ScheduledNotificationManager::InitCallback callback) {
        std::move(callback).Run(true);
      }));

  base::RunLoop run_loop;
  scheduler()->Init(base::BindOnce([](bool success) { EXPECT_TRUE(success); }));

  EXPECT_CALL(*client(), OnSchedulerInitialized(true, _))
      .WillOnce(InvokeWithoutArgs([&]() { run_loop.Quit(); }));

  run_loop.Run();
}

// Tests the case when impression tracker failed to initialize.
TEST_F(NotificationSchedulerTest, InitImpressionTrackerFailed) {
  EXPECT_CALL(*impression_tracker(), Init(_, _))
      .WillOnce(Invoke([](ImpressionHistoryTracker::Delegate* delegate,
                          ImpressionHistoryTracker::InitCallback callback) {
        // Impression tracker failed to load.
        std::move(callback).Run(false);
      }));

  EXPECT_CALL(*notification_manager(), Init(_, _)).Times(0);

  base::RunLoop run_loop;
  scheduler()->Init(
      base::BindOnce([](bool success) { EXPECT_FALSE(success); }));

  EXPECT_CALL(*client(), OnSchedulerInitialized(false, _))
      .WillOnce(InvokeWithoutArgs([&]() { run_loop.Quit(); }));

  run_loop.Run();
}

// Tests the case when scheduled notification manager failed to initialize.
TEST_F(NotificationSchedulerTest, InitScheduledNotificationManagerFailed) {
  EXPECT_CALL(*impression_tracker(), Init(_, _))
      .WillOnce(Invoke([](ImpressionHistoryTracker::Delegate* delegate,
                          ImpressionHistoryTracker::InitCallback callback) {
        std::move(callback).Run(true);
      }));

  EXPECT_CALL(*notification_manager(), Init(_, _))
      .WillOnce(Invoke([](ScheduledNotificationManager::Delegate* delegate,
                          ScheduledNotificationManager::InitCallback callback) {
        // Scheduled notification manager failed to load.
        std::move(callback).Run(false);
      }));

  base::RunLoop run_loop;
  scheduler()->Init(
      base::BindOnce([](bool success) { EXPECT_FALSE(success); }));

  EXPECT_CALL(*client(), OnSchedulerInitialized(false, _))
      .WillOnce(InvokeWithoutArgs([&]() { run_loop.Quit(); }));

  run_loop.Run();
}

}  // namespace
}  // namespace notifications
