// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/multi_process_notification.h"

#include "base/basictypes.h"
#include "base/environment.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_MACOSX)
// TODO(dmaclach): Remove defined(OS_MACOSX) once
//                 MultiProcessNotification is implemented on Win/Linux.

namespace {

const char kStartedNotificationName[] = "MultiProcessTestStartedNotification";
const char kQuitNotificationName[] = "MultiProcessTestQuitNotification";

void SpinRunLoop(int milliseconds) {
  MessageLoopForIO *loop = MessageLoopForIO::current();

  // Post a quit task so that this loop eventually ends and we don't hang
  // in the case of a bad test. Usually, the run loop will quit sooner than
  // that because all tests use a MultiProcessNotificationTestQuit which quits
  // the current run loop when it gets a notification.
  loop->PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask(), milliseconds);
  loop->Run();
}

int MultiProcessNotificationMain(multi_process_notification::Domain domain) {
  MessageLoop io_loop(MessageLoop::TYPE_IO);
  multi_process_notification::PerformTaskOnNotification quitter(
      new MessageLoop::QuitTask());
  multi_process_notification::Listener listener(
      kQuitNotificationName, domain, &quitter);
  EXPECT_TRUE(listener.Start());
  EXPECT_TRUE(multi_process_notification::Post(kStartedNotificationName,
                                               domain));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  EXPECT_TRUE(quitter.WasNotificationReceived());
  return 0;
}

}  // namespace

class MultiProcessNotificationTest : public base::MultiProcessTest {
 public:
  MultiProcessNotificationTest();

  void PostNotificationTest(multi_process_notification::Domain domain);
  void CrossPostNotificationTest(multi_process_notification::Domain domain);

 private:
  MessageLoop io_loop_;
};

MultiProcessNotificationTest::MultiProcessNotificationTest()
    : io_loop_(MessageLoop::TYPE_IO) {
}

void MultiProcessNotificationTest::PostNotificationTest(
    multi_process_notification::Domain domain) {
  multi_process_notification::PerformTaskOnNotification process_started(
      new MessageLoop::QuitTask());
  multi_process_notification::Listener listener(kStartedNotificationName,
                                                domain,
                                                &process_started);
  ASSERT_TRUE(listener.Start());
  std::string process_name;
  switch (domain) {
    case multi_process_notification::ProfileDomain:
      process_name = "MultiProcessProfileNotificationMain";
      break;

    case multi_process_notification::UserDomain:
      process_name = "MultiProcessUserNotificationMain";
      break;

    case multi_process_notification::SystemDomain:
      process_name = "MultiProcessSystemNotificationMain";
      break;
  }
  base::ProcessHandle handle = SpawnChild(process_name, false);
  ASSERT_TRUE(handle);
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_TRUE(process_started.WasNotificationReceived());
  ASSERT_TRUE(multi_process_notification::Post(kQuitNotificationName, domain));
  int exit_code = 0;
  EXPECT_TRUE(base::WaitForExitCodeWithTimeout(
      handle, &exit_code, TestTimeouts::action_max_timeout_ms()));
}

