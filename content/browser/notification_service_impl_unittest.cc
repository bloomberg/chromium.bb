// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/notification_service_impl.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Bogus class to act as a NotificationSource for the messages.
class TestSource {};

class TestObserver : public content::NotificationObserver {
public:
  TestObserver() : notification_count_(0) {}

  int notification_count() { return notification_count_; }

  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) {
    ++notification_count_;
  }

private:
  int notification_count_;
};

}  // namespace


class NotificationServiceImplTest : public testing::Test {
 protected:
  content::NotificationRegistrar registrar_;
};

TEST_F(NotificationServiceImplTest, Basic) {
  TestSource test_source;
  TestSource other_source;

  // Check the equality operators defined for NotificationSource
  EXPECT_TRUE(content::Source<TestSource>(&test_source) ==
              content::Source<TestSource>(&test_source));
  EXPECT_TRUE(content::Source<TestSource>(&test_source) !=
              content::Source<TestSource>(&other_source));

  TestObserver all_types_all_sources;
  TestObserver idle_all_sources;
  TestObserver all_types_test_source;
  TestObserver idle_test_source;

  // Make sure it doesn't freak out when there are no observers.
  content::NotificationService* service =
      content::NotificationService::current();
  service->Notify(content::NOTIFICATION_IDLE,
                  content::Source<TestSource>(&test_source),
                  content::NotificationService::NoDetails());

  registrar_.Add(&all_types_all_sources, content::NOTIFICATION_ALL,
                 content::NotificationService::AllSources());
  registrar_.Add(&idle_all_sources, content::NOTIFICATION_IDLE,
                 content::NotificationService::AllSources());
  registrar_.Add(&all_types_test_source, content::NOTIFICATION_ALL,
                 content::Source<TestSource>(&test_source));
  registrar_.Add(&idle_test_source, content::NOTIFICATION_IDLE,
                 content::Source<TestSource>(&test_source));

  EXPECT_EQ(0, all_types_all_sources.notification_count());
  EXPECT_EQ(0, idle_all_sources.notification_count());
  EXPECT_EQ(0, all_types_test_source.notification_count());
  EXPECT_EQ(0, idle_test_source.notification_count());

  service->Notify(content::NOTIFICATION_IDLE,
                  content::Source<TestSource>(&test_source),
                  content::NotificationService::NoDetails());

  EXPECT_EQ(1, all_types_all_sources.notification_count());
  EXPECT_EQ(1, idle_all_sources.notification_count());
  EXPECT_EQ(1, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->Notify(content::NOTIFICATION_BUSY,
                  content::Source<TestSource>(&test_source),
                  content::NotificationService::NoDetails());

  EXPECT_EQ(2, all_types_all_sources.notification_count());
  EXPECT_EQ(1, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->Notify(content::NOTIFICATION_IDLE,
                  content::Source<TestSource>(&other_source),
                  content::NotificationService::NoDetails());

  EXPECT_EQ(3, all_types_all_sources.notification_count());
  EXPECT_EQ(2, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  service->Notify(content::NOTIFICATION_BUSY,
                  content::Source<TestSource>(&other_source),
                  content::NotificationService::NoDetails());

  EXPECT_EQ(4, all_types_all_sources.notification_count());
  EXPECT_EQ(2, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  // Try send with NULL source.
  service->Notify(content::NOTIFICATION_IDLE,
                  content::NotificationService::AllSources(),
                  content::NotificationService::NoDetails());

  EXPECT_EQ(5, all_types_all_sources.notification_count());
  EXPECT_EQ(3, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());

  registrar_.RemoveAll();

  service->Notify(content::NOTIFICATION_IDLE,
                  content::Source<TestSource>(&test_source),
                  content::NotificationService::NoDetails());

  EXPECT_EQ(5, all_types_all_sources.notification_count());
  EXPECT_EQ(3, idle_all_sources.notification_count());
  EXPECT_EQ(2, all_types_test_source.notification_count());
  EXPECT_EQ(1, idle_test_source.notification_count());
}

TEST_F(NotificationServiceImplTest, MultipleRegistration) {
  TestSource test_source;

  TestObserver idle_test_source;

  content::NotificationService* service =
      content::NotificationService::current();

  registrar_.Add(&idle_test_source, content::NOTIFICATION_IDLE,
                 content::Source<TestSource>(&test_source));
  registrar_.Add(&idle_test_source, content::NOTIFICATION_ALL,
                 content::Source<TestSource>(&test_source));

  service->Notify(content::NOTIFICATION_IDLE,
                  content::Source<TestSource>(&test_source),
                  content::NotificationService::NoDetails());
  EXPECT_EQ(2, idle_test_source.notification_count());

  registrar_.Remove(&idle_test_source, content::NOTIFICATION_IDLE,
                    content::Source<TestSource>(&test_source));

  service->Notify(content::NOTIFICATION_IDLE,
                 content::Source<TestSource>(&test_source),
                 content::NotificationService::NoDetails());
  EXPECT_EQ(3, idle_test_source.notification_count());

  registrar_.Remove(&idle_test_source, content::NOTIFICATION_ALL,
                    content::Source<TestSource>(&test_source));

  service->Notify(content::NOTIFICATION_IDLE,
                  content::Source<TestSource>(&test_source),
                  content::NotificationService::NoDetails());
  EXPECT_EQ(3, idle_test_source.notification_count());
}