void MultiProcessNotificationTest::CrossPostNotificationTest(
    multi_process_notification::Domain domain) {
  // Check to make sure notifications sent to user domain aren't picked up
  // by system domain listeners and vice versa.
  std::string local_notification("QuitLocalNotification");
  std::string final_notification("FinalQuitLocalNotification");
  multi_process_notification::PerformTaskOnNotification profile_quitter(
      new MessageLoop::QuitTask());
  multi_process_notification::PerformTaskOnNotification user_quitter(
      new MessageLoop::QuitTask());
  multi_process_notification::PerformTaskOnNotification system_quitter(
      new MessageLoop::QuitTask());
  multi_process_notification::PerformTaskOnNotification final_quitter(
      new MessageLoop::QuitTask());
  multi_process_notification::Listener profile_listener(
      local_notification, multi_process_notification::ProfileDomain,
      &profile_quitter);
  multi_process_notification::Listener user_listener(
      local_notification, multi_process_notification::UserDomain,
      &user_quitter);
  multi_process_notification::Listener system_listener(
      local_notification, multi_process_notification::SystemDomain,
      &system_quitter);
  multi_process_notification::Listener final_listener(
      final_notification, multi_process_notification::UserDomain,
      &final_quitter);

  ASSERT_TRUE(profile_listener.Start());
  ASSERT_TRUE(user_listener.Start());
  ASSERT_TRUE(system_listener.Start());
  ASSERT_TRUE(multi_process_notification::Post(local_notification, domain));
  SpinRunLoop(TestTimeouts::action_timeout_ms());

  // Now send out a final_notification to queue up a notification
  // after the local_notification and make sure that all listeners have had a
  // chance to process local_notification before we check to see if they
  // were called.
  ASSERT_TRUE(final_listener.Start());
  ASSERT_TRUE(multi_process_notification::Post(
      final_notification, multi_process_notification::UserDomain));
  SpinRunLoop(TestTimeouts::action_timeout_ms());
  ASSERT_TRUE(final_quitter.WasNotificationReceived());
  switch (domain) {
    case multi_process_notification::ProfileDomain:
      ASSERT_TRUE(profile_quitter.WasNotificationReceived());
      ASSERT_FALSE(user_quitter.WasNotificationReceived());
      ASSERT_FALSE(system_quitter.WasNotificationReceived());
      break;

    case multi_process_notification::UserDomain:
      ASSERT_FALSE(profile_quitter.WasNotificationReceived());
      ASSERT_TRUE(user_quitter.WasNotificationReceived());
      ASSERT_FALSE(system_quitter.WasNotificationReceived());
      break;

    case multi_process_notification::SystemDomain:
      ASSERT_FALSE(profile_quitter.WasNotificationReceived());
      ASSERT_FALSE(user_quitter.WasNotificationReceived());
      ASSERT_TRUE(system_quitter.WasNotificationReceived());
      break;
  }
}

TEST_F(MultiProcessNotificationTest, BasicCreationTest) {
  multi_process_notification::Listener local_listener(
      "BasicCreationTest", multi_process_notification::UserDomain, NULL);
  ASSERT_TRUE(local_listener.Start());
  multi_process_notification::Listener system_listener(
      "BasicCreationTest", multi_process_notification::SystemDomain, NULL);
  ASSERT_TRUE(system_listener.Start());
}

TEST_F(MultiProcessNotificationTest, PostInProcessNotification) {
  std::string local_notification("QuitLocalNotification");
  multi_process_notification::PerformTaskOnNotification quitter(
      new MessageLoop::QuitTask());
  multi_process_notification::Listener listener(
      local_notification, multi_process_notification::UserDomain, &quitter);

  ASSERT_TRUE(listener.Start());
  ASSERT_TRUE(multi_process_notification::Post(
      local_notification, multi_process_notification::UserDomain));
  SpinRunLoop(TestTimeouts::action_max_timeout_ms());
  ASSERT_TRUE(quitter.WasNotificationReceived());
}

TEST_F(MultiProcessNotificationTest, PostProfileNotification) {
  PostNotificationTest(multi_process_notification::ProfileDomain);
}

TEST_F(MultiProcessNotificationTest, PostUserNotification) {
  PostNotificationTest(multi_process_notification::UserDomain);
}

TEST_F(MultiProcessNotificationTest, PostSystemNotification) {
  PostNotificationTest(multi_process_notification::SystemDomain);
}

TEST_F(MultiProcessNotificationTest, ProfileCrossDomainPosting) {
  CrossPostNotificationTest(multi_process_notification::ProfileDomain);
}

TEST_F(MultiProcessNotificationTest, UserCrossDomainPosting) {
  CrossPostNotificationTest(multi_process_notification::UserDomain);
}

TEST_F(MultiProcessNotificationTest, SystemCrossDomainPosting) {
  CrossPostNotificationTest(multi_process_notification::SystemDomain);
}

MULTIPROCESS_TEST_MAIN(MultiProcessProfileNotificationMain) {
  return MultiProcessNotificationMain(
      multi_process_notification::ProfileDomain);
}

MULTIPROCESS_TEST_MAIN(MultiProcessUserNotificationMain) {
  return MultiProcessNotificationMain(multi_process_notification::UserDomain);
}

MULTIPROCESS_TEST_MAIN(MultiProcessSystemNotificationMain) {
  return MultiProcessNotificationMain(multi_process_notification::SystemDomain);
}

#endif  // defined(OS_MACOSX)
